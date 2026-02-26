#include "stdafx.h"
#include "UsageRateTracker.h"

void UsageRateTracker::addObservation(const QString &key, double utilization, const QDateTime &timestamp)
{
	auto &history = m_history[key];

	// Detect quota renewal: a large utilization drop means the bucket reset.
	if(!history.empty() && (history.back().utilization - utilization) >= ResetDropThreshold)
	{
		qDebug() << "UsageRateTracker: reset detected for" << key
		         << "(" << history.back().utilization << "->" << utilization << ")";
		history.clear();
	}

	history.push_back({timestamp, utilization});
	pruneHistory(history, timestamp);
}

RateEstimate UsageRateTracker::estimate(const QString &key, const QDateTime &now,
                                        const QDateTime &resetsAt, bool hasResetTime) const
{
	auto it = m_history.find(key);
	if(it == m_history.end() || it->size() < 2)
		return {}; // not enough data

	// Work on a pruned copy so estimate() stays const and doesn't mutate state.
	std::deque<UsageObservation> window(it->begin(), it->end());
	pruneHistory(window, now);
	if(window.size() < 2)
		return {};

	const UsageObservation &oldest = window.front();
	const UsageObservation &newest = window.back();

	double deltaUtil  = newest.utilization - oldest.utilization;
	double deltaHours = static_cast<double>(oldest.timestamp.secsTo(newest.timestamp)) / 3600.0;

	if(deltaHours <= 0.0)
		return {}; // clock anomaly or duplicate timestamps

	double ratePerHour = deltaUtil / deltaHours;

	RateEstimate result;
	result.valid       = true;
	result.ratePerHour = ratePerHour;

	if(ratePerHour <= 0.0)
	{
		result.recovering = (ratePerHour < 0.0);
		return result; // valid but no exhaustion estimate
	}

	double hoursRemaining = (100.0 - newest.utilization) / ratePerHour;
	result.hoursRemaining = hoursRemaining;

	if(hasResetTime)
	{
		double hoursUntilReset     = static_cast<double>(now.secsTo(resetsAt)) / 3600.0;
		result.exhaustsBeforeReset = (hoursRemaining < hoursUntilReset);
	}
	else
	{
		// No reset time available — show estimate unconditionally (capped later by caller).
		result.exhaustsBeforeReset = true;
	}

	return result;
}

void UsageRateTracker::clearAll()
{
	m_history.clear();
}

void UsageRateTracker::pruneHistory(std::deque<UsageObservation> &history, const QDateTime &now)
{
	QDateTime cutoff = now.addSecs(-static_cast<qint64>(MaxWindowHours * 3600.0));

	// Remove observations outside the rolling time window, but always keep the oldest
	// one as the baseline for the first/last delta.
	while(history.size() > 1 && history.front().timestamp < cutoff)
		history.pop_front();

	// Enforce hard cap on count.
	while(static_cast<int>(history.size()) > MaxObservations)
		history.pop_front();
}
