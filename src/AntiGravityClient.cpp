#include "stdafx.h"
#include "AntiGravityClient.h"
#include "AntiGravityProcessScanner.h"
#include <QSslConfiguration>
#include <algorithm>
#include <QSslError>
#include <QSslSocket>

AntiGravityClient::AntiGravityClient(QObject *parent)
	: QObject(parent)
{
	networkManager = new QNetworkAccessManager(this);

	// Ignore SSL errors for the local self-signed certificate
	connect(networkManager, &QNetworkAccessManager::sslErrors,
		this, [](QNetworkReply *reply, const QList<QSslError> &) {
			reply->ignoreSslErrors();
		});

	connect(networkManager, &QNetworkAccessManager::finished,
		this, &AntiGravityClient::onReplyFinished);

	pollTimer = new QTimer(this);
	connect(pollTimer, &QTimer::timeout, this, &AntiGravityClient::fetch);
}

void AntiGravityClient::fetch()
{
	AntiGravityServerInfo serverInfo = AntiGravityProcessScanner::scan();

	if(!serverInfo.found)
	{
		qDebug() << "[AG] Language server process not found";
		emit usageUpdated(AntiGravityInfo{});
		return;
	}

	qDebug() << "[AG] Process found — extensionServerPort:" << serverInfo.extensionServerPort
	         << "| candidatePorts:" << serverInfo.candidatePorts
	         << "| csrfToken:" << serverInfo.csrfToken.left(8) << "...";

	cachedToken = serverInfo.csrfToken;

	for(int port : serverInfo.candidatePorts)
		tryFetch(port, cachedToken);
}

bool AntiGravityClient::tryFetch(int port, const QString &csrfToken)
{
	QString urlStr = QString("https://127.0.0.1:%1%2").arg(port).arg(EndpointPath);
	QUrl    url(urlStr);
	if(!url.isValid())
		return false;

	QNetworkRequest request(url);
	request.setRawHeader("x-codeium-csrf-token", csrfToken.toUtf8());
	request.setRawHeader("Content-Type", "application/json");
	request.setRawHeader("Accept", "application/json");

	// Disable SSL peer verification for the local self-signed certificate
	QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
	sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
	request.setSslConfiguration(sslConfig);

	networkManager->post(request, QByteArray("{}"));
	qDebug() << "[AG] POST" << urlStr;
	return true;
}

void AntiGravityClient::startPolling(int intervalMs)
{
	pollTimer->start(intervalMs);
	qInfo() << "AntiGravity polling started, interval:" << intervalMs / 1000 << "seconds";
}

void AntiGravityClient::stopPolling()
{
	pollTimer->stop();
}

void AntiGravityClient::onReplyFinished(QNetworkReply *reply)
{
	reply->deleteLater();

	int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	if(reply->error() != QNetworkReply::NoError)
	{
		// Connection errors are expected when probing ports — don't escalate as user-visible errors
		if(reply->error() == QNetworkReply::ConnectionRefusedError
			|| reply->error() == QNetworkReply::HostNotFoundError)
		{
			qDebug() << "[AG] Connection refused/not found:" << reply->errorString();
			emit usageUpdated(AntiGravityInfo{});
		}
		else
		{
			QByteArray body = reply->readAll();
			qWarning() << "[AG] Request failed — error:" << reply->error()
			           << reply->errorString()
			           << "| HTTP:" << httpStatus
			           << "| URL:" << reply->url().toString()
			           << "| body:" << body.left(500);
			emit fetchError(QString("AntiGravity request failed: %1").arg(reply->errorString()));
		}
		return;
	}

	if(httpStatus < 200 || httpStatus >= 300)
	{
		QByteArray body = reply->readAll();
		qWarning() << "[AG] Non-success HTTP status:" << httpStatus
		           << "| URL:" << reply->url().toString()
		           << "| body:" << body.left(500);
		emit fetchError(QString("AntiGravity: HTTP %1").arg(httpStatus));
		return;
	}

	QByteArray data = reply->readAll();
	qDebug() << "[AG] HTTP" << httpStatus << "— response (" << data.size() << "bytes)";

	AntiGravityInfo info = parseResponse(data);
	info.isAvailable = true;

	qDebug() << "[AG] Parsed — tier:" << info.userTier
	         << "| models:" << info.modelQuotas.size();
	for(const AntiGravityModelQuota &q : info.modelQuotas)
		qDebug() << "[AG]  " << q.modelName << q.utilization << "%";

	emit usageUpdated(info);
}

AntiGravityInfo AntiGravityClient::parseResponse(const QByteArray &data) const
{
	AntiGravityInfo info;

	QJsonParseError parseError;
	QJsonDocument   doc = QJsonDocument::fromJson(data, &parseError);
	if(parseError.error != QJsonParseError::NoError || !doc.isObject())
	{
		qWarning() << "[AG] Failed to parse response:" << parseError.errorString();
		return info;
	}

	// Structure: root -> userStatus -> { planStatus.planInfo.planName,
	//                                    cascadeModelConfigData.clientModelConfigs[] }
	QJsonObject userStatus = doc.object().value("userStatus").toObject();

	// Tier — prefer direct userTier field, fall back to planStatus.planInfo.planName
	info.userTier = userStatus.value("userTier").toString();
	if(info.userTier.isEmpty())
	{
		info.userTier = userStatus.value("planStatus").toObject()
		                          .value("planInfo").toObject()
		                          .value("planName").toString();
	}

	// Model quotas
	QJsonArray clientModelConfigs = userStatus.value("cascadeModelConfigData")
	                                          .toObject()
	                                          .value("clientModelConfigs")
	                                          .toArray();
	for(const QJsonValue &val : clientModelConfigs)
	{
		if(!val.isObject())
			continue;
		AntiGravityModelQuota quota = parseModelQuota(val.toObject());
		if(!quota.modelName.isEmpty())
			info.modelQuotas.append(quota);
	}

	// Sort model quotas into a canonical display order so tray icons and the popup
	// always appear left-to-right / top-to-bottom in the desired sequence.
	// Models not in the priority list are appended after the known ones.
	static const QStringList kModelOrder = {
		"Gemini 3.1 Pro (High)",
		"Gemini 3.1 Pro (Low)",
		"Gemini 3 Flash",
		"Claude Sonnet 4.6",
		"Claude Opus 4.6",
		"GPT-OSS 120B",
	};

	auto priority = [&](const QString &name) -> int {
		int idx = kModelOrder.indexOf(name);
		return idx >= 0 ? idx : kModelOrder.size();
	};

	std::stable_sort(info.modelQuotas.begin(), info.modelQuotas.end(),
		[&](const AntiGravityModelQuota &a, const AntiGravityModelQuota &b) {
			return priority(a.modelName) < priority(b.modelName);
		});

	return info;
}

AntiGravityModelQuota AntiGravityClient::parseModelQuota(const QJsonObject &obj) const
{
	AntiGravityModelQuota quota;

	// Model display name
	quota.modelName = obj.value("label").toString();

	// quotaInfo.remainingFraction is 0.0–1.0; utilization = (1 - remaining) * 100
	QJsonObject quotaInfo     = obj.value("quotaInfo").toObject();
	double      remainingFrac = quotaInfo.value("remainingFraction").toDouble(1.0);
	quota.utilization         = (1.0 - remainingFrac) * 100.0;

	// Reset time
	QString resetTimeStr = quotaInfo.value("resetTime").toString();
	if(!resetTimeStr.isEmpty())
	{
		quota.resetsAt     = QDateTime::fromString(resetTimeStr, Qt::ISODate);
		quota.hasResetTime = quota.resetsAt.isValid();
	}

	return quota;
}
