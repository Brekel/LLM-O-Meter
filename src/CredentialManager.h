#pragma once

#include <QObject>
#include <QPoint>
#include <QString>

class CredentialManager : public QObject
{
	Q_OBJECT

public:
	explicit CredentialManager(QObject *parent = nullptr);

	QString loadToken() const;
	bool    hasCredentialsFile() const;
	void    saveManualToken(const QString &token);

	int  loadPollingInterval() const;
	void savePollingInterval(int seconds);

	bool loadClaudeEnabled() const;
	void saveClaudeEnabled(bool enabled);

	bool loadAntiGravityEnabled() const;
	void saveAntiGravityEnabled(bool enabled);

	bool loadNotifyEnabled() const;
	void saveNotifyEnabled(bool enabled);

	bool loadNotifyBelowThreshold() const;
	void saveNotifyBelowThreshold(bool enabled);

	int  loadNotifyThresholdPercent() const;
	void saveNotifyThresholdPercent(int percent);

	bool loadNotifyExhausted() const;
	void saveNotifyExhausted(bool enabled);

	bool loadNotifyRenewed() const;
	void saveNotifyRenewed(bool enabled);

	bool loadHideZeroIcons() const;
	void saveHideZeroIcons(bool hide);

	bool loadShowRateEstimates() const;
	void saveShowRateEstimates(bool show);

	QPoint  loadPopupPosition() const;
	void    savePopupPosition(const QPoint &pos);
	void    clearPopupPosition();
	QString credentialsFilePath() const;

	void clearCachedToken();

private:
	QString loadTokenFromFile() const;
	QString loadTokenFromKeychain() const;
	QString loadTokenFromSettings() const;

	mutable QString m_cachedToken;
};
