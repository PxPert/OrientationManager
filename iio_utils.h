#ifndef IIO_UTILS_H
#define IIO_UTILS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* IIO - useful set of util functionality
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#ifdef	__cplusplus
extern "C" {
#endif

/* Made up value to limit allocation sizes */
#include <stdint.h>
#include <stdlib.h>

#define IIO_MAX_NAME_LENGTH 30

#define FORMAT_SCAN_ELEMENTS_DIR "%s/scan_elements"
#define FORMAT_TYPE_FILE "%s_type"

/*
const char *iio_dir = "/sys/bus/iio/devices/";
const char *iio_debugfs_dir = "/sys/kernel/debug/iio/";
*/
#define iio_dir "/sys/bus/iio/devices/"
#define iio_debugfs_dir "/sys/kernel/debug/iio/"

/**
 * struct iio_channel_info - information about a given channel
 * @name: channel name
 * @generic_name: general name for channel type
 * @scale: scale factor to be applied for conversion to si units
 * @offset: offset to be applied for conversion to si units
 * @index: the channel index in the buffer output
 * @bytes: number of bytes occupied in buffer output
 * @mask: a bit mask for the raw output
 * @is_signed: is the raw value stored signed
 * @enabled: is this channel enabled
 **/
struct iio_channel_info {
    char *name;
    char *generic_name;
    float scale;
    float offset;
    unsigned index;
    unsigned bytes;
    unsigned bits_used;
    unsigned shift;
    uint64_t mask;
    unsigned be;
    unsigned is_signed;
    unsigned enabled;
    unsigned location;
};

struct Device_info_s {
    int device_id;
    int channels_count;
    struct iio_channel_info *channels;
};

typedef struct Device_info_s Device_info;

typedef struct SensorData_s {
    ssize_t read_size;
    int scan_size;
    char* data;
} SensorData;


/**
 * iioutils_break_up_name() - extract generic name from full channel name
 * @full_name: the full channel name
 * @generic_name: the output generic channel name
 **/
int iioutils_break_up_name(const char *full_name,
                  char **generic_name);

/**
 * build_channel_array() - function to figure out what channels are present
 * @device_dir: the IIO device directory in sysfs
 * @
 **/
int build_channel_array(const char *device_dir,
                  struct iio_channel_info **ci_array,
                  int *counter);


/**
 * iioutils_get_type() - find and process _type attribute data
 * @is_signed: output whether channel is signed
 * @bytes: output how many bytes the channel storage occupies
 * @mask: output a bit mask for the raw data
 * @be: big endian
 * @device_dir: the iio device directory
 * @name: the channel name
 * @generic_name: the channel type name
 **/
int iioutils_get_type(unsigned *is_signed,
                 unsigned *bytes,
                 unsigned *bits_used,
                 unsigned *shift,
                 uint64_t *mask,
                 unsigned *be,
                 const char *device_dir,
                 const char *name,
                 const char *generic_name);

int iioutils_get_param_float(float *output,
                    const char *param_name,
                    const char *device_dir,
                    const char *name,
                    const char *generic_name);

/**
 * bsort_channel_array_by_index() - reorder so that the array is in index order
 *
 **/

void bsort_channel_array_by_index(struct iio_channel_info **ci_array,
                     int cnt);


int _write_sysfs_int(const char *filename, const char *basedir, int val, int verify, int type, int val2);

int write_sysfs_int(const char *filename, const char *basedir, int val);

int write_sysfs_int_and_verify(const char *filename, const char *basedir, int val);

int write_sysfs_int2(const char *filename, const char *basedir, int val, int val2);

int _write_sysfs_string(const char *filename, const char *basedir, const char *val, int verify);

/**
 * write_sysfs_string_and_verify() - string write, readback and verify
 * @filename: name of file to write to
 * @basedir: the sysfs directory in which the file is to be found
 * @val: the string to write
 **/
int write_sysfs_string_and_verify(const char *filename, const char *basedir, const char *val);

int write_sysfs_string(const char *filename, const char *basedir,const char *val);

int read_sysfs_posint(char *filename, char *basedir);

int read_sysfs_float(char *filename, char *basedir, float *val);


    int size_from_channelarray(struct iio_channel_info *channels, int num_channels);

    int prepare_output(Device_info* info, char * dev_dir_name, char * trigger_name,
            int (*callback)(SensorData, Device_info));

#ifdef	__cplusplus
}
#endif

#endif
