#ifndef ACCELLEROMETERCONTROLLER_H
#define ACCELLEROMETERCONTROLLER_H
#include <QObject>
#include <QString>
#include <QMetaEnum>

#include "iio_utils.h"
#include <poll.h>
#include "orientationenums.h"

class QTimer;

class AccellerometerController : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString deviceName READ deviceName)
    Q_PROPERTY(int pollingIntervalMsec READ pollingIntervalMsec WRITE setPollingIntervalMsec)
    Q_PROPERTY(OrientationEnums::OrientationPositions currentOrientation READ currentOrientation WRITE setCurrentOrientation NOTIFY currentOrientationChanged)

public:

    explicit AccellerometerController(QString device, QObject *parent = 0);

    ~AccellerometerController();
    bool initialize(QString* toError);

    QString deviceName() const
    {
        return QString::fromUtf8(m_deviceNameBA);
    }

    OrientationEnums::OrientationPositions currentOrientation() const
    {
        return m_currentOrientation;
    }

    int pollingIntervalMsec() const
    {
        return m_pollingIntervalMsec;
    }

public slots:

    void setPollingIntervalMsec(int pollingIntervalMsec);

signals:

    void currentOrientationChanged(OrientationEnums::OrientationPositions currentOrientation);

protected slots:
    void setCurrentOrientation(OrientationEnums::OrientationPositions currentOrientation)
    {
        if (m_currentOrientation == currentOrientation)
            return;

        m_currentOrientation = currentOrientation;
        emit currentOrientationChanged(currentOrientation);
    }

    void tmrPolling_timeout();

private:
    QTimer* _tmrPolling;

    bool openDevice(QString* toError);
    bool closeDevice();

    int enable_sensors(const char *device_dir);
    /**
     * find_type_by_name() - function to match top level types by name
     * @name: top level type instance name
     * @type: the type of top level instance being sort
     *
     * Typical types this is used for are device and trigger.
     **/
    int find_type_by_name(const char *name, const char *type);

    OrientationEnums::OrientationPositions process_scan(SensorData data, Device_info info);
    void process_scan_3(char *data, struct iio_channel_info *channels, int num_channels,
            const char *ch_name_1, int *ch_val_1, bool *ch_present_1,
            const char *ch_name_2, int *ch_val_2, bool *ch_present_2,
            const char *ch_name_3, int *ch_val_3, bool *ch_present_3);
    void process_scan_1(char *data, struct iio_channel_info *channels, int num_channels,
            const char *ch_name, int *ch_val, bool *ch_present);

    OrientationEnums::OrientationPositions prepare_output();


    QByteArray m_deviceNameBA;

    QByteArray m_dev_dir_name;
    QByteArray m_trigger_name;
    Device_info m_device;

    struct pollfd m_pfd;
    int m_fp;
    OrientationEnums::OrientationPositions m_currentOrientation;
    int m_pollingIntervalMsec;
};

#endif // ACCELLEROMETERCONTROLLER_H
