#pragma once

#include <QDateTime>
#include <QColor>
#include <QString>
#include "UsageRateTracker.h"

namespace FormatUtils
{

inline QString timeUntilReset(const QDateTime &resetsAt)
{
	QDateTime now      = QDateTime::currentDateTimeUtc();
	qint64    secsLeft = now.secsTo(resetsAt);

	if(secsLeft <= 0)
		return "now";

	qint64 days  = secsLeft / 86400;
	qint64 hours = (secsLeft % 86400) / 3600;
	qint64 mins  = (secsLeft % 3600) / 60;

	if(days > 0)
		return QString("in %1d %2h").arg(days).arg(hours);
	if(hours > 0)
		return QString("in %1h %2m").arg(hours).arg(mins);
	return QString("in %1m").arg(mins);
}

inline QString exhaustionLabel(const RateEstimate &est)
{
	if(!est.valid || !est.exhaustsBeforeReset || est.ratePerHour <= 0.0)
		return {};
	if(est.hoursRemaining > 500.0)
		return {};

	double totalMinutes = est.hoursRemaining * 60.0;
	int    h            = static_cast<int>(totalMinutes) / 60;
	int    m            = static_cast<int>(totalMinutes) % 60;

	if(h > 0)
		return QString("~exhausted in %1h %2m").arg(h).arg(m);
	if(m > 0)
		return QString("~exhausted in %1m").arg(m);
	return "~exhausted soon";
}

inline QColor utilizationColor(double utilization)
{
	if(utilization < 50.0)  return QColor(76, 175, 80);   // Green
	if(utilization < 80.0)  return QColor(255, 193, 7);   // Amber/Yellow
	                        return QColor(244, 67, 54);   // Red
}

inline QString barColorStyleSheet(double utilization)
{
	QString color;
	if(utilization < 50.0)
		color = "#4CAF50"; // Green
	else if(utilization < 80.0)
		color = "#FFC107"; // Amber
	else
		color = "#F44336"; // Red

	return QString("QProgressBar::chunk { background-color: %1; border-radius: 3px; }").arg(color);
}

} // namespace FormatUtils
