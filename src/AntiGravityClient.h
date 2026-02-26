#pragma once

#include <QObject>
#include <QJsonObject>
#include "AntiGravityData.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class AntiGravityClient : public QObject
{
	Q_OBJECT

public:
	explicit AntiGravityClient(QObject *parent = nullptr);

	void fetch();
	void startPolling(int intervalMs = 60 * 1000);
	void stopPolling();

signals:
	void usageUpdated(const AntiGravityInfo &info);
	void fetchError(const QString &errorMessage);

private slots:
	void onReplyFinished(QNetworkReply *reply);

private:
	AntiGravityInfo parseResponse(const QByteArray &data) const;
	AntiGravityModelQuota parseModelQuota(const QJsonObject &obj) const;
	bool tryFetch(int port, const QString &csrfToken);

	QNetworkAccessManager *networkManager;
	QTimer                *pollTimer;

	QString cachedToken; // csrf token from last scan, for reference only

	static constexpr const char *EndpointPath =
		"/exa.language_server_pb.LanguageServerService/GetUserStatus";
};
