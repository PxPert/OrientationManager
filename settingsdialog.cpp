#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "orientationmanager.h"
SettingsDialog::SettingsDialog(OrientationManager* orientationManager) :
    QDialog(),
    ui(new Ui::SettingsDialog)
{
    _orientationManager = orientationManager;
    ui->setupUi(this);
    loadSettings();

}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::loadSettings()
{
    ui->sbNotify->setValue(_orientationManager->notifyDelay());
    ui->sbPolling->setValue(_orientationManager->pollingInterval());
    ui->lnAccellerometer->setText(_orientationManager->accellerometerName());
    ui->lnIntelButton->setText(_orientationManager->inputTabletModeFile());
    ui->lnShellScript->setText(_orientationManager->programToStart());
}

void SettingsDialog::on_buttonBox_accepted()
{
    _orientationManager->setNotifyDelay(ui->sbNotify->value());
    _orientationManager->setPollingInterval(ui->sbPolling->value());
    _orientationManager->setAccellerometerName(ui->lnAccellerometer->text());
    _orientationManager->setInputTabletModeFile(ui->lnIntelButton->text());
    _orientationManager->setProgramToStart(ui->lnShellScript->text());
    _orientationManager->saveSettings();
}
