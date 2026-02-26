#include "stdafx.h"
#include "UsageApiClient.h"

UsageApiClient::UsageApiClient(const QString &token, QObject *parent)
	: QObject(parent), authToken(token)
{
	networkManager = new QNetworkAccessManager(this);
	connect(networkManager, &QNetworkAccessManager::finished, this, &UsageApiClient::onReplyFinished);

	pollTimer = new QTimer(this);
	connect(pollTimer, &QTimer::timeout, this, &UsageApiClient::fetchUsage);
}

void UsageApiClient::fetchUsage()
{
	QNetworkRequest request{QUrl{ApiUrl}};
	request.setRawHeader("Authorization", ("Bearer " + authToken).toUtf8());
	request.setRawHeader("anthropic-beta", "oauth-2025-04-20");
	request.setRawHeader("Accept", "application/json");

	networkManager->get(request);
	qDebug() << "Fetching usage data...";
}

void UsageApiClient::startPolling(int intervalMs)
{
	pollTimer->start(intervalMs);
	qInfo() << "Polling started, interval:" << intervalMs / 1000 << "seconds";
}

void UsageApiClient::stopPolling()
{
	pollTimer->stop();
}

bool UsageApiClient::isPolling() const
{
	return pollTimer->isActive();
}

void UsageApiClient::setToken(const QString &token)
{
	authToken = token;
}

void UsageApiClient::onReplyFinished(QNetworkReply *reply)
{
	reply->deleteLater();

	if(reply->error() != QNetworkReply::NoError)
	{
		int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if(httpStatus == 401)
		{
			qInfo() << "Authentication failed (HTTP 401) — token may be expired.";
			emit authenticationFailed();
			return;
		}
		QString errorMsg = QString("API request failed: %1 (HTTP %2)").arg(reply->errorString()).arg(httpStatus);
		qWarning() << errorMsg;
		emit fetchError(errorMsg);
		return;
	}

	QByteArray      data = reply->readAll();
	QJsonParseError parseError;
	QJsonDocument   doc = QJsonDocument::fromJson(data, &parseError);

	if(parseError.error != QJsonParseError::NoError)
	{
		QString errorMsg = "Failed to parse API response: " + parseError.errorString();
		qWarning() << errorMsg;
		emit fetchError(errorMsg);
		return;
	}

	QJsonObject root = doc.object();

	UsageInfo info;
	info.fiveHour     = parseBucket(root.value("five_hour").toObject());
	info.sevenDay     = parseBucket(root.value("seven_day").toObject());
	info.sevenDayOpus = parseBucket(root.value("seven_day_opus").toObject());

	qDebug() << "Usage updated — 5h:" << info.fiveHour.utilization << "% | 7d:" << info.sevenDay.utilization << "% | Opus:" << info.sevenDayOpus.utilization << "%";
	emit usageUpdated(info);
}

UsageBucket UsageApiClient::parseBucket(const QJsonObject &obj) const
{
	UsageBucket bucket;
	bucket.utilization = obj.value("utilization").toDouble(0.0);

	QJsonValue resetsAtVal = obj.value("resets_at");
	if(!resetsAtVal.isNull() && resetsAtVal.isString())
	{
		bucket.resetsAt     = QDateTime::fromString(resetsAtVal.toString(), Qt::ISODate);
		bucket.hasResetTime = bucket.resetsAt.isValid();
	}

	return bucket;
}
