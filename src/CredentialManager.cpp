#include "stdafx.h"
#include "CredentialManager.h"

#ifdef Q_OS_MAC
#include <Security/Security.h>
#endif

CredentialManager::CredentialManager(QObject *parent) : QObject(parent) {}

QString CredentialManager::loadToken() const
{
	if(!m_cachedToken.isEmpty())
		return m_cachedToken;

	QString token = loadTokenFromFile();
	if(!token.isEmpty())
	{
		m_cachedToken = token;
		return token;
	}

#ifdef Q_OS_MAC
	token = loadTokenFromKeychain();
	if(!token.isEmpty())
	{
		m_cachedToken = token;
		return token;
	}
#endif

	qInfo() << "Could not load token, trying QSettings fallback...";
	token = loadTokenFromSettings();
	if(!token.isEmpty())
		m_cachedToken = token;
	return token;
}

void CredentialManager::clearCachedToken()
{
	m_cachedToken.clear();
}

void CredentialManager::saveManualToken(const QString &token)
{
	QSettings settings;
	settings.setValue("auth/manualToken", token);
	qInfo() << "Manual token saved to QSettings.";
}

QString CredentialManager::loadTokenFromFile() const
{
	QString path = credentialsFilePath();
	QFile file(path);
	if(!file.exists())
	{
		qInfo() << "Credentials file not found at:" << path;
		return {};
	}

	if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		qWarning() << "Failed to open credentials file:" << path;
		return {};
	}

	QJsonParseError parseError;
	QJsonDocument   doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	file.close();

	if(parseError.error != QJsonParseError::NoError)
	{
		qWarning() << "Failed to parse credentials JSON:" << parseError.errorString();
		return {};
	}

	QJsonObject root  = doc.object();
	QJsonObject oauth = root.value("claudeAiOauth").toObject();
	QString     token = oauth.value("accessToken").toString();

	if(token.isEmpty())
		qWarning() << "accessToken field is empty or missing in credentials file.";
	else
		qInfo() << "Token loaded from credentials file.";

	return token;
}

QString CredentialManager::loadTokenFromKeychain() const
{
#ifdef Q_OS_MAC
	CFMutableDictionaryRef query = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
	CFDictionarySetValue(query, kSecAttrService, CFSTR("Claude Code-credentials"));
	CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
	CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

	CFTypeRef result = nullptr;
	OSStatus  status = SecItemCopyMatching(query, &result);
	CFRelease(query);

	if(status != errSecSuccess || !result)
	{
		qInfo() << "Keychain lookup failed (status:" << status << ")";
		return {};
	}

	CFDataRef  data     = static_cast<CFDataRef>(result);
	QByteArray jsonData(reinterpret_cast<const char *>(CFDataGetBytePtr(data)), static_cast<int>(CFDataGetLength(data)));
	CFRelease(result);

	QJsonParseError parseError;
	QJsonDocument   doc = QJsonDocument::fromJson(jsonData, &parseError);
	if(parseError.error != QJsonParseError::NoError)
	{
		qWarning() << "Failed to parse keychain credentials JSON:" << parseError.errorString();
		return {};
	}

	QJsonObject root  = doc.object();
	QJsonObject oauth = root.value("claudeAiOauth").toObject();
	QString     token = oauth.value("accessToken").toString();

	if(token.isEmpty())
		qWarning() << "accessToken empty or missing in keychain credentials.";
	else
		qInfo() << "Token loaded from macOS Keychain.";

	return token;
#else
	return {};
#endif
}

bool CredentialManager::hasCredentialsFile() const
{
	return QFile::exists(credentialsFilePath());
}

int CredentialManager::loadPollingInterval() const
{
	QSettings settings;
	return settings.value("polling/intervalSeconds", 60).toInt();
}

void CredentialManager::savePollingInterval(int seconds)
{
	QSettings settings;
	settings.setValue("polling/intervalSeconds", seconds);
}

bool CredentialManager::loadClaudeEnabled() const
{
	QSettings settings;
	return settings.value("claude/enabled", true).toBool();
}

void CredentialManager::saveClaudeEnabled(bool enabled)
{
	QSettings settings;
	settings.setValue("claude/enabled", enabled);
}

bool CredentialManager::loadAntiGravityEnabled() const
{
	QSettings settings;
	return settings.value("antigravity/enabled", true).toBool();
}

void CredentialManager::saveAntiGravityEnabled(bool enabled)
{
	QSettings settings;
	settings.setValue("antigravity/enabled", enabled);
}

QString CredentialManager::loadTokenFromSettings() const
{
	QSettings settings;
	return settings.value("auth/manualToken").toString();
}

QString CredentialManager::credentialsFilePath() const
{
	return QDir::homePath() + "/.claude/.credentials.json";
}

bool CredentialManager::loadHideZeroIcons() const
{
	QSettings settings;
	return settings.value("tray/hideZeroIcons", false).toBool();
}

void CredentialManager::saveHideZeroIcons(bool hide)
{
	QSettings settings;
	settings.setValue("tray/hideZeroIcons", hide);
}

bool CredentialManager::loadShowRateEstimates() const
{
	QSettings settings;
	return settings.value("tray/showRateEstimates", false).toBool();
}

void CredentialManager::saveShowRateEstimates(bool show)
{
	QSettings settings;
	settings.setValue("tray/showRateEstimates", show);
}

QPoint CredentialManager::loadPopupPosition() const
{
	QSettings settings;
	if(!settings.contains("popup/posX"))
		return QPoint();
	return QPoint(settings.value("popup/posX").toInt(), settings.value("popup/posY").toInt());
}

void CredentialManager::savePopupPosition(const QPoint &pos)
{
	QSettings settings;
	settings.setValue("popup/posX", pos.x());
	settings.setValue("popup/posY", pos.y());
}

void CredentialManager::clearPopupPosition()
{
	QSettings settings;
	settings.remove("popup/posX");
	settings.remove("popup/posY");
}

bool CredentialManager::loadNotifyEnabled() const
{
	QSettings settings;
	return settings.value("notifications/enabled", false).toBool();
}

void CredentialManager::saveNotifyEnabled(bool enabled)
{
	QSettings settings;
	settings.setValue("notifications/enabled", enabled);
}

bool CredentialManager::loadNotifyBelowThreshold() const
{
	QSettings settings;
	return settings.value("notifications/belowThreshold", true).toBool();
}

void CredentialManager::saveNotifyBelowThreshold(bool enabled)
{
	QSettings settings;
	settings.setValue("notifications/belowThreshold", enabled);
}

int CredentialManager::loadNotifyThresholdPercent() const
{
	QSettings settings;
	return settings.value("notifications/thresholdPercent", 90).toInt();
}

void CredentialManager::saveNotifyThresholdPercent(int percent)
{
	QSettings settings;
	settings.setValue("notifications/thresholdPercent", percent);
}

bool CredentialManager::loadNotifyExhausted() const
{
	QSettings settings;
	return settings.value("notifications/exhausted", true).toBool();
}

void CredentialManager::saveNotifyExhausted(bool enabled)
{
	QSettings settings;
	settings.setValue("notifications/exhausted", enabled);
}

bool CredentialManager::loadNotifyRenewed() const
{
	QSettings settings;
	return settings.value("notifications/renewed", true).toBool();
}

void CredentialManager::saveNotifyRenewed(bool enabled)
{
	QSettings settings;
	settings.setValue("notifications/renewed", enabled);
}
