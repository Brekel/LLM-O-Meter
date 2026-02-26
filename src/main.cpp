#include "stdafx.h"
#include "AntiGravityClient.h"
#include "CredentialManager.h"
#include "TrayManager.h"
#include "UsageApiClient.h"
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QTextStream>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <QLockFile>
#include <QStandardPaths>
#endif

static std::unique_ptr<QFile>       g_logFile;
static std::unique_ptr<QTextStream> g_logStream;

static void logHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
	if(!g_logStream)
		return;
	switch(type)
	{
		case QtDebugMsg:   *g_logStream << "[D] " << msg << "\n"; break;
		case QtInfoMsg:    *g_logStream << "[I] " << msg << "\n"; break;
		case QtWarningMsg: *g_logStream << "[W] " << msg << "\n"; break;
		default:           *g_logStream << "[E] " << msg << "\n"; break;
	}
	g_logStream->flush();
}

int main(int argc, char *argv[])
{
	// Prevent multiple instances using a named mutex
#ifdef Q_OS_WIN
	HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"LLMOMeter_SingleInstance");
	if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(hMutex);
		return 0;
	}
#else
	QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/LLMOMeter.lock";
	QLockFile lockFile(lockPath);
	if(!lockFile.tryLock(100))
		return 0;
#endif

	QApplication app(argc, argv);
	app.setOrganizationName("LLMOMeter");
	app.setApplicationName("LLM-O-Meter");

	// Enable file logging with --log command-line flag
	if(app.arguments().contains("--log"))
	{
		g_logFile = std::make_unique<QFile>(QCoreApplication::applicationDirPath() + "/llm-o-meter.log");
		if(g_logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		{
			g_logStream = std::make_unique<QTextStream>(g_logFile.get());
			qInstallMessageHandler(logHandler);
		}
	}

	app.setQuitOnLastWindowClosed(false);
	app.setWindowIcon(QIcon(":/llm-o-meter.svg"));

	if(!QSystemTrayIcon::isSystemTrayAvailable())
	{
		qWarning() << "System tray is not available on this system.";
		return 1;
	}

	CredentialManager credentialManager;

	QString           initialToken = credentialManager.loadToken();
	UsageApiClient    apiClient(initialToken);
	AntiGravityClient antiGravityClient;
	TrayManager       trayManager(&apiClient, &antiGravityClient, &credentialManager);

	int pollingIntervalMs = credentialManager.loadPollingInterval() * 1000;

	if(credentialManager.loadClaudeEnabled())
	{
		if(!initialToken.isEmpty())
		{
			apiClient.fetchUsage();
			apiClient.startPolling(pollingIntervalMs);
		}
		else
		{
			qInfo() << "Claude enabled but no valid token \u2014 waiting for credentials file...";
		}
	}

	if(credentialManager.loadAntiGravityEnabled())
	{
		antiGravityClient.fetch();
		antiGravityClient.startPolling(pollingIntervalMs);
	}

	// Watch for credentials file creation/changes so we can start polling automatically
	// once the user runs Claude Code for the first time (or their token is refreshed).
	QString            claudeDir = QDir::homePath() + "/.claude";
	QString            credFile  = credentialManager.credentialsFilePath();
	QFileSystemWatcher credWatcher;

	if(QDir(claudeDir).exists())
		credWatcher.addPath(claudeDir);
	else
		credWatcher.addPath(QDir::homePath()); // watch home dir until .claude dir appears

	if(QFile::exists(credFile))
		credWatcher.addPath(credFile);

	auto onCredentialsChanged = [&]() {
		// Re-add file path in case it was replaced (write-to-temp-then-rename pattern)
		if(QFile::exists(credFile) && !credWatcher.files().contains(credFile))
			credWatcher.addPath(credFile);
		if(credentialManager.loadClaudeEnabled())
		{
			credentialManager.clearCachedToken();
			QString newToken = credentialManager.loadToken();
			if(!newToken.isEmpty())
			{
				apiClient.setToken(newToken);
				apiClient.fetchUsage();
				if(!apiClient.isPolling())
					apiClient.startPolling(pollingIntervalMs);
			}
		}
	};

	QObject::connect(&credWatcher, &QFileSystemWatcher::directoryChanged, [&](const QString &path) {
		// Upgrade watch from home dir to .claude dir once it appears
		if(path == QDir::homePath() && QDir(claudeDir).exists())
		{
			credWatcher.removePath(QDir::homePath());
			credWatcher.addPath(claudeDir);
		}
		onCredentialsChanged();
	});

	QObject::connect(&credWatcher, &QFileSystemWatcher::fileChanged, [&](const QString &) {
		onCredentialsChanged();
	});

	// On auth failure (expired token), pause polling and retry periodically.
	// On Windows the file-watcher above also triggers re-auth when the credentials file changes.
	// On macOS credentials live in Keychain (no file to watch), so the timer is the primary mechanism.
	QTimer authRetryTimer;
	authRetryTimer.setInterval(30000);
	QObject::connect(&authRetryTimer, &QTimer::timeout, [&]() {
		credentialManager.clearCachedToken();
		QString newToken = credentialManager.loadToken();
		if(!newToken.isEmpty())
		{
			apiClient.setToken(newToken);
			apiClient.fetchUsage();
			if(!apiClient.isPolling())
				apiClient.startPolling(pollingIntervalMs);
			authRetryTimer.stop();
		}
	});

	QObject::connect(&apiClient, &UsageApiClient::authenticationFailed, [&]() {
		qInfo() << "Pausing Claude polling \u2014 waiting for credential refresh...";
		apiClient.stopPolling();
		authRetryTimer.start();
	});

	return app.exec();
}
