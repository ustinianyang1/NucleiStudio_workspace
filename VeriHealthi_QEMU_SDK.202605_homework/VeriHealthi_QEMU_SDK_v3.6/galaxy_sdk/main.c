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

#include <stddef.h>
#include <stdint.h>
#include "vs_conf.h"
#include "soc_init.h"
#include "soc_sysctl.h"
#include "bsp.h"
#include "uart_printf.h"
#include "board.h"
#include "osal.h"
#include "vpi_error.h"
#include "vpi_event.h"
#include "main.h"

static EventManager pSysManager = NULL;
static EventManager pCustomManager = NULL;

static int sys_manager_handler(EventManager manager, EventId event_id, EventParam param)
{
    uint32_t value;

    if (event_id == EVENT_SYS_TEST) {
        value = *(uint32_t *)param;
        uart_printf("Task1 received EVENT_SYS_TEST, value: 0x%lx\r\n", value);
    }

    return EVENT_OK;
}

static void task_sys_mgr(void *param)
{
    uart_printf("Task1 started, listening for events...\r\n");
    while (1) {
        vpi_event_listen(pSysManager);
    }
}

static void task_custom_mgr(void *param)
{
    uint32_t value = 0xa5a5;
    int ret;

    while (1) {
        uart_printf("Task2 sending EVENT_SYS_TEST with value: 0x%lx\r\n", value);
        ret = vpi_event_notify(EVENT_SYS_TEST, &value);
        if (ret != EVENT_OK) {
            uart_printf("vpi_event_notify failed: %d\r\n", ret);
        }
        osal_sleep(1000);
    }
}

static void task_init_app(void *param)
{
    int ret;
    BoardDevice board_dev;

    ret = board_register(board_get_ops());
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board register failed %d", ret);
        goto exit;
    }
    ret = board_init((void *)&board_dev);
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("board init failed %d", ret);
        goto exit;
    }
    if (board_dev.name) {
        uart_printf("Board: %s", board_dev.name);
    }

    uart_printf("Hello VeriHealthi!\r\n");

    pSysManager = vpi_event_new_manager(EVENT_MGR_SYS, sys_manager_handler);
    if (pSysManager == NULL) {
        uart_printf("Failed to create sys manager\r\n");
        goto exit;
    }

    ret = vpi_event_register(EVENT_SYS_TEST, pSysManager);
    if (ret != EVENT_OK) {
        uart_printf("Failed to register EVENT_SYS_TEST\r\n");
        goto exit;
    }

    pCustomManager = vpi_event_new_manager(EVENT_MGR_CUSTOM, NULL);
    if (pCustomManager == NULL) {
        uart_printf("Failed to create custom manager\r\n");
        goto exit;
    }

    osal_create_task(task_sys_mgr, "sys_mgr", 256, 4, NULL);
    osal_create_task(task_custom_mgr, "custom_mgr", 256, 5, NULL);

exit:
    osal_delete_task(NULL);
}

int main(void)
{
    int ret;

    ret = soc_init();
    ret = vsd_to_vpi(ret);
    if (ret != VPI_SUCCESS) {
        uart_printf("soc init error %d", ret);
        goto exit;
    } else {
        uart_printf("soc init done");
    }
    osal_pre_start_scheduler();
    osal_create_task(task_init_app, "init_app", 512, 1, NULL);
    osal_start_scheduler();
exit:
    goto exit;
}
