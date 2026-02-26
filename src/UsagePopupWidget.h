#pragma once

#include <QWidget>
#include "UsageData.h"
#include "AntiGravityData.h"
#include "UsageRateTracker.h"

class QLabel;
class QProgressBar;

namespace Ui
{
class UsagePopupWidget;
}

struct AgModelRow
{
	QLabel       *nameLabel    = nullptr;
	QLabel       *percentLabel = nullptr;
	QProgressBar *bar          = nullptr;
	QLabel       *resetLabel   = nullptr;
};

class UsagePopupWidget : public QWidget
{
	Q_OBJECT

public:
	explicit UsagePopupWidget(QWidget *parent = nullptr);
	~UsagePopupWidget();

	void updateUsage(const UsageInfo &info, const UsageRateInfo &rates);
	void updateAntiGravity(const AntiGravityInfo &info, const AntiGravityRateInfo &rates);
	void setAntiGravityVisible(bool visible);
	void setShowRateEstimates(bool show) { m_showRateEstimates = show; }

	bool hasUserPosition() const { return m_userMoved; }
	void setUserPosition(const QPoint &pos) { move(pos); m_userMoved = true; }
	void clearUserPosition() { m_userMoved = false; }

signals:
	void resetPositionRequested();
	void hiddenByDeactivation();

protected:
	bool event(QEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	void    updateBar(QProgressBar *bar, QLabel *percentLabel, QLabel *resetLabel, const UsageBucket &bucket, const RateEstimate &rate);
	AgModelRow buildModelRow(const QString &modelName);

	Ui::UsagePopupWidget *ui;
	QList<AgModelRow>     agModelRows;
	bool                  m_pinned             = false;
	bool                  m_dragging           = false;
	bool                  m_userMoved          = false;
	bool                  m_showRateEstimates  = false;
	QPoint                m_dragOffset;
};
