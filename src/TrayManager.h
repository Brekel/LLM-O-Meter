#pragma once

#include <QColor>
#include <QObject>
#include "UsageData.h"
#include "AntiGravityData.h"
#include "UsageRateTracker.h"

class QSystemTrayIcon;
class QMenu;
class UsageApiClient;
class AntiGravityClient;
class UsagePopupWidget;
class CredentialManager;

class TrayManager : public QObject
{
	Q_OBJECT

public:
	explicit TrayManager(UsageApiClient *apiClient, AntiGravityClient *antiGravityClient, CredentialManager *credentialManager, QObject *parent = nullptr);
	~TrayManager();

private slots:
	void onUsageUpdated(const UsageInfo &info);
	void onFetchError(const QString &errorMessage);
	void onSessionIconActivated(QSystemTrayIcon::ActivationReason reason);
	void onWeeklyIconActivated(QSystemTrayIcon::ActivationReason reason);
	void onAntiGravityUpdated(const AntiGravityInfo &info);
	void onAntiGravityError(const QString &errorMessage);
	void onRefreshRequested();
	void onSettingsRequested();
	void onQuitRequested();

private:
	void    setupTrayIcons();
	void    setupContextMenu();
	void    togglePopup();
	void    reapplyLastData();
	void    updateTrayIcon(QSystemTrayIcon *icon, const UsageBucket &bucket, const QString &label, const RateEstimate &rate, const QColor &dotColor);
	void    syncAntiGravityIcons(const AntiGravityInfo &info, const AntiGravityRateInfo &rates);
	void    updatePresenceIcon();
	void    checkAndNotify(const QString &sourceName, double prevUtil, double newUtil);
	QString formatAbsoluteResetTime(const QDateTime &resetsAt) const;

	static QPoint defaultPopupPosition(const QSize &popupSize);
	static QIcon  renderPieIcon(double utilization, const QColor &centerDotColor, int size = 64);
	static QColor utilizationColor(double utilization);
	static QColor agModelColor(int modelIndex);

	static const QColor kDotColor5h; // blue   — 5h weekly
	static const QColor kDotColor7d; // orange — 7d session
	

	QSystemTrayIcon   *presenceIcon = nullptr; // shown only when no data icons are visible
	QSystemTrayIcon   *sessionIcon;
	QSystemTrayIcon   *weeklyIcon;
	QMenu             *contextMenu;
	UsagePopupWidget  *popupWidget;
	UsageApiClient    *apiClient;
	AntiGravityClient *antiGravityClient;
	CredentialManager *credentialManager;
	UsageInfo          lastUsageInfo;

	// Notification state
	QMap<QString, double> lastUtilization; // keyed by source name, for AG models
	bool                  firstPollDone      = false;
	bool                  claudeDataReceived = false;

	// Dynamic list: one icon per AntiGravity model quota
	QList<QSystemTrayIcon *> antiGravityIcons;
	AntiGravityInfo          lastAntiGravityInfo;

	// Session usage rate tracking
	UsageRateTracker m_rateTracker;
};
