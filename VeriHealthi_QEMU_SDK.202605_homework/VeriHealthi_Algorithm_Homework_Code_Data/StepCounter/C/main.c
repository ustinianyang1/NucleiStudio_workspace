/*
 * Copyright (c) 2026, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * 1. Redistributing the source code of this software is only allowed after
 * receiving explicit, written permission from VeriSilicon. The copyright notice,
 * this list of conditions and the following disclaimer must be retained in all
 * source code distributions.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 */

#include "alg_step_counter.h"

#define MAX_ACC_LEN (10000)
static int16_t acc_x[MAX_ACC_LEN], acc_y[MAX_ACC_LEN], acc_z[MAX_ACC_LEN];

static void trim_newline(char *str)
{
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

static int read_data(const char *file_name, AccInput *saved_data)
{
    char line[256];
    int header_found = 0;
    int16_t new_num;
    uint16_t cnt;
    FILE *fd;
    int ret;
    if (!file_name || !saved_data) {
        return ALGO_ERR_GENERIC;
    }

    saved_data->len = 0;

    /** open data file */
    fd = fopen(file_name, "r");
    if (fd == NULL) {
        printf("Fail to open the file in read_data()\n");
        return ALGO_ERR_GENERIC;
    }

    /** skip header */
    while (fgets(line, sizeof(line), fd)) {
        trim_newline(line);
        if (strstr(line, "Device") != NULL || strstr(line, "time_stamp") != NULL ||
            strstr(line, "sample_rate") != NULL || strstr(line, "Data format") != NULL ||
            strstr(line, "TYPE") != NULL) {
            if (strstr(line, "TYPE") != NULL) {
                header_found = 1;
                break;
            }
        } else {
            break;
        }
    }
    if (!header_found) {
        fclose(fd);
        return ALGO_ERR_GENERIC;
    }

    /** read data file */
    cnt = 0;
    while ((ret = fscanf(fd, "%hd", &new_num)) != EOF && cnt < MAX_ACC_LEN * 7) {
        if (cnt % 7 == 3) {
            saved_data->x[cnt / 7] = new_num;
        } else if (cnt % 7 == 4) {
            saved_data->y[cnt / 7] = new_num;
        } else if (cnt % 7 == 5) {
            saved_data->z[cnt / 7] = new_num;
        }
        cnt++;
    }
    if (cnt % 7 != 0) {
        return ALGO_ERR_GENERIC;
    }
    saved_data->len = cnt / 7;

    /** close data file */
    ret = fclose(fd);
    if (ret != 0) {
        printf("Fail to close the file in read_data()\n");
        return ALGO_ERR_GENERIC;
    }
    return ALGO_NORMAL;
}

int main(void)
{
    uint16_t step_num = 0, step_total = 0, i;
    AlgoError ret        = ALGO_NORMAL;
    const char *fn       = "../../AccData/walk/IMU_walk_left_2026_04_28_15_38_28_ID0_step40.txt";
    AccInput acc_xyz     = {0, acc_x, acc_y, acc_z};
    AccInput acc_xyz_win = {0, NULL, NULL, NULL};

    ret = read_data(fn, &acc_xyz);
    if (ret != ALGO_NORMAL) {
        return 0;
    }

    ret = step_counter_init();
    if (ret != ALGO_NORMAL) {
        return 0;
    }

    for (i = 0; i <= acc_xyz.len - ACC_FS; i += ACC_FS) {
        acc_xyz_win.x   = acc_xyz.x + i;
        acc_xyz_win.y   = acc_xyz.y + i;
        acc_xyz_win.z   = acc_xyz.z + i;
        acc_xyz_win.len = ACC_FS;
        ret             = step_counter_process(&acc_xyz_win, &step_num);
        if (ret != ALGO_NORMAL) {
            printf("Error in step_counter_process()!\n");
            return 0;
        }
        step_total += step_num;
        printf("Time = %3d(s)\tTotal steps = %3d\n", i / ACC_FS, step_total * 2);
    }

    return 0;
}
