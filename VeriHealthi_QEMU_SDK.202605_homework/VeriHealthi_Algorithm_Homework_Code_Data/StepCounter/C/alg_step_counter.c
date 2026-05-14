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
#include <string.h>

#define ABS(a) (((a) >= 0) ? (a) : (-(a)))

/** buffer size to save historical data */
#define BUF_SEC (3)
#define BUF_LEN (BUF_SEC * ACC_FS)
/** window size to save new data */
#define WIN_SEC (5)
#define WIN_LEN (WIN_SEC * ACC_FS)
/** the total size to save data for each processing */
#define BUF_WIN_LEN (BUF_LEN + WIN_LEN)

#define MEAN_LEN1               (15)
#define MEAN_LEN2               (7)
#define ACC_SENSOR_GRAVITY      (4096)
#define STEP_ACC_DIFF_THRESHOLD (ACC_SENSOR_GRAVITY / 10) /**< 0.1g = 4096/10 */
#define PEAK_VALLEY_NUM         (250)
#define PEAK_VALLEY_DIFFERENCE  (ACC_SENSOR_GRAVITY / 14) /**< 1g/14 = 4096/14 */
/** 4 steps max per second, points number between peak & valley is FS/4/2 */
#define TIME_THRESHOLD1 (4)
/** 0.3 steps min per second, points number between peak & valley is FS/0.3/2 */
#define TIME_THRESHOLD2 (40)
#define LEFT_DATA_NUM   (2)

/**
 * @brief for store the peaks/valleys number and locations
 * @param p_cnt: peak number
 * @param v_cnt: valley number
 * @param p_loc: peak locations
 * @param v_loc: valley locations
 */
typedef struct PeakValley {
    uint16_t p_cnt;
    uint16_t v_cnt;
    uint16_t *p_loc;
    uint16_t *v_loc;
} PeakValley;

/**
 * @brief struct for storing the features of a complete waveform,
 * including a peak and it's left valley and right valley
 */
typedef struct WaveFormFeature {
    uint16_t peak;
    uint16_t left_valley;
    uint16_t right_valley;
    uint16_t left_height;
    uint16_t rignt_height;
    uint16_t left_time;
    uint16_t right_time;
} WaveFormFeature;

/**
 * @brief struct for storing acceleration data
 */
typedef struct AccData {
    uint16_t len;
    uint16_t x_cnt;
    uint16_t y_cnt;
    uint16_t z_cnt;
    int16_t *x;
    int16_t *y;
    int16_t *z;
} AccData;

typedef struct AccDataHub {
    AccData win;
    AccData buf;
} AccDataHub;

/**
 * @brief struct for storing mean filter parameters and data
 */
typedef struct MeanFilter {
    uint16_t len;
    uint16_t index;
    int16_t *buf;
    uint16_t buf_full;
} MeanFilter;

typedef struct MeanFilterGroup {
    MeanFilter f1;
    MeanFilter f2;
} MeanFilterGroup;

typedef struct MeanFilterHub {
    MeanFilterGroup x;
    MeanFilterGroup y;
    MeanFilterGroup z;
} MeanFilterHub;

/**
 * @brief struct for storing the output of mean filter
 */
typedef struct MeanOutput {
    int16_t raw;
    int16_t filt;
} MeanOutput;

uint16_t peak_loc[PEAK_VALLEY_NUM]   = {0};
uint16_t valley_loc[PEAK_VALLEY_NUM] = {0};
PeakValley peak_valley               = {0, 0, NULL, NULL};

int16_t x_win[WIN_LEN] = {0}, y_win[WIN_LEN] = {0}, z_win[WIN_LEN] = {0};
int16_t x_buf[WIN_LEN] = {0}, y_buf[WIN_LEN] = {0}, z_buf[WIN_LEN] = {0};
AccDataHub acc_data_hub;

int16_t xmean_filter1_buf[MEAN_LEN1] = {0}, ymean_filter1_buf[MEAN_LEN1] = {0},
        zmean_filter1_buf[MEAN_LEN1] = {0};
int16_t xmean_filter2_buf[MEAN_LEN2] = {0}, ymean_filter2_buf[MEAN_LEN2] = {0},
        zmean_filter2_buf[MEAN_LEN2] = {0};
MeanFilterHub mean_filter_hub;

static AlgoError array_max_min(int16_t *a, uint16_t a_len, int16_t direction, int16_t *a_max_min)
{
    uint16_t i = 0;
    if (!a || !a_max_min || a_len == 0 || (direction != -1 && direction != 1)) {
        return ALGO_ERR_GENERIC;
    }
    *a_max_min = *a;
    for (i = 1; i < a_len; i++) {
        if (*(a + i) * direction > (*a_max_min) * direction) {
            *a_max_min = *(a + i);
        }
    }
    return ALGO_NORMAL;
}

static AlgoError array_mean(int16_t *a, uint16_t a_len, int16_t *a_mean)
{
    int32_t sum = 0;
    uint16_t i  = 0;
    if (!a || !a_mean || a_len == 0) {
        return ALGO_ERR_GENERIC;
    }
    for (i = 0; i < a_len; i++) {
        sum += (int32_t)(a[i]);
    }
    *a_mean = (int16_t)(sum / a_len);
    return ALGO_NORMAL;
}

static AlgoError mean_filtering(int16_t input_data, MeanFilter *filter, MeanOutput *output)
{
    uint16_t i = 0, j = 0;
    int32_t tmp_data  = 0;
    uint16_t half_len = 0;
    AlgoError ret     = ALGO_ERR_GENERIC;

    if (!filter || !output) {
        return ALGO_ERR_GENERIC;
    }

    half_len = (filter->len - 1) >> 1;

    if (filter->buf_full == 0) {
        if (filter->index < filter->len) {
            filter->buf[filter->index++] = input_data;
            if (filter->index % 2 == 1) {
                j = (filter->index - 1) >> 1;
                array_mean(filter->buf, filter->index, &(output->filt));
                output->raw = filter->buf[j];
                ret         = ALGO_NORMAL;
            }
            if (filter->index >= filter->len) {
                filter->buf_full = 1;
                filter->index    = 0;
            }
        }
    } else {
        if (filter->index < filter->len) {
            filter->buf[filter->index++] = input_data;
            if (filter->index >= filter->len) {
                filter->index = 0;
            }
        }
        j = (filter->index + half_len) % (filter->len);
        array_mean(filter->buf, filter->len, &(output->filt));
        output->raw = filter->buf[j];
        ret         = ALGO_NORMAL;
    }
    return ret;
}

static AlgoError if_a_in_A(uint16_t a, int16_t *A, uint16_t A_len, int16_t *in_flag)
{
    uint16_t i = 0;
    if (!A || !in_flag || A_len == 0) {
        return ALGO_ERR_GENERIC;
    }
    *in_flag = 0;
    while (i < A_len) {
        if (*(A + i) == a) {
            *in_flag = 1;
            break;
        }
        i++;
    }
    return ALGO_NORMAL;
}

static AlgoError delete_ith_A(int16_t *A, uint16_t A_len, uint16_t i)
{
    uint16_t j = 0;
    if (!A || A_len == 0 || i >= A_len) {
        return ALGO_ERR_GENERIC;
    }
    for (j = i + 1; j < A_len; j++) {
        A[j - 1] = A[j];
    }
    A[A_len - 1] = 0;
    return ALGO_NORMAL;
}

static AlgoError find_possible_peak_valley(int16_t *a, uint16_t a_len, PeakValley *peak_valley)
{
    uint16_t i = 0;
    if (!a || !peak_valley || a_len == 0) {
        return ALGO_ERR_GENERIC;
    }
    for (i = 1; i < a_len - 1; i++) {
        if (peak_valley->p_cnt < PEAK_VALLEY_NUM) {
            if ((*(a + i) >= *(a + i - 1)) && (*(a + i) > *(a + i + 1))) {
                peak_valley->p_loc[peak_valley->p_cnt++] = i;
            }
        }
        if (peak_valley->v_cnt < PEAK_VALLEY_NUM) {
            if ((*(a + i) <= *(a + i - 1)) && (*(a + i) < *(a + i + 1))) {
                peak_valley->v_loc[peak_valley->v_cnt++] = i;
            }
        }
    }
    return ALGO_NORMAL;
}

static AlgoError merge_close_pole(int16_t *a, PeakValley *peak_valley, int16_t direction)
{
    uint16_t *pole_loc = NULL;
    uint16_t *v_loc    = NULL;
    uint16_t pole_cnt = 0, v_cnt = 0;
    uint16_t i = 0, j = 0;
    int16_t in_flag = 0;
    AlgoError r     = ALGO_ERR_GENERIC;
    if (!a || !peak_valley || (direction != 1 && direction != -1)) {
        return ALGO_ERR_GENERIC;
    }
    if (direction == 1) {
        pole_loc = peak_valley->p_loc;
        pole_cnt = peak_valley->p_cnt;
        v_loc    = peak_valley->v_loc;
        v_cnt    = peak_valley->v_cnt;
    } else if (direction == -1) {
        pole_loc = peak_valley->v_loc;
        pole_cnt = peak_valley->v_cnt;
        v_loc    = peak_valley->p_loc;
        v_cnt    = peak_valley->p_cnt;
    }
    if (pole_cnt > 1) {
        i = 1;
        while (i < pole_cnt) {
            r = 0;
            j = pole_loc[i - 1];
            while (j < pole_loc[i]) {
                if_a_in_A(j, v_loc, v_cnt, &in_flag);
                if (in_flag) {
                    break;
                }
                j++;
            }
            if (!in_flag) {
                if (a[pole_loc[i - 1]] * direction > a[pole_loc[i]] * direction) {
                    delete_ith_A(pole_loc, pole_cnt--, i);
                } else {
                    delete_ith_A(pole_loc, pole_cnt--, i - 1);
                }
            } else {
                i++;
            }
        }
    }
    if (direction == 1) {
        peak_valley->p_cnt = pole_cnt;
    } else if (direction == -1) {
        peak_valley->v_cnt = pole_cnt;
    }
    return ALGO_NORMAL;
}

static AlgoError merge_close_peak_valley(int16_t *a, PeakValley *peak_valley)
{
    if (!a || !peak_valley) {
        return ALGO_ERR_GENERIC;
    }
    if (peak_valley->p_cnt > 1) {
        merge_close_pole(a, peak_valley, 1);
    }
    if (peak_valley->v_cnt > 1) {
        merge_close_pole(a, peak_valley, -1);
    }
    /** make sure the first valley is before the first peak */
    if (peak_valley->p_cnt > 0 &&
        ((peak_valley->v_cnt == 0) ||
         (peak_valley->v_cnt > 0 && *(peak_valley->p_loc) < *(peak_valley->v_loc)))) {
        delete_ith_A(peak_valley->p_loc, peak_valley->p_cnt--, 0);
    }
    return ALGO_NORMAL;
}

static AlgoError remove_false_pole(int16_t *a, int16_t a_mean, uint16_t *pole_loc,
                                   uint16_t *pole_cnt, int16_t direction)
{
    uint16_t i = 0, cur_loc = 0;

    if (!a || !pole_loc || !pole_cnt || (direction != 1 && direction != -1)) {
        return ALGO_ERR_GENERIC;
    }

    while (i < *pole_cnt) {
        cur_loc = *(pole_loc + i);
        if (*(a + cur_loc) * direction < a_mean * direction) {
            delete_ith_A(pole_loc, *pole_cnt, i);
            (*pole_cnt)--;
        } else {
            i++;
        }
    }
    return ALGO_NORMAL;
}

static AlgoError remove_false_peak_valley(int16_t *a, uint16_t a_len, PeakValley *peak_valley)
{
    int16_t a_mean = 0;
    uint16_t i = 0, cur_loc = 0;
    if (!a || !peak_valley || a_len == 0) {
        return ALGO_ERR_GENERIC;
    }
    array_mean(a, a_len, &a_mean);
    remove_false_pole(a, a_mean, peak_valley->p_loc, &(peak_valley->p_cnt), 1);
    remove_false_pole(a, a_mean, peak_valley->v_loc, &(peak_valley->v_cnt), -1);
    return ALGO_NORMAL;
}

static AlgoError calculate_waveform_feature(int16_t *a, uint16_t a_len, WaveFormFeature *waveform)
{
    int16_t lh = 0, rh = 0;
    if (!a || !waveform || waveform->peak >= a_len || waveform->left_valley >= a_len ||
        waveform->right_valley >= a_len || waveform->left_valley >= waveform->right_valley ||
        waveform->peak <= waveform->left_valley || waveform->peak >= waveform->right_valley) {
        return ALGO_ERR_GENERIC;
    }
    lh = *(a + waveform->peak) - *(a + waveform->left_valley);
    rh = *(a + waveform->peak) - *(a + waveform->right_valley);
    if (lh < 0 || rh < 0) {
        return ALGO_ERR_GENERIC;
    }
    waveform->left_height  = lh;
    waveform->rignt_height = rh;
    waveform->left_time    = waveform->peak - waveform->left_valley;
    waveform->right_time   = waveform->right_valley - waveform->peak;
    return ALGO_NORMAL;
}

static AlgoError remove_asymmetric_peaks(int16_t *a, uint16_t a_len, PeakValley *peak_valley)
{
    uint16_t i = 0, j = 0, r1 = 0, r2 = 0;
    uint16_t *p_loc, *v_loc;
    WaveFormFeature waveform = {0, 0, 0, 0, 0, 0, 0};
    uint16_t last_valley_loc = 0;
    AlgoError ret            = ALGO_ERR_GENERIC;

    if (!a || !peak_valley) {
        return ALGO_ERR_GENERIC;
    }

    p_loc = peak_valley->p_loc;
    v_loc = peak_valley->v_loc;

    if (peak_valley->v_cnt > 1) {
        i = 0;
        while (i < peak_valley->p_cnt) {
            r1 = 0;
            r2 = 0;
            j  = 1;
            while (j < peak_valley->v_cnt) {
                if (v_loc[j - 1] < p_loc[i] && v_loc[j] > p_loc[i]) {
                    /** find the valid peak with two valid valleys */
                    r1                    = 1;
                    waveform.peak         = p_loc[i];
                    waveform.left_valley  = v_loc[j - 1];
                    waveform.right_valley = v_loc[j];
                    ret                   = calculate_waveform_feature(a, a_len, &waveform);
                    if (ret == ALGO_NORMAL) {
                        if (waveform.left_height > PEAK_VALLEY_DIFFERENCE &&
                            waveform.rignt_height > PEAK_VALLEY_DIFFERENCE &&
                            waveform.left_height > waveform.rignt_height / 2 &&
                            waveform.left_height < waveform.rignt_height * 2 &&
                            waveform.left_time >= TIME_THRESHOLD1 &&
                            waveform.left_time <= TIME_THRESHOLD2 &&
                            waveform.right_time >= TIME_THRESHOLD1 &&
                            waveform.right_time <= TIME_THRESHOLD2) {
                            last_valley_loc = v_loc[j];
                        } else {
                            /** the peak is invalid */
                            r2 = 1;
                            delete_ith_A(p_loc, peak_valley->p_cnt, i);
                            peak_valley->p_cnt--;
                            if (v_loc[j - 1] != last_valley_loc) {
                                delete_ith_A(v_loc, peak_valley->v_cnt, j - 1);
                                peak_valley->v_cnt--;
                            }
                        }
                    } else {
                        return ret;
                    }
                    break;
                }
                j++;
            }
            if (r1 == 0) {
                /** there is no valid valleys for the peak */
                delete_ith_A(p_loc, peak_valley->p_cnt, i);
                peak_valley->p_cnt--;
            } else if (r2 == 0) {
                i++;
            }
        }
    }
    return ALGO_NORMAL;
}

static AlgoError group_mean_filtering(int16_t data, MeanFilterGroup *filter_group,
                                      MeanOutput *mean_output)
{
    AlgoError ret = ALGO_ERR_GENERIC;
    MeanOutput output1, output2;
    if (!filter_group || !mean_output) {
        return ALGO_ERR_GENERIC;
    }
    ret = mean_filtering(data, &(filter_group->f1), &output1);
    if (ret == ALGO_NORMAL) {
        ret = mean_filtering(output1.filt, &(filter_group->f2), &output2);
        if (ret == ALGO_NORMAL) {
            mean_output->raw  = data;
            mean_output->filt = output2.filt;
            return ALGO_NORMAL;
        }
    }
    return ret;
}

static AlgoError load_data(int16_t *input, uint16_t len, int16_t *buf, uint16_t buf_len,
                           uint16_t *buf_cnt)
{
    uint16_t i = 0, j = 0;
    if (!input || !buf || !buf_cnt || buf_len == 0) {
        return ALGO_ERR_GENERIC;
    }
    if (*buf_cnt + len <= buf_len) {
        for (i = 0; i < len; i++) {
            buf[(*buf_cnt)++] = input[i];
        }
    } else {
        if (len >= buf_len) {
            for (i = 0; i < buf_len; i++) {
                buf[i] = input[len - buf_len + i];
            }
        } else {
            j = len + (*buf_cnt) - buf_len;
            for (i = j; i < (*buf_cnt); i++) {
                buf[i - j] = buf[i];
            }
            for (i = 0; i < len; i++) {
                buf[buf_len - len + i] = input[i];
            }
        }
        *buf_cnt = buf_len;
    }
    return ALGO_NORMAL;
}

static AlgoError acc_data_preprocess(AccInput *acc_input, MeanFilterHub *filter_hub,
                                     AccDataHub *acc_data_hub)
{
    AlgoError ret = ALGO_ERR_GENERIC;
    int16_t data  = 0;
    uint16_t i = 0, j = 0, array_cnt = 0;
    MeanOutput mean_output;
    int16_t array[WIN_LEN] = {0};

    if (!acc_input || !filter_hub || !acc_data_hub || acc_input->len > WIN_LEN) {
        return ALGO_ERR_GENERIC;
    }

    int16_t *arr_p[3]          = {acc_input->x, acc_input->y, acc_input->z};
    MeanFilterGroup *filt_p[3] = {&(filter_hub->x), &(filter_hub->y), &(filter_hub->z)};
    int16_t *acc_win_p[3]      = {acc_data_hub->win.x, acc_data_hub->win.y, acc_data_hub->win.z};
    int16_t *acc_win_cnt[3]    = {&(acc_data_hub->win.x_cnt), &(acc_data_hub->win.y_cnt),
                                  &(acc_data_hub->win.z_cnt)};

    for (i = 0; i < 3; i++) {
        array_cnt = 0;
        for (j = 0; j < acc_input->len; j++) {
            data = *(arr_p[i] + j);
            ret  = group_mean_filtering(data, filt_p[i], &mean_output);
            if (ret == ALGO_NORMAL) {
                array[array_cnt++] = mean_output.filt;
            }
        }
        load_data(array, array_cnt, acc_win_p[i], acc_data_hub->win.len, acc_win_cnt[i]);
    }
    if (*acc_win_cnt[0] != *acc_win_cnt[1] || *acc_win_cnt[0] != *acc_win_cnt[2]) {
        return ALGO_ERR_GENERIC;
    }
    return ALGO_NORMAL;
}

AlgoError step_counter_init(void)
{
    peak_valley.p_loc = peak_loc;
    peak_valley.p_cnt = 0;

    peak_valley.v_loc = valley_loc;
    peak_valley.v_cnt = 0;

    mean_filter_hub.x.f1.buf      = xmean_filter1_buf;
    mean_filter_hub.x.f1.buf_full = 0;
    mean_filter_hub.x.f1.index    = 0;
    mean_filter_hub.x.f1.len      = MEAN_LEN1;

    mean_filter_hub.x.f2.buf      = xmean_filter2_buf;
    mean_filter_hub.x.f2.buf_full = 0;
    mean_filter_hub.x.f2.index    = 0;
    mean_filter_hub.x.f2.len      = MEAN_LEN2;

    mean_filter_hub.y.f1.buf      = ymean_filter1_buf;
    mean_filter_hub.y.f1.buf_full = 0;
    mean_filter_hub.y.f1.index    = 0;
    mean_filter_hub.y.f1.len      = MEAN_LEN1;

    mean_filter_hub.y.f2.buf      = ymean_filter2_buf;
    mean_filter_hub.y.f2.buf_full = 0;
    mean_filter_hub.y.f2.index    = 0;
    mean_filter_hub.y.f2.len      = MEAN_LEN2;

    mean_filter_hub.z.f1.buf      = zmean_filter1_buf;
    mean_filter_hub.z.f1.buf_full = 0;
    mean_filter_hub.z.f1.index    = 0;
    mean_filter_hub.z.f1.len      = MEAN_LEN1;

    mean_filter_hub.z.f2.buf      = zmean_filter2_buf;
    mean_filter_hub.z.f2.buf_full = 0;
    mean_filter_hub.z.f2.index    = 0;
    mean_filter_hub.z.f2.len      = MEAN_LEN2;

    acc_data_hub.buf.x     = x_buf;
    acc_data_hub.buf.x_cnt = 0;
    acc_data_hub.buf.y     = y_buf;
    acc_data_hub.buf.y_cnt = 0;
    acc_data_hub.buf.z     = z_buf;
    acc_data_hub.buf.z_cnt = 0;
    acc_data_hub.buf.len   = WIN_LEN;

    acc_data_hub.win.x     = x_win;
    acc_data_hub.win.x_cnt = 0;
    acc_data_hub.win.y     = y_win;
    acc_data_hub.win.y_cnt = 0;
    acc_data_hub.win.z     = z_win;
    acc_data_hub.win.z_cnt = 0;
    acc_data_hub.win.len   = WIN_LEN;
    return ALGO_NORMAL;
}

AlgoError step_counter_process(AccInput *acc_input, uint16_t *step_num)
{
    uint16_t i = 0, j = 0;
    int16_t buf_win[BUF_WIN_LEN] = {0};
    uint16_t buf_win_cnt = 0, last_v_loc = 0, left_len = 0;
    AccData *win = &(acc_data_hub.win), *buf = &(acc_data_hub.buf);
    int16_t *buf_axis_p[3] = {buf->x, buf->y, buf->z};
    int16_t *win_axis_p[3] = {win->x, win->y, win->z};
    uint16_t *buf_cnt_p[3] = {&(buf->x_cnt), &(buf->y_cnt), &(buf->z_cnt)};
    uint16_t *win_cnt_p[3] = {&(win->x_cnt), &(win->y_cnt), &(win->z_cnt)};
    int16_t buf_win_max = 0, buf_win_min = 0, max_min_diff = 0;
    uint16_t xyz_steps[3] = {0};
    AlgoError ret         = ALGO_ERR_GENERIC;

    if (!acc_input || !step_num) {
        return ALGO_ERR_GENERIC;
    }

    *step_num = 0;

    ret = acc_data_preprocess(acc_input, &mean_filter_hub, &acc_data_hub);
    if (ret != ALGO_NORMAL) {
        return ret;
    }

    if (win->x_cnt >= win->len) {
        for (i = 0; i < 3; i++) {
            peak_valley.p_cnt = 0;
            peak_valley.v_cnt = 0;

            buf_win_cnt = 0;
            if (*buf_cnt_p[i] > 0) {
                load_data(buf_axis_p[i], *buf_cnt_p[i], buf_win, BUF_WIN_LEN, &buf_win_cnt);
                *buf_cnt_p[i] = 0;
            }
            load_data(win_axis_p[i], *win_cnt_p[i], buf_win, BUF_WIN_LEN, &buf_win_cnt);
            *win_cnt_p[i] = 0;

            array_max_min(buf_win, buf_win_cnt, 1, &buf_win_max);
            array_max_min(buf_win, buf_win_cnt, -1, &buf_win_min);

            max_min_diff = buf_win_max - buf_win_min;
            if (max_min_diff > STEP_ACC_DIFF_THRESHOLD) {
                ret = find_possible_peak_valley(buf_win, buf_win_cnt, &peak_valley);
                if (ret != ALGO_NORMAL) {
                    break;
                }

                ret = remove_false_peak_valley(buf_win, buf_win_cnt, &peak_valley);
                if (ret != ALGO_NORMAL) {
                    break;
                }

                ret = merge_close_peak_valley(buf_win, &peak_valley);
                if (ret != ALGO_NORMAL) {
                    break;
                }

                ret = remove_asymmetric_peaks(buf_win, buf_win_cnt, &peak_valley);
                if (ret != ALGO_NORMAL) {
                    break;
                }

                if (peak_valley.v_cnt >= 1) {
                    last_v_loc = peak_valley.v_loc[peak_valley.v_cnt - 1];
                    left_len   = buf_win_cnt - last_v_loc + LEFT_DATA_NUM;
                    if (left_len < BUF_LEN) {
                        for (j = 0; j < left_len; j++) {
                            *(buf_axis_p[i] + j) = buf_win[buf_win_cnt - left_len + j];
                            (*buf_cnt_p[i])++;
                        }
                    }
                }
                xyz_steps[i] = peak_valley.p_cnt;
            }
        }
    }
    if (ret == ALGO_NORMAL) {
        array_max_min(xyz_steps, 3, 1, &buf_win_max);
        array_max_min(xyz_steps, 3, -1, &buf_win_min);
        *step_num = xyz_steps[0] + xyz_steps[1] + xyz_steps[2] - buf_win_max - buf_win_min;
    }
    printf("x=%d, y=%d, z=%d, step=%d\t", xyz_steps[0] * 2, xyz_steps[1] * 2, xyz_steps[2] * 2,
           (*step_num) * 2);
    return ret;
}
