#include "stdafx.h"
#include "UsagePopupWidget.h"
#include "ui_UsagePopupWidget.h"
#include "FormatUtils.h"
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QProgressBar>
#include <QScreen>
#include <QGraphicsOpacityEffect>
#include <QVBoxLayout>

UsagePopupWidget::UsagePopupWidget(QWidget *parent)
	: QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool), ui(new Ui::UsagePopupWidget)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_ShowWithoutActivating, false);

	// Hide AntiGravity section until data arrives
	ui->agSeparator->setVisible(false);
	ui->antiGravitySection->setVisible(false);

	// Set up opacity effect for pin button — dim when unpinned, full when pinned
	auto *pinOpacity = new QGraphicsOpacityEffect(ui->pinButton);
	pinOpacity->setOpacity(0.35);
	ui->pinButton->setGraphicsEffect(pinOpacity);

	connect(ui->pinButton, &QPushButton::toggled, this, [this, pinOpacity](bool checked) {
		m_pinned = checked;
		ui->pinButton->setText(checked ? "📌" : "📍");
		ui->pinButton->setToolTip(checked ? "Unpin window" : "Keep window open");
		pinOpacity->setOpacity(checked ? 1.0 : 0.35);
	});
}

UsagePopupWidget::~UsagePopupWidget()
{
	delete ui;
}

void UsagePopupWidget::updateUsage(const UsageInfo &info, const UsageRateInfo &rates)
{
	updateBar(ui->fiveHourBar, ui->fiveHourPercentLabel, ui->fiveHourResetLabel, info.fiveHour, rates.fiveHour);
	updateBar(ui->sevenDayBar, ui->sevenDayPercentLabel, ui->sevenDayResetLabel, info.sevenDay, rates.sevenDay);
	updateBar(ui->opusBar, ui->opusPercentLabel, ui->opusResetLabel, info.sevenDayOpus, rates.sevenDayOpus);

	// Hide Opus row if utilization is 0 and no reset time
	bool showOpus = info.sevenDayOpus.utilization > 0.0 || info.sevenDayOpus.hasResetTime;
	ui->opusLabel->setVisible(showOpus);
	ui->opusPercentLabel->setVisible(showOpus);
	ui->opusBar->setVisible(showOpus);
	ui->opusResetLabel->setVisible(showOpus);
}

bool UsagePopupWidget::event(QEvent *e)
{
	if(e->type() == QEvent::WindowDeactivate && !m_pinned)
	{
		hide();
		emit hiddenByDeactivation();
	}
	return QWidget::event(e);
}

void UsagePopupWidget::keyPressEvent(QKeyEvent *e)
{
	if(e->key() == Qt::Key_Escape)
		hide();
	else
		QWidget::keyPressEvent(e);
}

void UsagePopupWidget::mousePressEvent(QMouseEvent *e)
{
	if(e->button() == Qt::LeftButton && ui->titleLabel->geometry().contains(e->pos()))
	{
		m_dragging   = true;
		m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
		e->accept();
		return;
	}
	QWidget::mousePressEvent(e);
}

void UsagePopupWidget::mouseMoveEvent(QMouseEvent *e)
{
	if(m_dragging && (e->buttons() & Qt::LeftButton))
	{
		move(e->globalPosition().toPoint() - m_dragOffset);
		m_userMoved = true;
		e->accept();
		return;
	}
	QWidget::mouseMoveEvent(e);
}

void UsagePopupWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if(e->button() == Qt::LeftButton && m_dragging)
	{
		m_dragging = false;
		e->accept();
		return;
	}
	if(e->button() == Qt::MiddleButton && m_userMoved)
	{
		emit resetPositionRequested();
		e->accept();
		return;
	}
	QWidget::mouseReleaseEvent(e);
}

void UsagePopupWidget::updateBar(QProgressBar *bar, QLabel *percentLabel, QLabel *resetLabel,
                                 const UsageBucket &bucket, const RateEstimate &rate)
{
	double clamped = qMin(bucket.utilization, 100.0);
	bar->setValue(static_cast<int>(clamped * 10)); // max is 1000 for 100.0%
	bar->setStyleSheet(FormatUtils::barColorStyleSheet(clamped));

	percentLabel->setText(QString("%1%").arg(bucket.utilization, 0, 'f', 1));

	if(bucket.hasResetTime)
	{
		QString relative   = FormatUtils::timeUntilReset(bucket.resetsAt);
		QString absolute   = bucket.resetsAt.toLocalTime().toString("ddd H:mm");
		QString text       = QString("Resets %1 (%2)").arg(relative, absolute);
		if(m_showRateEstimates)
		{
			QString exhaustion = FormatUtils::exhaustionLabel(rate);
			if(!exhaustion.isEmpty())
				text += " • " + exhaustion;
		}
		resetLabel->setText(text);
	}
	else
	{
		resetLabel->setText(m_showRateEstimates ? FormatUtils::exhaustionLabel(rate) : QString());
	}
}

AgModelRow UsagePopupWidget::buildModelRow(const QString &modelName)
{
	AgModelRow row;

	// Name label
	row.nameLabel = new QLabel(modelName, this);
	row.nameLabel->setStyleSheet("color: #e0e0e0; font-size: 12px;");

	// Percent label (right-aligned)
	row.percentLabel = new QLabel("--", this);
	row.percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	row.percentLabel->setStyleSheet("color: #e0e0e0; font-size: 12px;");

	// Header row (name + percent)
	QHBoxLayout *headerLayout = new QHBoxLayout;
	headerLayout->addWidget(row.nameLabel);
	headerLayout->addWidget(row.percentLabel);

	// Progress bar
	row.bar = new QProgressBar(this);
	row.bar->setMinimum(0);
	row.bar->setMaximum(1000);
	row.bar->setValue(0);
	row.bar->setTextVisible(false);
	row.bar->setStyleSheet(FormatUtils::barColorStyleSheet(0.0));

	// Reset label
	row.resetLabel = new QLabel("", this);
	row.resetLabel->setStyleSheet("color: #888; font-size: 11px;");

	// Assemble into the agModelRowsContainer layout
	QVBoxLayout *rowsLayout = qobject_cast<QVBoxLayout *>(ui->agModelRowsContainer->layout());
	if(rowsLayout)
	{
		rowsLayout->addLayout(headerLayout);
		rowsLayout->addWidget(row.bar);
		rowsLayout->addWidget(row.resetLabel);
	}

	return row;
}

void UsagePopupWidget::updateAntiGravity(const AntiGravityInfo &info, const AntiGravityRateInfo &rates)
{
	if(!info.isAvailable)
	{
		setAntiGravityVisible(false);
		return;
	}

	// Rebuild rows if the model count changed
	if(agModelRows.size() != info.modelQuotas.size())
	{
		// Clear existing dynamic rows
		QLayout     *layout = ui->agModelRowsContainer->layout();
		QLayoutItem *item;
		while((item = layout->takeAt(0)) != nullptr)
		{
			if(item->layout())
			{
				QLayoutItem *child;
				while((child = item->layout()->takeAt(0)) != nullptr)
				{
					delete child->widget();
					delete child;
				}
			}
			delete item->widget();
			delete item;
		}
		agModelRows.clear();

		for(const AntiGravityModelQuota &quota : info.modelQuotas)
			agModelRows.append(buildModelRow(quota.modelName));
	}

	// Update values
	for(int i = 0; i < agModelRows.size() && i < info.modelQuotas.size(); ++i)
	{
		const AntiGravityModelQuota &quota = info.modelQuotas[i];
		AgModelRow                  &row   = agModelRows[i];

		double clamped = qMin(quota.utilization, 100.0);
		row.bar->setValue(static_cast<int>(clamped * 10));
		row.bar->setStyleSheet(FormatUtils::barColorStyleSheet(clamped));
		row.percentLabel->setText(QString("%1%").arg(quota.utilization, 0, 'f', 1));

		RateEstimate est = rates.value(quota.modelName);

		if(quota.hasResetTime)
		{
			QString relative = FormatUtils::timeUntilReset(quota.resetsAt);
			QString absolute = quota.resetsAt.toLocalTime().toString("ddd H:mm");
			QString text     = QString("Resets %1 (%2)").arg(relative, absolute);
			if(m_showRateEstimates)
			{
				QString exhaustion = FormatUtils::exhaustionLabel(est);
				if(!exhaustion.isEmpty())
					text += " • " + exhaustion;
			}
			row.resetLabel->setText(text);
		}
		else
		{
			row.resetLabel->setText(m_showRateEstimates ? FormatUtils::exhaustionLabel(est) : QString());
		}
	}

	setAntiGravityVisible(true);
	adjustSize();
}

void UsagePopupWidget::setAntiGravityVisible(bool visible)
{
	ui->agSeparator->setVisible(visible);
	ui->antiGravitySection->setVisible(visible);
	adjustSize();
}
