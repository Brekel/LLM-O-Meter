#include "stdafx.h"
#include "SettingsDialog.h"
#include "CredentialManager.h"
#include "ui_SettingsDialog.h"
#include <QLineEdit>
#include <QSpinBox>

SettingsDialog::SettingsDialog(QWidget *parent, CredentialManager *credentialManager)
	: QDialog(parent), ui(new Ui::SettingsDialog), credentialManager(credentialManager)
{
	ui->setupUi(this);

	// Context-aware info label
	if(credentialManager->hasCredentialsFile())
		ui->infoLabel->setText("Using token from Claude Code credentials file. Override below if needed");
	else
		ui->infoLabel->setText("Could not find Claude Code credentials file. Paste your OAuth bearer token below:");

	// Load saved polling interval
	ui->pollingSpinBox->setValue(credentialManager->loadPollingInterval());

	// Load monitor enabled states
	ui->claudeEnabledCheckBox->setChecked(credentialManager->loadClaudeEnabled());
	ui->agEnabledCheckBox->setChecked(credentialManager->loadAntiGravityEnabled());

	// Load tray icon settings
	ui->hideZeroIconsCheckBox->setChecked(credentialManager->loadHideZeroIcons());
	ui->showRateEstimatesCheckBox->setChecked(credentialManager->loadShowRateEstimates());

	// Load notification settings
	bool notifyEnabled = credentialManager->loadNotifyEnabled();
	ui->notifyEnabledCheckBox->setChecked(notifyEnabled);
	ui->notifySubWidget->setVisible(notifyEnabled);
	ui->notifyBelowThresholdCheckBox->setChecked(credentialManager->loadNotifyBelowThreshold());
	ui->notifyThresholdSpinBox->setValue(credentialManager->loadNotifyThresholdPercent());
	ui->notifyExhaustedCheckBox->setChecked(credentialManager->loadNotifyExhausted());
	ui->notifyRenewedCheckBox->setChecked(credentialManager->loadNotifyRenewed());

	connect(ui->notifyEnabledCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
		ui->notifySubWidget->setVisible(checked);
		adjustSize();
	});

	// Pre-fill token if there's a manual override saved
	if(!credentialManager->hasCredentialsFile())
	{
		QString savedToken = credentialManager->loadToken();
		if(!savedToken.isEmpty())
			ui->tokenEdit->setText(savedToken);
	}
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

int SettingsDialog::pollingIntervalSeconds() const
{
	return ui->pollingSpinBox->value();
}

void SettingsDialog::accept()
{
	QString token = ui->tokenEdit->text().trimmed();

	// Save token if provided
	if(!token.isEmpty())
		credentialManager->saveManualToken(token);

	// Save polling interval
	int interval = ui->pollingSpinBox->value();
	credentialManager->savePollingInterval(interval);
	emit pollingIntervalChanged(interval);

	// Save monitor enabled states
	credentialManager->saveClaudeEnabled(ui->claudeEnabledCheckBox->isChecked());
	credentialManager->saveAntiGravityEnabled(ui->agEnabledCheckBox->isChecked());

	// Save tray icon settings
	credentialManager->saveHideZeroIcons(ui->hideZeroIconsCheckBox->isChecked());
	credentialManager->saveShowRateEstimates(ui->showRateEstimatesCheckBox->isChecked());

	// Save notification settings
	credentialManager->saveNotifyEnabled(ui->notifyEnabledCheckBox->isChecked());
	credentialManager->saveNotifyBelowThreshold(ui->notifyBelowThresholdCheckBox->isChecked());
	credentialManager->saveNotifyThresholdPercent(ui->notifyThresholdSpinBox->value());
	credentialManager->saveNotifyExhausted(ui->notifyExhaustedCheckBox->isChecked());
	credentialManager->saveNotifyRenewed(ui->notifyRenewedCheckBox->isChecked());

	QDialog::accept();
}
