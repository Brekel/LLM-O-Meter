#pragma once

#include <QString>
#include <QList>

struct AntiGravityServerInfo
{
	bool        found               = false;
	int         extensionServerPort = 0;    // raw value from --extension_server_port
	QList<int>  candidatePorts;             // all localhost TCP ports the process listens on (excluding extensionServerPort)
	QString     csrfToken;
};

class AntiGravityProcessScanner
{
public:
	// Scans all running processes via WMI to find the AntiGravity language server.
	// Returns found=false if the IDE is not running.
	static AntiGravityServerInfo scan();

	// Name of the AntiGravity language server executable to look for.
#ifdef Q_OS_WIN
	static constexpr const wchar_t *ProcessName = L"language_server_windows_x64.exe";
#elif defined(Q_OS_MAC)
	static constexpr const char *ProcessName = "language_server_macos_arm";
#endif

private:
	static AntiGravityServerInfo parseCommandLine(const QString &cmdLine);
	static QList<int>            getTcpListeningPorts(quint32 pid);
};
