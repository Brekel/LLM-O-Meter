#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

struct AntiGravityModelQuota
{
	QString   modelName;
	double    utilization  = 0.0; // 0.0–100.0
	QDateTime resetsAt;
	bool      hasResetTime = false;
};

struct AntiGravityInfo
{
	bool    isAvailable = false;
	QString userTier; // "Free", "Pro", "Ultra"
	QList<AntiGravityModelQuota> modelQuotas;
};
