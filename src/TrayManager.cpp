#include "stdafx.h"
#include "TrayManager.h"
#include "AntiGravityClient.h"

const QColor TrayManager::kDotColor5h(0x5B, 0x9B, 0xD5); // blue   — 5h weekly
const QColor TrayManager::kDotColor7d(0xDA, 0x77, 0x56); // orange — 7d session
#include "CredentialManager.h"
#include "FormatUtils.h"
#include "SettingsDialog.h"
#include "UsageApiClient.h"
#include "UsagePopupWidget.h"

TrayManager::TrayManager(UsageApiClient *apiClient, AntiGravityClient *antiGravityClient, CredentialManager *credentialManager, QObject *parent)
	: QObject(parent), apiClient(apiClient), antiGravityClient(antiGravityClient), credentialManager(credentialManager)
{
	popupWidget = new UsagePopupWidget();

	// Restore saved popup position so togglePopup won't override it
	QPoint savedPos = credentialManager->loadPopupPosition();
	if(!savedPos.isNull())
		popupWidget->setUserPosition(savedPos);

	setupTrayIcons();
	setupContextMenu();

	connect(popupWidget, &UsagePopupWidget::hiddenByDeactivation, this, [this]() {
		if(popupWidget->hasUserPosition())
			this->credentialManager->savePopupPosition(popupWidget->pos());
	});

	connect(popupWidget, &UsagePopupWidget::resetPositionRequested, this, [this]() {
		this->credentialManager->clearPopupPosition();
		popupWidget->clearUserPosition();
		popupWidget->adjustSize();
		popupWidget->move(defaultPopupPosition(popupWidget->sizeHint()));
	});

	connect(apiClient, &UsageApiClient::usageUpdated, this, &TrayManager::onUsageUpdated);
	connect(apiClient, &UsageApiClient::fetchError, this, &TrayManager::onFetchError);

	if(antiGravityClient)
	{
		connect(antiGravityClient, &AntiGravityClient::usageUpdated, this, &TrayManager::onAntiGravityUpdated);
		connect(antiGravityClient, &AntiGravityClient::fetchError, this, &TrayManager::onAntiGravityError);
	}
}

TrayManager::~TrayManager()
{
	delete popupWidget;
	delete contextMenu;
}

void TrayManager::setupTrayIcons()
{
	presenceIcon = new QSystemTrayIcon(QIcon(":/llm-o-meter.svg"), this);
	presenceIcon->setToolTip("LLM-O-Meter (waiting for data...)");
	presenceIcon->show();
	connect(presenceIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
		if(reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
			togglePopup();
	});

	sessionIcon = new QSystemTrayIcon(renderPieIcon(0.0, kDotColor5h), this);
	sessionIcon->setToolTip("Claude 5h: --");
	sessionIcon->hide();
	connect(sessionIcon, &QSystemTrayIcon::activated, this, &TrayManager::onSessionIconActivated);

	weeklyIcon = new QSystemTrayIcon(renderPieIcon(0.0, kDotColor7d), this);
	weeklyIcon->setToolTip("Claude 7d: --");
	weeklyIcon->hide();
	connect(weeklyIcon, &QSystemTrayIcon::activated, this, &TrayManager::onWeeklyIconActivated);
}

void TrayManager::setupContextMenu()
{
	contextMenu = new QMenu();
	contextMenu->addAction("Show Usage", this, &TrayManager::togglePopup);
	contextMenu->addSeparator();
	contextMenu->addAction("Refresh Now", this, &TrayManager::onRefreshRequested);
	contextMenu->addAction("Settings...", this, &TrayManager::onSettingsRequested);
	contextMenu->addSeparator();
	contextMenu->addAction("Quit", this, &TrayManager::onQuitRequested);

	presenceIcon->setContextMenu(contextMenu);
	sessionIcon->setContextMenu(contextMenu);
	weeklyIcon->setContextMenu(contextMenu);
	// AntiGravity icons get the same context menu when they are created in syncAntiGravityIcons()
}

void TrayManager::onUsageUpdated(const UsageInfo &info)
{
	claudeDataReceived = true;

	if(firstPollDone)
	{
		checkAndNotify("Claude 5h", lastUsageInfo.fiveHour.utilization, info.fiveHour.utilization);
		checkAndNotify("Claude 7d", lastUsageInfo.sevenDay.utilization, info.sevenDay.utilization);
	}

	lastUsageInfo = info;
	firstPollDone = true;

	QDateTime now = QDateTime::currentDateTimeUtc();
	m_rateTracker.addObservation("claude_5h",   info.fiveHour.utilization,    now);
	m_rateTracker.addObservation("claude_7d",   info.sevenDay.utilization,    now);
	m_rateTracker.addObservation("claude_opus", info.sevenDayOpus.utilization, now);

	RateEstimate est5h   = m_rateTracker.estimate("claude_5h",   now, info.fiveHour.resetsAt,     info.fiveHour.hasResetTime);
	RateEstimate est7d   = m_rateTracker.estimate("claude_7d",   now, info.sevenDay.resetsAt,     info.sevenDay.hasResetTime);
	RateEstimate estOpus = m_rateTracker.estimate("claude_opus", now, info.sevenDayOpus.resetsAt, info.sevenDayOpus.hasResetTime);

	updateTrayIcon(sessionIcon, info.fiveHour, "5h", est5h, kDotColor5h);
#ifdef Q_OS_MAC
	{
		UsageBucket  bucket7d = info.sevenDay;
		RateEstimate rate7d   = est7d;
		QTimer::singleShot(0, this, [this, bucket7d, rate7d]() {
			updateTrayIcon(weeklyIcon, bucket7d, "7d", rate7d, kDotColor7d);
			updatePresenceIcon();
		});
	}
#else
	updateTrayIcon(weeklyIcon, info.sevenDay, "7d", est7d, kDotColor7d);
#endif

	UsageRateInfo rateInfo{est5h, est7d, estOpus};
	popupWidget->setShowRateEstimates(credentialManager->loadShowRateEstimates());
	popupWidget->updateUsage(info, rateInfo);

#ifndef Q_OS_MAC
	updatePresenceIcon();
#endif
}

void TrayManager::onFetchError(const QString &errorMessage)
{
	if(!claudeDataReceived)
	{
		qInfo() << "[Claude] Suppressing fetch error balloon (no data yet):" << errorMessage;
		return;
	}
	sessionIcon->showMessage("LLM-O-Meter", "Failed to fetch usage: " + errorMessage, QSystemTrayIcon::Warning, 5000);
}

void TrayManager::onSessionIconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if(reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
		togglePopup();
}

void TrayManager::onWeeklyIconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if(reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
		togglePopup();
}

void TrayManager::onAntiGravityUpdated(const AntiGravityInfo &info)
{
	if(firstPollDone && info.isAvailable)
	{
		for(const AntiGravityModelQuota &quota : info.modelQuotas)
		{
			QString key      = "AG " + quota.modelName;
			double  prevUtil = lastUtilization.value(key, 0.0);
			checkAndNotify(key, prevUtil, quota.utilization);
			lastUtilization.insert(key, quota.utilization);
		}
	}
	else if(info.isAvailable)
	{
		// First poll — seed the map without firing notifications
		for(const AntiGravityModelQuota &quota : info.modelQuotas)
			lastUtilization.insert("AG " + quota.modelName, quota.utilization);
	}

	lastAntiGravityInfo = info;

	QDateTime          now     = QDateTime::currentDateTimeUtc();
	AntiGravityRateInfo agRates;
	if(info.isAvailable)
	{
		for(const AntiGravityModelQuota &quota : info.modelQuotas)
		{
			m_rateTracker.addObservation(quota.modelName, quota.utilization, now);
			agRates.insert(quota.modelName,
			               m_rateTracker.estimate(quota.modelName, now, quota.resetsAt, quota.hasResetTime));
		}
	}

	syncAntiGravityIcons(info, agRates);
	popupWidget->setShowRateEstimates(credentialManager->loadShowRateEstimates());
	popupWidget->updateAntiGravity(info, agRates);
}

void TrayManager::onAntiGravityError(const QString &errorMessage)
{
	qWarning() << "AntiGravity fetch error:" << errorMessage;
	// Not surfaced as a balloon — AntiGravity errors are secondary to Claude errors
}

void TrayManager::syncAntiGravityIcons(const AntiGravityInfo &info, const AntiGravityRateInfo &rates)
{
	if(!info.isAvailable || info.modelQuotas.isEmpty())
	{
		for(QSystemTrayIcon *icon : antiGravityIcons)
			icon->hide();
		updatePresenceIcon();
		return;
	}

	// Grow the list if needed
	while(antiGravityIcons.size() < info.modelQuotas.size())
	{
		int              newIndex = antiGravityIcons.size();
		QSystemTrayIcon *icon     = new QSystemTrayIcon(renderPieIcon(0.0, agModelColor(newIndex)), this);
		icon->setContextMenu(contextMenu);
		connect(icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
			if(reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
				togglePopup();
		});
		antiGravityIcons.append(icon);
	}

	// Update visible icons
	for(int i = 0; i < info.modelQuotas.size(); ++i)
	{
		const AntiGravityModelQuota &quota = info.modelQuotas[i];
		QSystemTrayIcon             *icon  = antiGravityIcons[i];

		if(credentialManager->loadHideZeroIcons() && quota.utilization <= 0.0)
		{
			icon->hide();
			continue;
		}

		icon->setIcon(renderPieIcon(quota.utilization, agModelColor(i)));

		QString tooltip = QString("AG %1: %2%").arg(quota.modelName).arg(quota.utilization, 0, 'f', 1);
		if(quota.hasResetTime)
			tooltip += QString("\nResets %1 (%2)").arg(FormatUtils::timeUntilReset(quota.resetsAt), formatAbsoluteResetTime(quota.resetsAt));
		if(credentialManager->loadShowRateEstimates())
		{
			QString rateAnnotation = FormatUtils::exhaustionLabel(rates.value(quota.modelName));
			if(!rateAnnotation.isEmpty())
				tooltip += "\n" + rateAnnotation;
		}
		icon->setToolTip(tooltip);
		icon->show();
	}

	// Hide surplus icons from a previous response with more models
	for(int i = info.modelQuotas.size(); i < antiGravityIcons.size(); ++i)
		antiGravityIcons[i]->hide();

	updatePresenceIcon();
}

void TrayManager::updatePresenceIcon()
{
	bool anyVisible = sessionIcon->isVisible() || weeklyIcon->isVisible();
	if(!anyVisible)
	{
		for(QSystemTrayIcon *icon : antiGravityIcons)
		{
			if(icon->isVisible())
			{
				anyVisible = true;
				break;
			}
		}
	}
	presenceIcon->setVisible(!anyVisible);
}

void TrayManager::checkAndNotify(const QString &sourceName, double prevUtil, double newUtil)
{
	if(!credentialManager->loadNotifyEnabled())
		return;

	double threshold = static_cast<double>(credentialManager->loadNotifyThresholdPercent());

	// Exhausted — crossed from below 100% to at/above 100%
	if(credentialManager->loadNotifyExhausted() && newUtil >= 100.0 && prevUtil < 100.0)
	{
		sessionIcon->showMessage(
			"Quota Exhausted",
			sourceName + " quota is now exhausted (100%+).",
			QSystemTrayIcon::Warning, 6000);
		return; // exhausted supersedes threshold warning
	}

	// Threshold crossed — utilization rose past the configured % used
	if(credentialManager->loadNotifyBelowThreshold() && newUtil >= threshold && prevUtil < threshold)
	{
		sessionIcon->showMessage(
			"Quota Warning",
			QString("%1 has reached %2% used.").arg(sourceName).arg(newUtil, 0, 'f', 1),
			QSystemTrayIcon::Warning, 5000);
		return;
	}

	// Renewed — utilization dropped back well below the threshold
	if(credentialManager->loadNotifyRenewed() && newUtil <= threshold / 2.0 && prevUtil > threshold)
	{
		sessionIcon->showMessage(
			"Quota Renewed",
			QString("%1 quota has renewed (%2% used).").arg(sourceName).arg(newUtil, 0, 'f', 1),
			QSystemTrayIcon::Information, 5000);
	}
}

void TrayManager::onRefreshRequested()
{
	apiClient->fetchUsage();
	if(antiGravityClient)
		antiGravityClient->fetch();
}

void TrayManager::onSettingsRequested()
{
	SettingsDialog dialog(nullptr, credentialManager);
	connect(&dialog, &SettingsDialog::pollingIntervalChanged, this, [this](int seconds) {
		apiClient->stopPolling();
		apiClient->startPolling(seconds * 1000);
	});

	if(dialog.exec() == QDialog::Accepted)
	{
		m_rateTracker.clearAll();
		reapplyLastData();
		credentialManager->clearCachedToken();
		QString token = credentialManager->loadToken();
		if(!token.isEmpty())
		{
			apiClient->setToken(token);
			apiClient->fetchUsage();
		}
	}
}

void TrayManager::reapplyLastData()
{
	if(!firstPollDone)
		return;

	QDateTime now = QDateTime::currentDateTimeUtc();

	RateEstimate est5h   = m_rateTracker.estimate("claude_5h",   now, lastUsageInfo.fiveHour.resetsAt,     lastUsageInfo.fiveHour.hasResetTime);
	RateEstimate est7d   = m_rateTracker.estimate("claude_7d",   now, lastUsageInfo.sevenDay.resetsAt,     lastUsageInfo.sevenDay.hasResetTime);
	RateEstimate estOpus = m_rateTracker.estimate("claude_opus", now, lastUsageInfo.sevenDayOpus.resetsAt, lastUsageInfo.sevenDayOpus.hasResetTime);

	updateTrayIcon(sessionIcon, lastUsageInfo.fiveHour, "5h", est5h, kDotColor5h);
#ifdef Q_OS_MAC
	{
		UsageBucket  bucket7d = lastUsageInfo.sevenDay;
		RateEstimate rate7d   = est7d;
		QTimer::singleShot(0, this, [this, bucket7d, rate7d]() {
			updateTrayIcon(weeklyIcon, bucket7d, "7d", rate7d, kDotColor7d);
		});
	}
#else
	updateTrayIcon(weeklyIcon, lastUsageInfo.sevenDay, "7d", est7d, kDotColor7d);
#endif

	AntiGravityRateInfo agRates;
	if(lastAntiGravityInfo.isAvailable)
	{
		for(const AntiGravityModelQuota &quota : lastAntiGravityInfo.modelQuotas)
			agRates.insert(quota.modelName,
			               m_rateTracker.estimate(quota.modelName, now, quota.resetsAt, quota.hasResetTime));
	}
	syncAntiGravityIcons(lastAntiGravityInfo, agRates);
}

void TrayManager::onQuitRequested()
{
	QApplication::quit();
}

void TrayManager::togglePopup()
{
	if(popupWidget->isVisible())
	{
		if(popupWidget->hasUserPosition())
			credentialManager->savePopupPosition(popupWidget->pos());
		popupWidget->hide();
		return;
	}

	// Ensure layout is up to date so sizeHint is correct
	popupWidget->adjustSize();

	// Only snap to the default near-tray position if the user hasn't manually moved it
	if(!popupWidget->hasUserPosition())
		popupWidget->move(defaultPopupPosition(popupWidget->sizeHint()));
	popupWidget->show();
	popupWidget->raise();
	popupWidget->activateWindow();
}

QPoint TrayManager::defaultPopupPosition(const QSize &popupSize)
{
	QScreen *screen    = QApplication::primaryScreen();
	QRect    screenGeo = screen->availableGeometry();

	int x = screenGeo.right() - popupSize.width() - 8;
#ifdef Q_OS_MAC
	int y = screenGeo.top() + 8;
#else
	int y = screenGeo.bottom() - popupSize.height() - 8;
#endif

	// Clamp to screen bounds with margin
	x = qMax(screenGeo.left() + 8, qMin(x, screenGeo.right()  - popupSize.width()))  - 32;
	y = qMax(screenGeo.top()  + 8, qMin(y, screenGeo.bottom() - popupSize.height())) - 12;

	return QPoint(x, y);
}

void TrayManager::updateTrayIcon(QSystemTrayIcon *icon, const UsageBucket &bucket,
                                 const QString &label, const RateEstimate &rate,
                                 const QColor &dotColor)
{
	if(credentialManager->loadHideZeroIcons() && bucket.utilization <= 0.0)
	{
		icon->hide();
		return;
	}

	icon->setIcon(renderPieIcon(bucket.utilization, dotColor));

	QString tooltip = QString("Claude %1: %2%").arg(label).arg(bucket.utilization, 0, 'f', 1);
	if(bucket.hasResetTime)
		tooltip += QString("\nResets %1 (%2)").arg(FormatUtils::timeUntilReset(bucket.resetsAt), formatAbsoluteResetTime(bucket.resetsAt));
	if(credentialManager->loadShowRateEstimates())
	{
		QString rateAnnotation = FormatUtils::exhaustionLabel(rate);
		if(!rateAnnotation.isEmpty())
			tooltip += "\n" + rateAnnotation;
	}

	icon->setToolTip(tooltip);
	icon->show();
}

QString TrayManager::formatAbsoluteResetTime(const QDateTime &resetsAt) const
{
	QDateTime localTime = resetsAt.toLocalTime();
	return localTime.toString("ddd H:mm");
}

QIcon TrayManager::renderPieIcon(double utilization, const QColor &centerDotColor, int size)
{
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);

	const int    ringWidth = qMax(3, size / 5);
	const int    margin    = ringWidth / 2 + 2;
	const QRectF ringRect(margin, margin, size - 2 * margin, size - 2 * margin);

	// Ring track: full dark-gray circle (NoBrush = transparent center / donut hole)
	painter.setPen(QPen(QColor(60, 60, 60), ringWidth, Qt::SolidLine, Qt::RoundCap));
	painter.setBrush(Qt::NoBrush);
	painter.drawEllipse(ringRect);

	// Utilization arc: painted over the track in green/amber/red
	if(utilization > 0.0)
	{
		double clampedUtil = qMin(utilization, 100.0);
		int    startAngle  = 90 * 16; // 12 o'clock
		int    spanAngle   = -static_cast<int>(clampedUtil / 100.0 * 360.0 * 16); // clockwise

		painter.setPen(QPen(utilizationColor(clampedUtil), ringWidth, Qt::SolidLine, Qt::RoundCap));
		painter.setBrush(Qt::NoBrush);
		painter.drawArc(ringRect, startAngle, spanAngle);
	}

	// Center identity dot — color encodes which icon this is
	const double dotRadius = size * 0.15;
	const double cx        = size / 2.0;
	const double cy        = size / 2.0;
	painter.setPen(Qt::NoPen);
	painter.setBrush(centerDotColor);
	painter.drawEllipse(QRectF(cx - dotRadius, cy - dotRadius, dotRadius * 2.0, dotRadius * 2.0));

	painter.end();
	return QIcon(pixmap);
}

QColor TrayManager::utilizationColor(double utilization)
{
	return FormatUtils::utilizationColor(utilization);
}

// static
QColor TrayManager::agModelColor(int modelIndex)
{
	static const QColor palette[] = {
		QColor(0xE0, 0x6C, 0x75), // rose-red
		QColor(0x61, 0xAF, 0xEF), // sky-blue
		QColor(0x98, 0xC3, 0x79), // sage-green
		QColor(0xE5, 0xC0, 0x7B), // warm-yellow
		QColor(0xC6, 0x78, 0xDD), // soft-purple
		QColor(0x56, 0xB6, 0xC2), // teal
	};
	constexpr int N = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
	return palette[modelIndex % N];
}
