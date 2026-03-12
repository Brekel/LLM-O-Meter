#pragma once

#include <QObject>
#include "UsageData.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class UsageApiClient : public QObject
{
	Q_OBJECT

public:
	explicit UsageApiClient(const QString &token, QObject *parent = nullptr);

	void fetchUsage();
	void startPolling(int intervalMs = 60 * 1000);
	void stopPolling();
	bool isPolling() const;
	void setToken(const QString &token);

signals:
	void usageUpdated(const UsageInfo &info);
	void fetchError(const QString &errorMessage);
	void authenticationFailed();

private slots:
	void onReplyFinished(QNetworkReply *reply);

private:
	UsageBucket parseBucket(const QJsonObject &obj) const;
	static bool isClaudeRunning();

	QNetworkAccessManager *networkManager;
	QTimer                *pollTimer;
	QString                authToken;

	static constexpr const char *ApiUrl = "https://api.anthropic.com/api/oauth/usage";
};
