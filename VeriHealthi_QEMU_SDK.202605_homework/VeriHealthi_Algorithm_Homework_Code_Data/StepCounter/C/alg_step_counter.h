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

#ifndef __ALG_STEP_COUNTER_H__
#define __ALG_STEP_COUNTER_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ACC_FS (50)

/**
 * @brief the struct of input data
 */
typedef struct AccInput {
    uint16_t len;
    int16_t *x;
    int16_t *y;
    int16_t *z;
} AccInput;

/**
 * @brief error code
 */
typedef enum AlgoError { ALGO_NORMAL = 0, ALGO_ERR_GENERIC = 1 } AlgoError;

/**
 * @brief initialize global variables for step counter algorithm
 */
AlgoError step_counter_init(void);

/**
 * @brief obtain step counts for current input data
 * @param acc_input: input data
 * @param step_num: the step counting results
 */
AlgoError step_counter_process(AccInput *acc_input, uint16_t *step_num);

#endif
