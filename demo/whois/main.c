/**************************************************************************
*
* Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

/* command line tool that sends a BACnet service, and displays the reply */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>       /* for time */
#include <errno.h>
#include "bactext.h"
#include "iam.h"
#include "address.h"
#include "config.h"
#include "bacdef.h"
#include "npdu.h"
#include "apdu.h"
#include "device.h"
#include "datalink.h"
/* some demo stuff needed */
#include "filename.h"
#include "handlers.h"
#include "client.h"
#include "txbuf.h"
#if defined(BACDL_MSTP)
#include "rs485.h"
#endif

/* buffer used for receive */
static uint8_t Rx_Buf[MAX_MPDU] = { 0 };

/* global variables used in this file */
static int32_t Target_Object_Instance_Min = 0;
static int32_t Target_Object_Instance_Max = BACNET_MAX_INSTANCE;

static bool Error_Detected = false;

void MyAbortHandler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    uint8_t abort_reason,
    bool server)
{
    /* FIXME: verify src and invoke id */
    (void) src;
    (void) invoke_id;
    (void) server;
    printf("BACnet Abort: %s\r\n", bactext_abort_reason_name(abort_reason));
    Error_Detected = true;
}

void MyRejectHandler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    uint8_t reject_reason)
{
    /* FIXME: verify src and invoke id */
    (void) src;
    (void) invoke_id;
    printf("BACnet Reject: %s\r\n", bactext_reject_reason_name(reject_reason));
    Error_Detected = true;
}

static void Init_Service_Handlers(
    void)
{
    /* Note: this applications doesn't need to handle who-is 
       it is confusing for the user! */
    /* set the handler for all the services we don't implement
       It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler
        (handler_unrecognized_service);
    /* we must implement read property - it's required! */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY,
        handler_read_property);
    /* handle the reply (request) coming back */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_add);
    /* handle any errors coming back */
    apdu_set_abort_handler(MyAbortHandler);
    apdu_set_reject_handler(MyRejectHandler);
}

static void print_address_cache(
    void)
{
    int i, j;
    BACNET_ADDRESS address;
    uint32_t device_id = 0;
    unsigned max_apdu = 0;

    printf("%-7s %-14s %-4s %-5s %-14s\n", "Device", "MAC", "APDU", "SNET",
        "SADR");
    printf("------- -------------- ---- ----- --------------\n");
    for (i = 0; i < MAX_ADDRESS_CACHE; i++) {
        if (address_get_by_index(i, &device_id, &max_apdu, &address)) {
            printf("%7u ", device_id);
            for (j = 0; j < MAX_MAC_LEN; j++) {
                if (j < address.mac_len) {
                    printf("%02X", address.mac[j]);
                } else {
                    printf("  ");
                }
            }
            printf(" %4hu ", max_apdu);
            printf("%5hu ", address.net);
            if (address.net) {
                for (j = 0; j < MAX_MAC_LEN; j++) {
                    if (j < address.len) {
                        printf("%02X", address.adr[j]);
                    } else {
                        printf("  ");
                    }
                    printf(" ");
                }
            }
            printf("\n");
        }
    }
}

static void Init_DataLink(
    void)
{
    char *pEnv = NULL;
#if defined(BACDL_BIP) && BBMD_ENABLED
    long bbmd_port = 0xBAC0;
    long bbmd_address = 0;
    long bbmd_timetolive_seconds = 60000;
#endif

#if defined(BACDL_ALL)
    pEnv = getenv("BACNET_DATALINK");
    if (pEnv) {
        datalink_set(pEnv));
    } else {
        datalink_set(NULL);
    }
#endif

#if defined(BACDL_BIP)
    pEnv = getenv("BACNET_IP_PORT");
    if (pEnv) {
        bip_set_port(strtol(pEnv, NULL, 0));
    } else {
        bip_set_port(0xBAC0);
    }
#elif defined(BACDL_MSTP)
    pEnv = getenv("BACNET_MAX_INFO_FRAMES");
    if (pEnv) {
        dlmstp_set_max_info_frames(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_max_info_frames(1);
    }
    pEnv = getenv("BACNET_MAX_MASTER");
    if (pEnv) {
        dlmstp_set_max_master(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_max_master(127);
    }
    pEnv = getenv("BACNET_MSTP_BAUD");
    if (pEnv) {
        dlmstp_set_baud_rate(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_baud_rate(38400);
    }
    pEnv = getenv("BACNET_MSTP_MAC");
    if (pEnv) {
        dlmstp_set_mac_address(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_mac_address(127);
    }
#endif
    pEnv = getenv("BACNET_APDU_TIMEOUT");
    if (pEnv) {
        apdu_timeout_set(strtol(pEnv, NULL, 0));
        fprintf(stderr, "BACNET_APDU_TIMEOUT=%s\r\n", pEnv);
    } else {
#if defined(BACDL_MSTP)
        apdu_timeout_set(60000);
#endif
    }
    if (!datalink_init(getenv("BACNET_IFACE"))) {
        exit(1);
    }
#if defined(BACDL_BIP) && BBMD_ENABLED
    pEnv = getenv("BACNET_BBMD_PORT");
    if (pEnv) {
        bbmd_port = strtol(pEnv, NULL, 0);
        if (bbmd_port > 0xFFFF) {
            bbmd_port = 0xBAC0;
        }
    }
    pEnv = getenv("BACNET_BBMD_TIMETOLIVE");
    if (pEnv) {
        bbmd_timetolive_seconds = strtol(pEnv, NULL, 0);
        if (bbmd_timetolive_seconds > 0xFFFF) {
            bbmd_timetolive_seconds = 0xFFFF;
        }
    }
    pEnv = getenv("BACNET_BBMD_ADDRESS");
    if (pEnv) {
        bbmd_address = bip_getaddrbyname(pEnv);
        if (bbmd_address) {
            struct in_addr addr;
            addr.s_addr = bbmd_address;
            printf("WhoIs: Registering with BBMD at %s:%ld for %ld seconds\n",
                inet_ntoa(addr), bbmd_port, bbmd_timetolive_seconds);
            bvlc_register_with_bbmd(bbmd_address, bbmd_port,
                bbmd_timetolive_seconds);
        }
    }
#endif
}

int main(int argc, char *argv[]) {
    BACNET_ADDRESS src = {
    0}; /* address where message came from */
    uint16_t pdu_len = 0;
    unsigned timeout = 100;     /* milliseconds */
    time_t total_seconds = 0;
    time_t elapsed_seconds = 0;
    time_t last_seconds = 0;
    time_t current_seconds = 0;
    time_t timeout_seconds = 0;

    if (argc < 2) {
        printf("Usage: %s device-instance | "
            "device-instance-min device-instance-max\r\n",
            filename_remove_path(argv[0]));
        return 0;
    }
    if ((argc > 1) && (strcmp(argv[1], "--help") == 0)) {
        printf("Send BACnet WhoIs service request to a device or\r\n"
            "multiple devices, and wait for responses.  Displays\r\n"
            "any devices found and their network information.\r\n"
            "\r\ndevice-instance:\r\n"
            "BACnet Device Object Instance number that you are trying to\r\n"
            "send a Who-Is service request.  The value should be in\r\n"
            "the range of 0 to 4194303.  A range of values can also be\r\n"
            "specified by using a minimum value and a maximum value.\r\n"
            "\r\nExample:\r\n" "To send a WhoIs request to Device 123\r\n"
            "use the following command:\r\n" "%s 123\r\n"
            "To send a WhoIs request to Devices from 1000 to 9000:\r\n"
            "%s 1000 9000\r\n" "To send a WhoIs request to all devices\r\n"
            "use the following command:\r\n" "%s -1\r\n",
            filename_remove_path(argv[0]), filename_remove_path(argv[0]),
            filename_remove_path(argv[0]));
        return 0;
    }
    /* decode the command line parameters */
    if (argc < 3) {
        Target_Object_Instance_Min = strtol(argv[1], NULL, 0);
        Target_Object_Instance_Max = Target_Object_Instance_Min;
        if (Target_Object_Instance_Min > BACNET_MAX_INSTANCE) {
            fprintf(stderr,
                "object-instance-min=%u - it must be less than %u\r\n",
                Target_Object_Instance_Min, BACNET_MAX_INSTANCE + 1);
            return 1;
        }
    } else {
        Target_Object_Instance_Min = strtol(argv[1], NULL, 0);
        Target_Object_Instance_Max = strtol(argv[2], NULL, 0);
        if (Target_Object_Instance_Min > BACNET_MAX_INSTANCE) {
            fprintf(stderr,
                "object-instance-min=%u - it must be less than %u\r\n",
                Target_Object_Instance_Min, BACNET_MAX_INSTANCE + 1);
            return 1;
        }
        if (Target_Object_Instance_Max > BACNET_MAX_INSTANCE) {
            fprintf(stderr,
                "object-instance-max=%u - it must be less than %u\r\n",
                Target_Object_Instance_Max, BACNET_MAX_INSTANCE + 1);
            return 1;
        }
    }
    /* setup my info */
    Device_Set_Object_Instance_Number(BACNET_MAX_INSTANCE);
    Init_Service_Handlers();
    address_init();
    Init_DataLink();
    /* configure the timeout values */
    last_seconds = time(NULL);
    timeout_seconds = apdu_timeout() / 1000;
    /* send the request */
    Send_WhoIs(Target_Object_Instance_Min, Target_Object_Instance_Max);
    /* loop forever */
    for (;;) {
        /* increment timer - exit if timed out */
        current_seconds = time(NULL);
        /* returns 0 bytes on timeout */
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);
        /* process */
        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
        }
        if (Error_Detected)
            break;
        /* increment timer - exit if timed out */
        elapsed_seconds = current_seconds - last_seconds;
        if (elapsed_seconds) {
#if defined(BACDL_BIP) && BBMD_ENABLED
            bvlc_maintenance_timer(elapsed_seconds);
#endif
        }
        total_seconds += elapsed_seconds;
        if (total_seconds > timeout_seconds)
            break;
        /* keep track of time for next check */
        last_seconds = current_seconds;
    }
    print_address_cache();

    return 0;
}
