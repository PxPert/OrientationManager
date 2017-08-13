#include "accellerometercontroller.h"

#include <QDebug>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <QTimer>

/**
 * process_scan_1() - get an integer value for a particular channel
 * @data:               pointer to the start of the scan
 * @channels:           information about the channels. Note
 *  size_from_channelarray must have been called first to fill the
 *  location offsets.
 * @num_channels:       number of channels
 * ch_name:		name of channel to get
 * ch_val:		value for the channel
 * ch_present:		whether the channel is present
 **/
void AccellerometerController::process_scan_1(char *data, struct iio_channel_info *channels, int num_channels,
        const char *ch_name, int *ch_val, bool *ch_present) {
    int k;
    for (k = 0; k < num_channels; k++) {
        if (0 == strcmp(channels[k].name, ch_name)) {
            switch (channels[k].bytes) {
                /* only a few cases implemented so far */
                case 2:
                    break;
                case 4:
                    if (!channels[k].is_signed) {
                        uint32_t val = *(uint32_t *) (data + channels[k].location);
                        val = val >> channels[k].shift;
                        if (channels[k].bits_used < 32) val &= ((uint32_t) 1 << channels[k].bits_used) - 1;
                        *ch_val = (int) val;
                        *ch_present = true;
                    } else {
                        int32_t val = *(int32_t *) (data + channels[k].location);
                        val = val >> channels[k].shift;
                        if (channels[k].bits_used < 32) val &= ((uint32_t) 1 << channels[k].bits_used) - 1;
                        val = (int32_t) (val << (32 - channels[k].bits_used)) >> (32 - channels[k].bits_used);
                        *ch_val = (int) val;
                        *ch_present = true;
                    }
                    break;
                case 8:
                    break;
            }
        }
    }
}

/**
 * process_scan_3() - get three integer values - see above
 **/
void AccellerometerController::process_scan_3(char *data, struct iio_channel_info *channels, int num_channels,
        const char *ch_name_1, int *ch_val_1, bool *ch_present_1,
        const char *ch_name_2, int *ch_val_2, bool *ch_present_2,
        const char *ch_name_3, int *ch_val_3, bool *ch_present_3) {
    process_scan_1(data, channels, num_channels, ch_name_1, ch_val_1, ch_present_1);
    process_scan_1(data, channels, num_channels, ch_name_2, ch_val_2, ch_present_2);
    process_scan_1(data, channels, num_channels, ch_name_3, ch_val_3, ch_present_3);
}


OrientationEnums::OrientationPositions AccellerometerController::process_scan(SensorData data, Device_info info) {
    OrientationEnums::OrientationPositions orientation = OrientationEnums::ORIENTATION_FLAT;
    int i;

    int accel_x, accel_y, accel_z;
    bool present_x, present_y, present_z;

    for (i = 0; i < data.read_size / data.scan_size; i++) {
        process_scan_3(data.data + data.scan_size*i, info.channels, info.channels_count,
                "in_accel_x", &accel_x, &present_x,
                "in_accel_y", &accel_y, &present_y,
                "in_accel_z", &accel_z, &present_z);
        /* Determine orientation */
        int accel_x_abs = abs(accel_x);
        int accel_y_abs = abs(accel_y);
        int accel_z_abs = abs(accel_z);
        /* printf("%u > %u && %u > %u\n", accel_z_abs, 4*accel_x_abs, accel_z_abs, 4*accel_y_abs); */
        if (accel_z_abs > 4 * accel_x_abs && accel_z_abs > 4 * accel_y_abs) {
            /* printf("set FLAT\n"); */
            orientation = OrientationEnums::ORIENTATION_FLAT;
        } else if (3 * accel_y_abs > 2 * accel_x_abs) {
            /* printf("set TOP/BOTTOM (%u, %u)\n", 3*accel_y_abs, 2*accel_x_abs); */
            orientation = accel_y > 0 ? OrientationEnums::ORIENTATION_BOTTOM : OrientationEnums::ORIENTATION_TOP;
        } else {
            orientation = accel_x > 0 ? OrientationEnums::ORIENTATION_LEFT : OrientationEnums::ORIENTATION_RIGHT;
            /* printf("set LEFT/RIGHT\n"); */
        }
        /*
        printf("Orientation %d, x:%5d, y:%5d, z:%5d \n",
                orientation, accel_x, accel_y, accel_z );
                */

    }
    return orientation;
}

AccellerometerController::AccellerometerController(QString device, QObject *parent) : QObject(parent)
{
    m_fp = -1;
    m_deviceNameBA = device.toUtf8();
    _tmrPolling = new QTimer(this);
    connect(_tmrPolling,SIGNAL(timeout()),this,SLOT(tmrPolling_timeout()));
    m_currentOrientation = OrientationEnums::ORIENTATION_INVALID;

}

AccellerometerController::~AccellerometerController()
{
    closeDevice();
}

bool AccellerometerController::initialize(QString *toError)
{
    if (m_fp > -1) {
        closeDevice();
    }
    /* Device info */
    Device_info info;
    const char* device_name = m_deviceNameBA.constData();

    /* Find the device requested */
    info.device_id = find_type_by_name(device_name, "iio:device");
    if (info.device_id < 0) {
        *toError = QString(tr("Device %1 not found")).arg(device_name);
        return false;
    }

    m_dev_dir_name=QString("%1iio:device%2").arg(iio_dir).arg(info.device_id).toUtf8();

    int ret = 0;

    enable_sensors(m_dev_dir_name.constData());

    m_trigger_name = QString("%1-dev%2").arg(device_name).arg(info.device_id).toUtf8();

    /* Verify the trigger exists */
    ret = find_type_by_name(m_trigger_name.constData(), "trigger");
    if (ret < 0) {
        *toError = tr("Failed to find the trigger %1").arg(m_trigger_name.constData());
        return 1;
    }

    ret = build_channel_array(m_dev_dir_name.constData(), &(info.channels), &(info.channels_count));

    m_device = info;
    if (! openDevice(toError)) {
        return false;
    } else {
        // tmrPolling_timeout();
        m_currentOrientation = OrientationEnums::ORIENTATION_TOP;
    }


    return true;
}

void AccellerometerController::setPollingIntervalMsec(int pollingIntervalMsec)
{
    m_pollingIntervalMsec = pollingIntervalMsec;
    _tmrPolling->setInterval(m_pollingIntervalMsec);
    _tmrPolling->start();

}
void AccellerometerController::tmrPolling_timeout()
{
    this->setCurrentOrientation(this->prepare_output());
}

int AccellerometerController::enable_sensors(const char *device_dir)
{
    DIR *dp;
    FILE *sysfsfp;
    int ret;
    const struct dirent *ent;
    char *scan_el_dir;
    char *filename;

    ret = asprintf(&scan_el_dir, FORMAT_SCAN_ELEMENTS_DIR, device_dir);
    if (ret < 0) {
        ret = -ENOMEM;
        return ret;;
    }
    dp = opendir(scan_el_dir);
    if (dp == NULL) {
        ret = -errno;
        goto error_free_name;
    }
    while (ent = readdir(dp), ent != NULL)
        if (strcmp(ent->d_name + strlen(ent->d_name) - strlen("_en"),
                "_en") == 0) {
            ret = asprintf(&filename,
                    "%s/%s", scan_el_dir, ent->d_name);
            if (ret < 0) {
                ret = -ENOMEM;
                goto error_close_dir;
            }
            sysfsfp = fopen(filename, "r");
            if (sysfsfp == NULL) {
                ret = -errno;
                free(filename);
                goto error_close_dir;
            }
            fscanf(sysfsfp, "%d", &ret);
            fclose(sysfsfp);
            if (!ret)
                write_sysfs_int(ent->d_name, scan_el_dir, 1);
            free(filename);
        }
    ret = 0;
error_close_dir:
    closedir(dp);
error_free_name:
    free(scan_el_dir);
    return ret;
}

int AccellerometerController::find_type_by_name(const char *name, const char *type)
{
    const struct dirent *ent;
    int number, numstrlen;

    FILE *nameFile;
    DIR *dp;
    char thisname[IIO_MAX_NAME_LENGTH];
    char *filename;

    dp = opendir(iio_dir);
    if (dp == NULL) {
        return -ENODEV;
    }

    while (ent = readdir(dp), ent != NULL) {
        if (strcmp(ent->d_name, ".") != 0 &&
            strcmp(ent->d_name, "..") != 0 &&
            strlen(ent->d_name) > strlen(type) &&
            strncmp(ent->d_name, type, strlen(type)) == 0) {
            numstrlen = sscanf(ent->d_name + strlen(type),
                       "%d",
                       &number);
            /* verify the next character is not a colon */
            if (strncmp(ent->d_name + strlen(type) + numstrlen,
                    ":",
                    1) != 0) {
                filename = (char*)malloc(strlen(iio_dir)
                        + strlen(type)
                        + numstrlen
                        + 6);
                if (filename == NULL) {
                    closedir(dp);
                    return -ENOMEM;
                }
                sprintf(filename, "%s%s%d/name",
                    iio_dir,
                    type,
                    number);
                nameFile = fopen(filename, "r");
                if (!nameFile) {
                    free(filename);
                    continue;
                }
                free(filename);
                fscanf(nameFile, "%s", thisname);
                fclose(nameFile);
                if (strcmp(name, thisname) == 0) {
                    closedir(dp);
                    return number;
                }
            }
        }
    }
    closedir(dp);
    return -ENODEV;
}

bool AccellerometerController::openDevice(QString *toError)
{
    if (m_fp > -1 )
    {
        *toError = tr("Device already opened");
        return false;
    }
    /* Set the device trigger to be the data ready trigger */

    int ret = write_sysfs_string_and_verify("trigger/current_trigger",
            m_dev_dir_name.constData(), m_trigger_name.constData());
    if (ret < 0) {
        *toError = tr("Failed to write current_trigger file %1 %2").arg(QString::fromUtf8(m_dev_dir_name)).arg(strerror(-ret));
        return false;
    }

    /*	Setup ring buffer parameters */

    ret = write_sysfs_int("buffer/length", m_dev_dir_name.constData(), 128);
    if (ret < 0) {
        *toError = tr("Failed to write buffer/length to %1").arg(m_dev_dir_name.constData());
        return false;
    }

    /* Enable the buffer */

    ret = write_sysfs_int_and_verify("buffer/enable", m_dev_dir_name.constData(), 1);
    if (ret < 0) {
        *toError = tr("Unable to enable the buffer %1").arg(ret);
        return false;
    }

    /* Attempt to open non blocking to access dev */
    QByteArray buffer_access = QString("/dev/iio:device%1").arg(m_device.device_id).toUtf8();
    m_fp = open(buffer_access.constData(), O_RDONLY | O_NONBLOCK);

    if (m_fp == -1) { /* If it isn't there make the node */
        *toError = tr("Failed to open %1 : %2 - %3").arg(buffer_access.constData()).arg(strerror(errno)).arg(errno);
        return false;
    }

    /* Actually read the data */
    m_pfd.fd = m_fp;
    m_pfd.events = POLLIN;
    // m_pfd = {.fd = m_fp, .events = POLLIN,};

    return true;
}

bool AccellerometerController::closeDevice()
{
    if (m_fp < 0)
        return false;


    /* Stop the buffer */
    int bret = write_sysfs_int("buffer/enable", m_dev_dir_name.constData(), 0);
    if (bret < 0) {
        close(m_fp);
        m_fp = -1;
        return false;
    }

    /* Disconnect the trigger - just write a dummy name. */
    write_sysfs_string("trigger/current_trigger", m_dev_dir_name.constData(), "NULL");
    close(m_fp);
    m_fp = -1;
    return true;

}

OrientationEnums::OrientationPositions AccellerometerController::prepare_output()
{
    OrientationEnums::OrientationPositions ret = OrientationEnums::ORIENTATION_INVALID;
    if (m_fp < 0) {
        return ret;
    }
    SensorData data;

    int buf_len = 127;

    data.scan_size = size_from_channelarray(m_device.channels,m_device.channels_count);
    data.data = (char*)malloc(data.scan_size * buf_len);
    if (!data.data) {
        return ret;
    }

//    printf("Polling the data\n");

    poll(&m_pfd, 1, -1);

//    printf("Reading the data\n");

    data.read_size = read(m_fp, data.data, buf_len * data.scan_size);

//    qDebug() << "Read the data";

    if (data.read_size == -EAGAIN) {
        printf("nothing available\n");
    } else {
        ret = process_scan(data, m_device);
    }


    free(data.data);
    return ret;
}

