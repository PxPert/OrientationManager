/*
 *   Copyright (C) %{CURRENT_YEAR} by %{AUTHOR} <%{EMAIL}>                      *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "orientationmanager.h"
// #include <klocalizedstring.h>
#include <QTimer>
#include "accellerometercontroller.h"
#include <QDebug>
#include "rawinputhandler.h"

#include <QHash>
#include <QTimer>
#include <QProcess>
#include <QFile>
#include <QSystemTrayIcon>
#include <QAction>
#include "settingsdialog.h"
#include <QApplication>
#include <QMenu>
#include <QSettings>


#define COMPONENT_NAME "Orientation Manager"

OrientationManager::OrientationManager(QObject *parent = NULL)
    : QObject(parent)
{
    qDebug() << "Creating Orientation Manager";
    _tray = new QSystemTrayIcon();
    createTray();
    // OrientationEnums::declareQML();
    _tmrAccellerometer = new QTimer(this);
    _tmrAccellerometer->setSingleShot(true);
    connect (_tmrAccellerometer,SIGNAL(timeout()),this,SLOT(tmrAccellerometer_timeout()));
    _accellerometer = NULL;
    _rawInputHandler = NULL;
    m_pollingInterval = 0;
    m_notifyDelay = 0;
    m_orientationLocked = false;
    m_currentOrientation = OrientationEnums::ORIENTATION_INVALID;
    m_currentTabletMode = OrientationEnums::MODE_INVALID;
    loadSettings();



}


QString OrientationManager::getOrientationChangedMsg(int orientation, int mode)
{
    return QString("%1\n%2").arg(getOrientationChangedMsg((OrientationEnums::OrientationPositions)orientation)).arg(getOrientationChangedMsg((OrientationEnums::TabletModes)mode));
}

QString OrientationManager::getOrientationChangedMsg(OrientationEnums::OrientationPositions orientation)
{
    static QHash<OrientationEnums::OrientationPositions,QString> messages;

    if (! messages.count()){
        messages.insert(OrientationEnums::ORIENTATION_INVALID,tr("Invalid orientation detected"));
        messages.insert(OrientationEnums::ORIENTATION_FLAT,tr("Orientation set to Flat"));
        messages.insert(OrientationEnums::ORIENTATION_TOP,tr("Orientation set to Top"));
        messages.insert(OrientationEnums::ORIENTATION_RIGHT,tr("Orientation set to Right"));
        messages.insert(OrientationEnums::ORIENTATION_BOTTOM,tr("Orientation set to Bottom"));
        messages.insert(OrientationEnums::ORIENTATION_LEFT,tr("Orientation set to Left"));
    }

    return messages.value(orientation);
}

QString OrientationManager::getOrientationChangedMsg(OrientationEnums::TabletModes mode)
{
    static QHash<OrientationEnums::TabletModes,QString> messages;

    if (! messages.count()){
        messages.insert(OrientationEnums::MODE_INVALID,tr("Invalid Tablet mode detected"));
        messages.insert(OrientationEnums::MODE_TABLET,tr("Laptop set to Tablet mode"));
        messages.insert(OrientationEnums::MODE_NOTEBOOK,tr("Laptop set to Standard position"));
    }

    return messages.value(mode);
}

void OrientationManager::startTmrAccellerometer()
{
    if (!orientationLocked()) {
        _tmrAccellerometer->start();
    }
}

void OrientationManager::tmrAccellerometer_timeout()
{
    updateOrientation();
}

void OrientationManager::updateOrientation()
{
    if (!_accellerometer || orientationLocked())
        return;

    OrientationEnums::OrientationPositions currentOrientation = _accellerometer->currentOrientation();

    //qDebug() << "Ero su: " << m_currentOrientation << " - Adesso su: " << currentOrientation;
    bool modeChanged = false;
    bool orientationChanged = false;
    if (m_currentOrientation != currentOrientation)
    {
        orientationChanged = true;
        m_currentOrientation = currentOrientation;
        emit currentOrientationChanged(currentOrientation);
    }

    OrientationEnums::TabletModes currentTabletMode = _rawInputHandler->currentMode();

    if (m_currentTabletMode != currentTabletMode) {
        modeChanged = true;
        m_currentTabletMode = currentTabletMode;
        emit currentTabletModeChanged(currentTabletMode);
    }

    if (modeChanged || orientationChanged) {
        QString notificationText;
        if (modeChanged && orientationChanged) {
            notificationText = getOrientationChangedMsg(currentOrientation,currentTabletMode);
        } else if (modeChanged) {
            notificationText = getOrientationChangedMsg(currentTabletMode);
        } else {
            notificationText = getOrientationChangedMsg(currentOrientation);
        }

        sendOrientationChangedNotification(notificationText);
        emit currentModesChanged(currentOrientation,currentTabletMode);
        QStringList args;
        args.append(QString::number(currentOrientation));
        args.append(QString::number(currentTabletMode));
        if ((!programToStart().isEmpty()) && (QFile::exists(programToStart())))
        {
            QProcess::execute(programToStart(),args); 
        } else {
            if (programToStart().isEmpty()) {
                sendErrorNotification(tr("Script to start not set"));
            } else {
                sendErrorNotification(QString(tr("Unable to find the script: %1")).arg(programToStart()));
            }
        }
    }
    return;
}


void OrientationManager::lockUnlock()
{
    QAction* senderAct = (QAction*)QObject::sender();

    setOrientationLocked(! orientationLocked());
    senderAct->setChecked(orientationLocked());
    if (orientationLocked()) {
        _tray->showMessage(COMPONENT_NAME,tr("Orientation manually locked"));
    } else {
        _tray->showMessage(COMPONENT_NAME,tr("Orientation unlocked"));
    }
}

void OrientationManager::sendErrorNotification(const QString &error)
{
    _tray->showMessage(tr("Orientation Manager error"),error);
}

void OrientationManager::sendOrientationChangedNotification(QString text)
{
    QString descriptionImage = ":/images/question.svg";
    QSystemTrayIcon::MessageIcon notifyIcon = (currentOrientation() == OrientationEnums::ORIENTATION_INVALID)?QSystemTrayIcon::Warning:QSystemTrayIcon::Information;

    switch (currentOrientation()) {
    case OrientationEnums::ORIENTATION_FLAT:
    case OrientationEnums::ORIENTATION_TOP:
    case OrientationEnums::ORIENTATION_BOTTOM:
        if (currentTabletMode() == OrientationEnums::MODE_TABLET)  {
            descriptionImage = ":/images/tablet.svg";
        } else {
            descriptionImage = ":/images/computer-laptop.svg";
        }
        break;
    case OrientationEnums::ORIENTATION_LEFT:
    case OrientationEnums::ORIENTATION_RIGHT:
        descriptionImage = ":/images/smartphone.svg";
        break;
    default:
        break;
    }


    _tray->setIcon(QPixmap(descriptionImage));
    _tray->showMessage(tr("Orientation changed"),text,notifyIcon,5000);
}

void OrientationManager::loadSettings()
{
    QSettings s;
    setAccellerometerName(s.value("AccellName", "accel_3d").toString());
    setProgramToStart(s.value("ApplicationPath","").toString());
    setNotifyDelay(s.value("NotifyDelay", 2000).toInt());
    setPollingInterval(s.value("PollingInterval", 1000).toInt());
    setInputTabletModeFile(s.value("TabletModeButton","").toString());
}

void OrientationManager::saveSettings()
{
    QSettings s;
    s.setValue("AccellName",accellerometerName());
    s.setValue("ApplicationPath",programToStart());
    s.setValue("NotifyDelay",notifyDelay());
    s.setValue("PollingInterval",pollingInterval());
    s.setValue("TabletModeButton",inputTabletModeFile());
    s.sync();
}


void OrientationManager::showSettings()
{
    SettingsDialog s(this);
    qDebug() << "mostro settings";
    s.setModal(true);
    s.exec();
    qDebug() << "Mostrati";

}

void OrientationManager::createTray()
{
    _tray->setIcon(QPixmap(":/images/computer-laptop.svg"));
    _tray->setVisible(true);

    QAction* lockAct = new QAction(tr("L&ock"), this);
    lockAct->setCheckable(true);
    lockAct->setStatusTip(tr("Lock the current orientation"));
    connect(lockAct, SIGNAL(triggered), this, SLOT(lockUnlock()));

    QAction* settingsAct = new QAction(tr("S&ettings"), this);
    settingsAct->setShortcuts(QKeySequence::Preferences);
    settingsAct->setStatusTip(tr("Open the settings window"));
    connect(settingsAct, &QAction::triggered, this, &OrientationManager::showSettings);

    QAction* exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, &QAction::triggered, this, QApplication::quit);

    QMenu* menu = new QMenu();
    menu->addAction(lockAct);
    menu->addAction(settingsAct);
    menu->addSeparator();
    menu->addAction(exitAct);

    _tray->setContextMenu(menu);
    _tray->show();
}

void OrientationManager::initializeAccellerometer()
{
    if (_accellerometer) {
        _accellerometer->deleteLater();
    }
    _accellerometer = new AccellerometerController(this->accellerometerName(),this);
    QString errore;
    if ( !_accellerometer->initialize(&errore)) {
        sendErrorNotification(errore);
    } else {
        m_currentOrientation = OrientationEnums::ORIENTATION_TOP;
    }
    connect(_accellerometer,SIGNAL(currentOrientationChanged(OrientationEnums::OrientationPositions)),this,SLOT(startTmrAccellerometer()));

}

void OrientationManager::startAccellerometer()
{
    if (! (
            (_accellerometer)
            &&
            (pollingInterval() > 0)
            &&
            (notifyDelay() > 0)
    )) {
        return;
    }
    _tmrAccellerometer->setInterval(notifyDelay());
    _accellerometer->setPollingIntervalMsec(pollingInterval());

}

void OrientationManager::startRawInputHandler()
{
    if (_rawInputHandler) {
        delete (_rawInputHandler);
    }
    _rawInputHandler = new RawInputHandler(this);
    QString error;
    connect(_rawInputHandler,SIGNAL(currentModeChanged(OrientationEnums::TabletModes)),this,SLOT(startTmrAccellerometer()));

    _rawInputHandler->initialize(inputTabletModeFile(),&error);

    if (! error.isEmpty()) {
        sendErrorNotification(error);
    } else {
        m_currentTabletMode = OrientationEnums::MODE_NOTEBOOK;
    }

}

OrientationManager::~OrientationManager()
{
    qDebug() << "Destroying orientationmanager";
}



// K_EXPORT_PLASMA_APPLET_WITH_JSON(orientationmanager, OrientationManager, "metadata.json")
