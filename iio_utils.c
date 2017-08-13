#ifdef	__cplusplus
extern "C" {
#endif


#include "iio_utils.h"

#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <ctype.h>




/**
 * size_from_channelarray() - calculate the storage size of a scan
 * @channels:           the channel info array
 * @num_channels:       number of channels
 *
 * Has the side effect of filling the channels[i].location values used
 * in processing the buffer output.
 **/
int size_from_channelarray(struct iio_channel_info *channels, int num_channels)
{
        int bytes = 0;
        int i = 0;
        while (i < num_channels) {
                if (bytes % channels[i].bytes == 0)
                        channels[i].location = bytes;
                else
                        channels[i].location = bytes - bytes%channels[i].bytes
                                + channels[i].bytes;
                bytes = channels[i].location + channels[i].bytes;
                i++;
        }
        return bytes;
}

void print2byte(int input, struct iio_channel_info *info)
{
        /* First swap if incorrect endian */
        if (info->be)
                input = be16toh((uint16_t)input);
        else
                input = le16toh((uint16_t)input);

        /*
         * Shift before conversion to avoid sign extension
         * of left aligned data
         */
        input = input >> info->shift;
        if (info->is_signed) {
                int16_t val = input;
                val &= (1 << info->bits_used) - 1;
                val = (int16_t)(val << (16 - info->bits_used)) >>
                        (16 - info->bits_used);
                printf("SCALED %05f ", ((float)val + info->offset)*info->scale);
        } else {
                uint16_t val = input;
                val &= (1 << info->bits_used) - 1;
                printf("SCALED %05f ", ((float)val + info->offset)*info->scale);
        }
}
/**
 * process_scan() - print out the values in SI units
 * @data:               pointer to the start of the scan
 * @channels:           information about the channels. Note
 *  size_from_channelarray must have been called first to fill the
 *  location offsets.
 * @num_channels:       number of channels
 **/
void process_scan(char *data,
                  struct iio_channel_info *channels,
                  int num_channels)
{
        int k;
        for (k = 0; k < num_channels; k++) {
      /*pfps printf("PROCESS SCAN channel %d bytes %d location %d signed %d data %x %d\n",k,
              channels[k].bytes,channels[k].location,
              channels[k].is_signed,(data+channels[k].location),*(data+channels[k].location));*/
                switch (channels[k].bytes) {
                        /* only a few cases implemented so far */
                case 2:
                        print2byte(*(uint16_t *)(data + channels[k].location),
                                   &channels[k]);
                        break;
                case 4:
                        if (!channels[k].is_signed) {
                                uint32_t val = *(uint32_t *)
                                        (data + channels[k].location);
                                printf("SCALED %05f ", ((float)val +
                                                 channels[k].offset)*
                                       channels[k].scale);
                        } else {
              int32_t val = *(int32_t *) (data + channels[k].location);
              /*pfps printf("VAL RAW %d %8x  ",channels[k].location,val); */
              val = val >> channels[k].shift;
              /*pfps printf("SHIFT %d %8x  ",channels[k].shift,val); */
              if ( channels[k].bits_used < 32 ) val &= ((uint32_t)1 << channels[k].bits_used) - 1;
              /*pfps printf("MASK %d %8x  ",channels[k].bits_used,val); */
              val = (int32_t)(val << (32 - channels[k].bits_used)) >> (32 - channels[k].bits_used);
              /*pfps printf("FIX %x\n",val); */
              printf("%s %4d %6.1f  ", channels[k].name,
                 val, ((float)val + channels[k].offset)* channels[k].scale);
            }
                        break;
                case 8:
                        if (channels[k].is_signed) {
                                int64_t val = *(int64_t *)
                                        (data +
                                         channels[k].location);
                                if ((val >> channels[k].bits_used) & 1)
                                        val = (val & channels[k].mask) |
                                                ~channels[k].mask;
                                /* special case for timestamp */
                                if (channels[k].scale == 1.0f &&
                                     channels[k].offset == 0.0f)
                                        printf("TIME %" PRId64 " ", val);
                                else
                                        printf("SCALED %05f ", ((float)val +
                                                         channels[k].offset)*
                                               channels[k].scale);
                        }
                        break;
                default:
                        break;
                }
    }
        printf("\n");
}



/**
 * iioutils_break_up_name() - extract generic name from full channel name
 * @full_name: the full channel name
 * @generic_name: the output generic channel name
 **/
int iioutils_break_up_name(const char *full_name,
                  char **generic_name)
{
    char *current;
    char *w, *r;
    char *working;
    current = strdup(full_name);
    working = strtok(current, "_\0");
    w = working;
    r = working;

    while (*r != '\0') {
        if (!isdigit(*r)) {
            *w = *r;
            w++;
        }
        r++;
    }
    *w = '\0';
    *generic_name = strdup(working);
    free(current);

    return 0;
}



int prepare_output_delme(Device_info* info, char * dev_dir_name, char * trigger_name,
        int (*callback)(SensorData, Device_info)) {
    char * buffer_access;
    int ret, dev_num = info->device_id, num_channels = info->channels_count;
    struct iio_channel_info *channels = info->channels;
    SensorData data;

    int fp, buf_len = 127;


    /* Set the device trigger to be the data ready trigger */

    ret = write_sysfs_string_and_verify("trigger/current_trigger",
            dev_dir_name, trigger_name);
    if (ret < 0) {
        printf("Failed to write current_trigger file %s\n", strerror(-ret));
        return ret;;
    }


    /*	Setup ring buffer parameters */

    ret = write_sysfs_int("buffer/length", dev_dir_name, 128);
    if (ret < 0) return ret;;

    /* Enable the buffer */

    ret = write_sysfs_int_and_verify("buffer/enable", dev_dir_name, 1);
    if (ret < 0) {
        printf("Unable to enable the buffer %d\n", ret);
        return ret;;
    }

    data.scan_size = size_from_channelarray(channels, num_channels);
    data.data = (char*)malloc(data.scan_size * buf_len);
    if (!data.data) {
        ret = -ENOMEM;
        return ret;
    }

    ret = asprintf(&buffer_access, "/dev/iio:device%d", dev_num);
    if (ret < 0) {
        ret = -ENOMEM;
        free(data.data);
        return ret;
    }
    /* Attempt to open non blocking to access dev */
    fp = open(buffer_access, O_RDONLY | O_NONBLOCK);
    if (fp == -1) { /* If it isn't there make the node */
        printf("Failed to open %s : %s\n", buffer_access, strerror(errno));
        ret = -errno;
        free(buffer_access);
        free(data.data);
        return ret;
    }

    /* Actually read the data */
    struct pollfd pfd = {.fd = fp, .events = POLLIN,};


    printf("Polling the data\n");

    poll(&pfd, 1, -1);

    printf("Reading the data\n");

    data.read_size = read(fp, data.data, buf_len * data.scan_size);

    printf("Read the data\n");

    if (data.read_size == -EAGAIN) {
        printf("nothing available\n");
    } else {
        ret = callback(data, *info);
    }

    /* Stop the buffer */
    int bret = write_sysfs_int("buffer/enable", dev_dir_name, 0);
    if (bret < 0) {
        close(fp);
        free(buffer_access);
        free(data.data);
        return ret;
    }

    /* Disconnect the trigger - just write a dummy name. */
    write_sysfs_string("trigger/current_trigger", dev_dir_name, "NULL");

    close(fp);
    free(buffer_access);
    free(data.data);
    return ret;
}



int _write_sysfs_string(const char *filename, const char *basedir, const char *val, int verify)
{
    int ret = 0;
    FILE  *sysfsfp;
    char *temp = (char*)malloc(strlen(basedir) + strlen(filename) + 2);
    if (temp == NULL) {
        return -ENOMEM;
    }
    sprintf(temp, "%s/%s", basedir, filename);
    sysfsfp = fopen(temp, "w");
    if (sysfsfp == NULL) {
        ret = -errno;
        goto error_free;
    }
    fprintf(sysfsfp, "%s", val);
    fclose(sysfsfp);
    if (verify) {
        sysfsfp = fopen(temp, "r");
        if (sysfsfp == NULL) {
            ret = -errno;
            goto error_free;
        }
        fscanf(sysfsfp, "%s", temp);
        if (strcmp(temp, val) != 0) {
            ret = -1;
        }
        fclose(sysfsfp);
    }
error_free:
    free(temp);

    return ret;
}

int write_sysfs_string_and_verify(const char *filename, const char *basedir, const char *val)
{
    return _write_sysfs_string(filename, basedir, val, 1);
}

int write_sysfs_string(const char *filename, const char *basedir, const char *val)
{
    return _write_sysfs_string(filename, basedir, val, 0);
}

int read_sysfs_posint(char *filename, char *basedir)
{
    int ret;
    FILE  *sysfsfp;
    char *temp = (char*)malloc(strlen(basedir) + strlen(filename) + 2);
    if (temp == NULL) {
        return -ENOMEM;
    }
    sprintf(temp, "%s/%s", basedir, filename);
    sysfsfp = fopen(temp, "r");
    if (sysfsfp == NULL) {
        ret = -errno;
        goto error_free;
    }
    fscanf(sysfsfp, "%i\n", &ret);
    fclose(sysfsfp);
error_free:
    free(temp);
    return ret;
}

int read_sysfs_float(char *filename, char *basedir, float *val)
{
    float ret = 0;
    FILE  *sysfsfp;
    char *temp = (char*)malloc(strlen(basedir) + strlen(filename) + 2);
    if (temp == NULL) {
        return -ENOMEM;
    }
    sprintf(temp, "%s/%s", basedir, filename);
    sysfsfp = fopen(temp, "r");
    if (sysfsfp == NULL) {
        ret = -errno;
        goto error_free;
    }
    fscanf(sysfsfp, "%f\n", val);
    fclose(sysfsfp);
error_free:
    free(temp);
    return ret;
}

int _write_sysfs_int(const char *filename, const char *basedir, int val, int verify, int type, int val2)
{
    int ret = 0;
    FILE *sysfsfp;
    int test;
    char *temp = (char*)malloc(strlen(basedir) + strlen(filename) + 2);
    if (temp == NULL)
        return -ENOMEM;
    sprintf(temp, "%s/%s", basedir, filename);
    sysfsfp = fopen(temp, "w");
    if (sysfsfp == NULL) {
        ret = -errno;
        goto error_free;
    }
    if (type)
        fprintf(sysfsfp, "%d %d", val, val2);
    else
        fprintf(sysfsfp, "%d", val);

    fclose(sysfsfp);
    if (verify) {
        sysfsfp = fopen(temp, "r");
        if (sysfsfp == NULL) {
            ret = -errno;
            goto error_free;
        }
        fscanf(sysfsfp, "%d", &test);
        if (test != val) {
            ret = -1;
        }
        fclose(sysfsfp);
    }
error_free:
    free(temp);
    return ret;
}

int write_sysfs_int(const char *filename, const char *basedir, int val)
{
    return _write_sysfs_int(filename, basedir, val, 0, 0, 0);
}

int write_sysfs_int_and_verify(const char *filename, const char *basedir, int val)
{
    return _write_sysfs_int(filename, basedir, val, 1, 0, 0);
}

int write_sysfs_int2(const char *filename, const char *basedir, int val, int val2)
{
    return _write_sysfs_int(filename, basedir, val, 0, 1 , val2);
}



void bsort_channel_array_by_index(struct iio_channel_info **ci_array,
                     int cnt)
{

    struct iio_channel_info temp;
    int x, y;

    for (x = 0; x < cnt; x++)
        for (y = 0; y < (cnt - 1); y++)
            if ((*ci_array)[y].index > (*ci_array)[y+1].index) {
                temp = (*ci_array)[y + 1];
                (*ci_array)[y + 1] = (*ci_array)[y];
                (*ci_array)[y] = temp;
            }
}

int build_channel_array(const char *device_dir,
                  struct iio_channel_info **ci_array,
                  int *counter)
{
    DIR *dp;
    FILE *sysfsfp;
    int count, i;
    struct iio_channel_info *current;
    int ret;
    const struct dirent *ent;
    char *scan_el_dir;
    char *filename;

    *counter = 0;
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
            if /* pfps (ret == 1) */ (ret )
                (*counter)++;
            fclose(sysfsfp);
            free(filename);
        }
    *ci_array = malloc(sizeof(**ci_array) * (*counter));
    if (*ci_array == NULL) {
        ret = -ENOMEM;
        goto error_close_dir;
    }
    seekdir(dp, 0);
    count = 0;
    while (ent = readdir(dp), ent != NULL) {
        if (strcmp(ent->d_name + strlen(ent->d_name) - strlen("_en"),
               "_en") == 0) {
            current = &(*ci_array)[count++];
            ret = asprintf(&filename,
                       "%s/%s", scan_el_dir, ent->d_name);
            if (ret < 0) {
                ret = -ENOMEM;
                /* decrement count to avoid freeing name */
                count--;
                goto error_cleanup_array;
            }
            sysfsfp = fopen(filename, "r");
            if (sysfsfp == NULL) {
                free(filename);
                ret = -errno;
                goto error_cleanup_array;
            }
            fscanf(sysfsfp, "%u", &current->enabled);
            fclose(sysfsfp);

            if (!current->enabled) {
                free(filename);
                count--;
                continue;
            }

            current->scale = 1.0;
            current->offset = 0;
            current->name = strndup(ent->d_name,
                        strlen(ent->d_name) -
                        strlen("_en"));
            if (current->name == NULL) {
                free(filename);
                ret = -ENOMEM;
                goto error_cleanup_array;
            }
            /* Get the generic and specific name elements */
            ret = iioutils_break_up_name(current->name,
                             &current->generic_name);
            if (ret) {
                free(filename);
                goto error_cleanup_array;
            }
            ret = asprintf(&filename,
                       "%s/%s_index",
                       scan_el_dir,
                       current->name);
            if (ret < 0) {
                free(filename);
                ret = -ENOMEM;
                goto error_cleanup_array;
            }
            sysfsfp = fopen(filename, "r");
            fscanf(sysfsfp, "%u", &current->index);
            fclose(sysfsfp);
            free(filename);
            /* Find the scale */
            ret = iioutils_get_param_float(&current->scale,
                               "scale",
                               device_dir,
                               current->name,
                               current->generic_name);
            if (ret < 0)
                goto error_cleanup_array;
            ret = iioutils_get_param_float(&current->offset,
                               "offset",
                               device_dir,
                               current->name,
                               current->generic_name);
            if (ret < 0)
                goto error_cleanup_array;
            ret = iioutils_get_type(&current->is_signed,
                        &current->bytes,
                        &current->bits_used,
                        &current->shift,
                        &current->mask,
                        &current->be,
                        device_dir,
                        current->name,
                        current->generic_name);
        }
    }

    closedir(dp);
    /* reorder so that the array is in index order */
    bsort_channel_array_by_index(ci_array, *counter);

    return 0;

error_cleanup_array:
    for (i = count - 1;  i >= 0; i--)
        free((*ci_array)[i].name);
    free(*ci_array);
error_close_dir:
    closedir(dp);
error_free_name:
    free(scan_el_dir);
    return ret;
}


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
                 const char *generic_name)
{
    FILE *sysfsfp;
    int ret;
    DIR *dp;
    char *scan_el_dir, *builtname, *builtname_generic, *filename = 0;
    char signchar, endianchar;
    unsigned padint;
    const struct dirent *ent;

    ret = asprintf(&scan_el_dir, FORMAT_SCAN_ELEMENTS_DIR, device_dir);
    if (ret < 0) {
        ret = -ENOMEM;
        return ret;;
    }
    ret = asprintf(&builtname, FORMAT_TYPE_FILE, name);
    if (ret < 0) {
        ret = -ENOMEM;
        goto error_free_scan_el_dir;
    }
    ret = asprintf(&builtname_generic, FORMAT_TYPE_FILE, generic_name);
    if (ret < 0) {
        ret = -ENOMEM;
        goto error_free_builtname;
    }

    dp = opendir(scan_el_dir);
    if (dp == NULL) {
        ret = -errno;
        goto error_free_builtname_generic;
    }
    while (ent = readdir(dp), ent != NULL)
        /*
         * Do we allow devices to override a generic name with
         * a specific one?
         */
        if ((strcmp(builtname, ent->d_name) == 0) ||
            (strcmp(builtname_generic, ent->d_name) == 0)) {
            ret = asprintf(&filename,
                       "%s/%s", scan_el_dir, ent->d_name);
            if (ret < 0) {
                ret = -ENOMEM;
                goto error_closedir;
            }
            sysfsfp = fopen(filename, "r");
            if (sysfsfp == NULL) {
                ret = -errno;
                goto error_free_filename;
            }

            ret = fscanf(sysfsfp,
                     "%ce:%c%u/%u>>%u",
                     &endianchar,
                     &signchar,
                     bits_used,
                     &padint, shift);
            if (ret < 0) {
                printf("failed to pass scan type description\n");
                ret = -errno;
                goto error_close_sysfsfp;
            }
            *be = (endianchar == 'b');
            *bytes = padint / 8;
            if (*bits_used == 64)
                *mask = ~0;
            else
                *mask = (1 << *bits_used) - 1;
            if (signchar == 's')
                *is_signed = 1;
            else
                *is_signed = 0;
            fclose(sysfsfp);
            free(filename);

            filename = 0;
            sysfsfp = 0;
        }
error_close_sysfsfp:
    if (sysfsfp)
        fclose(sysfsfp);
error_free_filename:
    if (filename)
        free(filename);
error_closedir:
    closedir(dp);
error_free_builtname_generic:
    free(builtname_generic);
error_free_builtname:
    free(builtname);
error_free_scan_el_dir:
    free(scan_el_dir);
    return ret;
}

int iioutils_get_param_float(float *output,
                    const char *param_name,
                    const char *device_dir,
                    const char *name,
                    const char *generic_name)
{
    FILE *sysfsfp;
    int ret;
    DIR *dp;
    char *builtname, *builtname_generic;
    char *filename = NULL;
    const struct dirent *ent;

    ret = asprintf(&builtname, "%s_%s", name, param_name);
    if (ret < 0) {
        ret = -ENOMEM;
        return ret;;
    }
    ret = asprintf(&builtname_generic,
               "%s_%s", generic_name, param_name);
    if (ret < 0) {
        ret = -ENOMEM;
        goto error_free_builtname;
    }
    dp = opendir(device_dir);
    if (dp == NULL) {
        ret = -errno;
        goto error_free_builtname_generic;
    }
    while (ent = readdir(dp), ent != NULL)
        if ((strcmp(builtname, ent->d_name) == 0) ||
            (strcmp(builtname_generic, ent->d_name) == 0)) {
            ret = asprintf(&filename,
                       "%s/%s", device_dir, ent->d_name);
            if (ret < 0) {
                ret = -ENOMEM;
                goto error_closedir;
            }
            sysfsfp = fopen(filename, "r");
            if (!sysfsfp) {
                ret = -errno;
                goto error_free_filename;
            }
            fscanf(sysfsfp, "%f", output);
            break;
        }
error_free_filename:
    if (filename)
        free(filename);
error_closedir:
    closedir(dp);
error_free_builtname_generic:
    free(builtname_generic);
error_free_builtname:
    free(builtname);
    return ret;
}


#ifdef	__cplusplus
}
#endif
