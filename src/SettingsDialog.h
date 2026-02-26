#pragma once

#include <QDialog>

class CredentialManager;

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent, CredentialManager *credentialManager);
	~SettingsDialog();

	int pollingIntervalSeconds() const;

signals:
	void pollingIntervalChanged(int seconds);

public slots:
	void accept() override;

private:
	Ui::SettingsDialog *ui;
	CredentialManager  *credentialManager;
};
