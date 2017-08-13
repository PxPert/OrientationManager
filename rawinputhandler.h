#ifndef RAWINPUTHANDLER_H
#define RAWINPUTHANDLER_H
#include <QObject>
#include <QDebug>
#include "orientationenums.h"

class QFile;
class QSocketNotifier;

class RawInputHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(OrientationEnums::TabletModes currentMode READ currentMode WRITE setCurrentMode NOTIFY currentModeChanged)
public:
    explicit RawInputHandler(QObject *parent = 0);
    ~RawInputHandler();

    OrientationEnums::TabletModes currentMode() const
    {
        return m_currentMode;
    }

signals:
    void currentModeChanged(OrientationEnums::TabletModes currentMode);

    void errorChanged(QString error);

public slots:

    bool initialize(const QString& path, QString* toError);

protected slots:

    void setCurrentMode(OrientationEnums::TabletModes currentMode)
    {
        if (m_currentMode == currentMode)
            return;

        m_currentMode = currentMode;
        emit currentModeChanged(currentMode);
    }
    void m_sn_activated(int);
private:

    QSocketNotifier* m_sn;
    int m_fp;
    OrientationEnums::TabletModes m_currentMode;
};

#endif // RAWINPUTHANDLER_H
