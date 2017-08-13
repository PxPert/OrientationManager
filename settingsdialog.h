#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
class OrientationManager;

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(OrientationManager *orientationManager = 0);
    ~SettingsDialog();

private slots:
    void on_buttonBox_accepted();

private:
    void loadSettings();
    OrientationManager* _orientationManager;
    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
