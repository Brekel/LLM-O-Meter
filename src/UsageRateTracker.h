#pragma once

#include <QDateTime>
#include <QHash>
#include <deque>

struct UsageObservation
{
	QDateTime timestamp;
	double    utilization = 0.0;
};

struct RateEstimate
{
	bool   valid               = false;
	double ratePerHour         = 0.0;   // % per hour; positive = consuming
	bool   recovering          = false; // rate < 0 (utilization falling)
	double hoursRemaining      = -1.0;  // (100 - current) / rate; -1 if N/A
	bool   exhaustsBeforeReset = false; // true only when actionable
};

struct UsageRateInfo
{
	RateEstimate fiveHour;
	RateEstimate sevenDay;
	RateEstimate sevenDayOpus;
};

using AntiGravityRateInfo = QHash<QString, RateEstimate>;

class UsageRateTracker
{
public:
	// Maximum observations to retain per bucket.
	static constexpr int    MaxObservations    = 10;
	// Observations older than this are discarded before calculation.
	static constexpr double MaxWindowHours     = 2.0;
	// A utilization drop larger than this triggers a history reset (quota renewed).
	static constexpr double ResetDropThreshold = 20.0;

	// Record a new observation for the given bucket key.
	// Claude bucket keys: "claude_5h", "claude_7d", "claude_opus"
	// AntiGravity keys: the model's modelName string
	void addObservation(const QString &key, double utilization, const QDateTime &timestamp);

	// Compute a rate estimate for the named bucket.
	// Returns RateEstimate{valid=false} when there is insufficient history.
	RateEstimate estimate(const QString &key, const QDateTime &now,
	                      const QDateTime &resetsAt, bool hasResetTime) const;

	// Discard all history across all buckets (call after token/account change).
	void clearAll();

private:
	static void pruneHistory(std::deque<UsageObservation> &history, const QDateTime &now);

	QHash<QString, std::deque<UsageObservation>> m_history;
};
