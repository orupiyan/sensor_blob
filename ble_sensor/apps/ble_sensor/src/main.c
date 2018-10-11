/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <nrf_temp.h>
#include <os/mynewt.h>
#include <nimble/ble.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

#include "ble_temp_sensor.h"
#include "gatt_svr.h"
#include "temp.h"


/* Global buffer to store temperature blob */
int16_t g_temp[10]={0};

#define GEN_TASK_PRIO       3     
#define GEN_TASK_STACK_SZ   512

static os_stack_t gen_task_stack[GEN_TASK_STACK_SZ];
static struct os_task gen_task_str;

/* Callback function for application task event */
static void my_ev_cb(struct os_event *);

/* Initialize the event with the callback function */
static struct os_event gen_task_ev = {
    .ev_cb = my_ev_cb,
};

/* Log data */
struct log logger;

static const char *device_name = "ble_temp_sensor";

static int ble_temp_gap_event(struct ble_gap_event *event, void *arg);

static uint8_t ble_temp_addr_type;

/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void
ble_temp_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     */
    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                    BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(ble_temp_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_temp_gap_event, NULL);
    if (rc != 0) {
        LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}


static int
ble_temp_gap_event(struct ble_gap_event *event, void *arg)
{

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed */
        LOG(INFO, "connection %s; status=%d\n",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising */
            ble_temp_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        LOG(INFO, "disconnect; reason=%d\n", event->disconnect.reason);

        /* Connection terminated; resume advertising */
        ble_temp_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        LOG(INFO, "adv complete\n");
        ble_temp_advertise();
        break;


    case BLE_GAP_EVENT_MTU:
        LOG(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;

    }

    return 0;
}

static void
on_sync(void)
{
    int rc;

    /* Use privacy */
    rc = ble_hs_id_infer_auto(0, &ble_temp_addr_type);
    assert(rc == 0);

    /* Begin advertising */
    ble_temp_advertise();

    LOG(INFO, "adv started\n");
}


/* Utility Function to append the current temperature to the buffer */
static void 
append_temp_measurement(int16_t temp)
{
	int i = 9;
    for (i = 9; i > 0; i--) {
        g_temp[i] = g_temp[i - 1];
    }
    LOG(DEBUG, "Append {%i} to g_temp buffer\n", temp);
    g_temp[0] = temp;

}

static void 
my_ev_cb(struct os_event *ev)
{
	int16_t temp = 0;
    temp = get_temp_measurement();
    LOG(DEBUG, "Temp Measurement @ every 100ms=%i\n", temp);
    append_temp_measurement(temp);
    return;
}

/* 
 * Task handler to generate an event to measure temperature every 100ms. 
 * The event is added to the default event queue. 
 */

static void
gen_task(void *arg)
{
	while (1) {
		os_time_delay(OS_TICKS_PER_SEC/10);
	    os_eventq_put(os_eventq_dflt_get(), &gen_task_ev);
	}
}

static void
init_tasks(void)
{
    /* Create a task to generate events to measure temperature every 100ms */

    os_task_init(&gen_task_str, "gen_task", gen_task, NULL, GEN_TASK_PRIO,
                 OS_WAIT_FOREVER, gen_task_stack, GEN_TASK_STACK_SZ);

}

/*
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    int rc;

    /* Initialize OS */
    sysinit();

    /* Initialize the logger */
    log_register("ble_temp_sensor_log", &logger, &log_console_handler, NULL, LOG_SYSLEVEL);

    /* Prepare the internal temperature module for measurement */
    nrf_temp_init();
    init_tasks();

    /* Prepare BLE host and GATT server */
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name */
    rc = ble_svc_gap_device_name_set(device_name);
    assert(rc == 0);

    //get_temp_measurement_async();

    /* As the last thing, process events from default event queue */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}
