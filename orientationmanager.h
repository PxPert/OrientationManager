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

#ifndef ORIENTATIONMANAGER_H
#define ORIENTATIONMANAGER_H


// #include <Plasma/Applet>
#include "orientationenums.h"
#include <QDebug>

class QTimer;
class AccellerometerController;
class RawInputHandler;
class QSystemTrayIcon;

class OrientationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString accellerometerName READ accellerometerName WRITE setAccellerometerName)
    Q_PROPERTY(QString inputTabletModeFile READ inputTabletModeFile WRITE setInputTabletModeFile)
    Q_PROPERTY(int pollingInterval READ pollingInterval WRITE setPollingInterval)
    Q_PROPERTY(int notifyDelay READ notifyDelay WRITE setNotifyDelay)
    Q_PROPERTY(OrientationEnums::OrientationPositions currentOrientation READ currentOrientation NOTIFY currentOrientationChanged)
    Q_PROPERTY(OrientationEnums::TabletModes currentTabletMode READ currentTabletMode NOTIFY currentTabletModeChanged)
    Q_PROPERTY(bool orientationLocked READ orientationLocked WRITE setOrientationLocked NOTIFY orientationLockedChanged)
    Q_PROPERTY(QString programToStart READ programToStart WRITE setProgramToStart NOTIFY programToStartChanged)

    
public:
    OrientationManager( QObject *parent );
    virtual ~OrientationManager();

    QString accellerometerName() const
    {
        return m_accellerometerName;
    }

    int pollingInterval() const
    {
        return m_pollingInterval;
    }

    int notifyDelay() const
    {
        return m_notifyDelay;
    }

    OrientationEnums::OrientationPositions currentOrientation() const
    {
        return m_currentOrientation;
    }

    OrientationEnums::TabletModes currentTabletMode() const
    {
        return m_currentTabletMode;
    }
    QString inputTabletModeFile() const
    {
        return m_inputTabletModeFile;
    }

    bool orientationLocked() const
    {
        return m_orientationLocked;
    }

    QString programToStart() const
    {
        return m_programToStart;
    }

signals:


    void currentTabletModeChanged(OrientationEnums::TabletModes currentTabletMode);
    void currentOrientationChanged(OrientationEnums::OrientationPositions currentOrientation);
    void currentModesChanged(OrientationEnums::OrientationPositions currentOrientation, OrientationEnums::TabletModes currentTabletMode);

    void orientationLockedChanged(bool orientationLocked);

    void programToStartChanged(QString programToStart);

public Q_SLOTS:
    void saveSettings();

    void setAccellerometerName(QString accellerometerName)
    {
        if (m_accellerometerName != accellerometerName)
        {
            m_accellerometerName = accellerometerName;
            initializeAccellerometer();
            startAccellerometer();
        }
    }

    void setPollingInterval(int pollingInterval)
    {
        m_pollingInterval = pollingInterval;
        startAccellerometer();
    }

    void setNotifyDelay(int notifyDelay)
    {
        m_notifyDelay = notifyDelay;
        startAccellerometer();
    }

    void setInputTabletModeFile(QString inputTabletModeFile)
    {
        if (m_inputTabletModeFile == inputTabletModeFile)
            return;
        
        m_inputTabletModeFile = inputTabletModeFile;
        startRawInputHandler();
    }

    void setOrientationLocked(bool orientationLocked)
    {
        if (m_orientationLocked == orientationLocked)
            return;

        m_orientationLocked = orientationLocked;
        emit orientationLockedChanged(orientationLocked);
        startTmrAccellerometer();
        
    }

    void setProgramToStart(QString programToStart)
    {
        if (m_programToStart == programToStart)
            return;

        m_programToStart = programToStart;
        emit programToStartChanged(programToStart);
    }

    QString getOrientationChangedMsg(OrientationEnums::OrientationPositions orientation);
    QString getOrientationChangedMsg(OrientationEnums::TabletModes mode);
    QString getOrientationChangedMsg(int orientation, int mode);

    void updateOrientation();
protected slots:
    /*
    void accellerometer_currentOrientationChanged(OrientationPositions orientation);
    void rawInputHandler_currentModeChanged(TabletModes currentMode);
    */
    void startTmrAccellerometer();
    void tmrAccellerometer_timeout();

    void lockUnlock();

    void sendErrorNotification(const QString& error);
    void sendOrientationChangedNotification(QString text);
    
//    void deinitialize();
private:    
    QTimer* _tmrAccellerometer;
    AccellerometerController* _accellerometer;
    RawInputHandler* _rawInputHandler;
    QString m_accellerometerName;
    QSystemTrayIcon* _tray;
    int m_pollingInterval;
    int m_notifyDelay;

    void loadSettings();

    void showSettings();
    void createTray();
    void initializeAccellerometer();
    void startAccellerometer();
    void startRawInputHandler();
    OrientationEnums::OrientationPositions m_currentOrientation;
    bool m_tabletMode;
    OrientationEnums::TabletModes m_currentTabletMode;
    QString m_inputTabletModeFile;
    bool m_orientationLocked;
    QString m_programToStart;

};

#endif
