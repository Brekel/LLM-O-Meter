#pragma once

#include <QDateTime>

struct UsageBucket
{
	double   utilization = 0.0;
	QDateTime resetsAt;
	bool     hasResetTime = false;
};

struct UsageInfo
{
	UsageBucket fiveHour;
	UsageBucket sevenDay;
	UsageBucket sevenDayOpus;
};
