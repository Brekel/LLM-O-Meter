#include "stdafx.h"
#include "UsageApiClient.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#elif defined(Q_OS_MAC)
#include <QProcess>
#endif

UsageApiClient::UsageApiClient(const QString &token, QObject *parent)
	: QObject(parent), authToken(token)
{
	networkManager = new QNetworkAccessManager(this);
	connect(networkManager, &QNetworkAccessManager::finished, this, &UsageApiClient::onReplyFinished);

	pollTimer = new QTimer(this);
	connect(pollTimer, &QTimer::timeout, this, &UsageApiClient::fetchUsage);
}

bool UsageApiClient::isClaudeRunning()
{
#ifdef Q_OS_WIN
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hSnap == INVALID_HANDLE_VALUE)
	{
		qWarning() << "[Claude] CreateToolhelp32Snapshot failed";
		return false;
	}

	bool       found = false;
	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof(pe);

	if(Process32FirstW(hSnap, &pe))
	{
		do
		{
			if(_wcsicmp(pe.szExeFile, L"claude.exe") == 0)
			{
				found = true;
				break;
			}
		} while(Process32NextW(hSnap, &pe));
	}

	CloseHandle(hSnap);
	return found;
#elif defined(Q_OS_MAC)
	QProcess ps;
	ps.start("pgrep", QStringList() << "-x" << "claude");
	if(!ps.waitForFinished(3000))
		return false;
	return ps.exitCode() == 0;
#else
	return true; // unsupported platform — assume running
#endif
}

void UsageApiClient::fetchUsage()
{
	if(!isClaudeRunning())
	{
		qDebug() << "[Claude] Process not running — skipping usage fetch";
		return;
	}

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
