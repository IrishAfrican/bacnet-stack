/**************************************************************************
*
* Copyright (C) 2007 Steve Karg <skarg@users.sourceforge.net>
* Inspired by John Stachler.
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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "txbuf.h"
#include "bacdef.h"
#include "bacdcode.h"
#include "apdu.h"
#include "npdu.h"
#include "abort.h"
#include "rpm.h"
#include "handlers.h"
/* demo objects */
#include "device.h"
#include "ai.h"
#include "ao.h"
#include "av.h"
#include "bi.h"
#include "bo.h"
#include "bv.h"
#include "lc.h"
#include "lsp.h"
#include "mso.h"
#if BACFILE
#include "bacfile.h"
#endif

static uint8_t Temp_Buf[MAX_APDU] = { 0 };
static uint8_t Application_Buf[MAX_APDU] = { 0 };

struct property_list_t
{
    const int *pList;
    unsigned count;
};

struct special_property_list_t
{
    struct property_list_t Required;
    struct property_list_t Optional;
    struct property_list_t Proprietary;
};

static unsigned property_list_count(const int *pList)
{
    unsigned property_count = 0;

    if (pList) {
        while (*pList != -1) {
            property_count++;
        }
    }

    return property_count;
}

/* for a given object type, returns the special property list */
static void RPM_Property_List(
    BACNET_OBJECT_TYPE object_type,
    struct special_property_list_t *pPropertyList)
{
    pPropertyList->Required.pList = NULL;
    pPropertyList->Optional.pList = NULL;
    pPropertyList->Proprietary.pList = NULL;
    switch (object_type) {
    case OBJECT_ANALOG_INPUT:
        Analog_Input_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_ANALOG_OUTPUT:
        Analog_Output_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_ANALOG_VALUE:
        Analog_Value_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_BINARY_INPUT:
        Binary_Input_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_BINARY_OUTPUT:
        Binary_Output_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_BINARY_VALUE:
        Binary_Value_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_LIFE_SAFETY_POINT:
        Life_Safety_Point_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_LOAD_CONTROL:
        Load_Control_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    case OBJECT_MULTI_STATE_OUTPUT:
        break;
#if BACFILE
    case OBJECT_FILE:
        BACfile_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
#endif
    case OBJECT_DEVICE:
        Device_Property_Lists(
            &pPropertyList->Required.pList,
            &pPropertyList->Optional.pList,
            &pPropertyList->Proprietary.pList);
        break;
    default:
        break;
    }
    /* fill the count */
    pPropertyList->Required.count =
        property_list_count(pPropertyList->Required.pList);
    pPropertyList->Optional.count =
        property_list_count(pPropertyList->Optional.pList);
    pPropertyList->Proprietary.count =
        property_list_count(pPropertyList->Proprietary.pList);

    return;
}

static int RPM_Object_Property(
    struct special_property_list_t *pPropertyList,
    BACNET_PROPERTY_ID special_property,
    unsigned index)
{
    int property = -1; /* return value */
    unsigned required, optional, proprietary;

    required = pPropertyList->Required.count;
    optional = pPropertyList->Optional.count;
    proprietary = pPropertyList->Proprietary.count;
    if (special_property == PROP_ALL) {
        if (index < required) {
            property = pPropertyList->Required.pList[index];
        } else if (index < (required + optional)) {
            property = pPropertyList->Optional.pList[index];
        } else if (index < (required + optional + proprietary)) {
            property = pPropertyList->Proprietary.pList[index];
        }
    } else if (special_property == PROP_REQUIRED) {
        if (index < required) {
            property = pPropertyList->Required.pList[index];
        }
    } else if (special_property == PROP_OPTIONAL) {
        if (index < optional) {
            property = pPropertyList->Optional.pList[index];
        }
    }

    return property;
}

static unsigned RPM_Object_Property_Count(
    struct special_property_list_t *pPropertyList,
    BACNET_PROPERTY_ID special_property)
{
    unsigned count = 0; /* return value */

    if (special_property == PROP_ALL) {
        count = pPropertyList->Required.count +
            pPropertyList->Optional.count +
            pPropertyList->Proprietary.count;
    } else if (special_property == PROP_REQUIRED) {
        count = pPropertyList->Required.count;
    } else if (special_property == PROP_OPTIONAL) {
        count = pPropertyList->Optional.count;
    }

    return count;
}

/* copy len bytes from src to offset of dest if there is enough space. */
int apdu_copy(uint8_t *dest, uint8_t *src, int offset, int len, int max)
{
    int i;
    int copy_len = 0;

    if (len <= (max-offset)) {
        for (i = 0; i < len; i++) {
            dest[offset+i] = src[i];
            copy_len++;
        }
    }

    return copy_len;
}

/* Encodes the property APDU and returns the length,
   or sets the error, and returns -1 */
int Encode_Property_APDU(
    uint8_t * apdu,
    BACNET_OBJECT_TYPE object_type,
    uint32_t object_instance,
    BACNET_PROPERTY_ID property,
    int32_t array_index,
    BACNET_ERROR_CLASS * error_class,
    BACNET_ERROR_CODE * error_code)
{
    int apdu_len = -1;

    switch(object_type) {
        case OBJECT_DEVICE:
            if ((object_instance == Device_Object_Instance_Number()) ||
                (object_instance == BACNET_MAX_INSTANCE)) {
                apdu_len = Device_Encode_Property_APDU(
                    &apdu[0],
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_ANALOG_INPUT:
            if (Analog_Input_Valid_Instance(object_instance)) {
                apdu_len = Analog_Input_Encode_Property_APDU(
                    &apdu[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_ANALOG_OUTPUT:
            if (!Analog_Output_Valid_Instance(object_instance)) {
                apdu_len = Analog_Output_Encode_Property_APDU(
                    &apdu[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_ANALOG_VALUE:
            if (Analog_Value_Valid_Instance(object_instance)) {
                apdu_len = Analog_Value_Encode_Property_APDU(&Temp_Buf[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_BINARY_INPUT:
            if (Binary_Input_Valid_Instance(object_instance)) {
                apdu_len = Binary_Input_Encode_Property_APDU(
                    &apdu[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_BINARY_OUTPUT:
            if (Binary_Output_Valid_Instance(object_instance)) {
                apdu_len = Binary_Output_Encode_Property_APDU(
                    &apdu[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_BINARY_VALUE:
            if (Binary_Value_Valid_Instance(object_instance)) {
                apdu_len = Binary_Value_Encode_Property_APDU(&Temp_Buf[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_LIFE_SAFETY_POINT:
            if (Life_Safety_Point_Valid_Instance(object_instance)) {
                apdu_len = Life_Safety_Point_Encode_Property_APDU(&Temp_Buf[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_LOAD_CONTROL:
            if (Load_Control_Valid_Instance(object_instance)) {
                apdu_len = Load_Control_Encode_Property_APDU(&Temp_Buf[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
        case OBJECT_MULTI_STATE_OUTPUT:
            if (Multistate_Output_Valid_Instance(object_instance)) {
                apdu_len = Multistate_Output_Encode_Property_APDU(&Temp_Buf[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
#if BACFILE
        case OBJECT_FILE:
            if (bacfile_valid_instance(object_instance)) {
                apdu_len = bacfile_encode_property_apdu(&Temp_Buf[0],
                    object_instance,
                    property,
                    array_index,
                    error_class, error_code);
            } else {
                *error_class = ERROR_CLASS_OBJECT;
                *error_code = ERROR_CODE_UNKNOWN_OBJECT;
            }
            break;
#endif
        default:
            *error_class = ERROR_CLASS_OBJECT;
            *error_code = ERROR_CODE_UNSUPPORTED_OBJECT_TYPE;
            break;
    }

    return apdu_len;
}

void handler_read_property_multiple(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src,
    BACNET_CONFIRMED_SERVICE_DATA * service_data)
{
    int len = 0;
    int copy_len = 0;
    int decode_len = 0;
    int application_data_len = 0;
    int pdu_len = 0;
    BACNET_NPDU_DATA npdu_data;
    int bytes_sent;
    BACNET_ERROR_CLASS error_class = ERROR_CLASS_OBJECT;
    BACNET_ERROR_CODE error_code = ERROR_CODE_UNKNOWN_OBJECT;
    BACNET_ADDRESS my_address;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance = 0;
    int apdu_len = 0;
    int npdu_len;
    BACNET_PROPERTY_ID object_property, temp_object_property;
    int32_t array_index = 0;

    /* jps_debug - see if we are utilizing all the buffer */
    /* memset(&Handler_Transmit_Buffer[0], 0xff, MAX_MPDU);*/
    /* encode the NPDU portion of the packet */
    datalink_get_my_address(&my_address);
    npdu_encode_npdu_data(&npdu_data, false, MESSAGE_PRIORITY_NORMAL);
    npdu_len = npdu_encode_pdu(
        &Handler_Transmit_Buffer[0],
        src, &my_address, &npdu_data);
#if PRINT_ENABLED
    if (service_len <= 0)
        printf("RPM: Unable to decode request!\r\n");
#endif
    /* bad decoding - send an abort */
    if (service_len == 0)
    {
        apdu_len = abort_encode_apdu(
            &Handler_Transmit_Buffer[npdu_len],
            service_data->invoke_id,
            ABORT_REASON_OTHER, true);
#if PRINT_ENABLED
        printf("RPM: Sending Abort!\r\n");
#endif
    } else if (service_data->segmented_message) {
        apdu_len = abort_encode_apdu(
            &Handler_Transmit_Buffer[npdu_len],
            service_data->invoke_id,
            ABORT_REASON_SEGMENTATION_NOT_SUPPORTED, true);
#if PRINT_ENABLED
    printf("RPM: Sending Abort!\r\n");
#endif
    } else {
        /* decode apdu request & encode apdu reply
           encode complex ack, invoke id, service choice */
        apdu_len = rpm_ack_encode_apdu_init(
            &Handler_Transmit_Buffer[npdu_len],
            service_data->invoke_id);
        do
        {
            len = rpm_ack_decode_object_id(
                &service_request[decode_len],
                service_len - decode_len,
                &object_type, &object_instance);
            /* error - end of object? */
            if (len < 0) {
                len = rpm_ack_decode_object_end(
                    &service_request[decode_len],
                    service_len - decode_len);
                if (len == 1) {
                    decode_len++;
                    len = rpm_ack_encode_apdu_object_end(&Temp_Buf[0]);
                    copy_len = apdu_copy(
                        &Handler_Transmit_Buffer[npdu_len], &Temp_Buf[0],
                        apdu_len, len,
                        sizeof(Handler_Transmit_Buffer));
                    if (!copy_len) {
                        apdu_len = abort_encode_apdu(
                            &Handler_Transmit_Buffer[npdu_len],
                            service_data->invoke_id,
                            ABORT_REASON_SEGMENTATION_NOT_SUPPORTED,
                            true);
                        goto RPM_ABORT;
                    }
                } else {
                    apdu_len = abort_encode_apdu(
                        &Handler_Transmit_Buffer[npdu_len],
                        service_data->invoke_id,
                        ABORT_REASON_OTHER, true);
                    goto RPM_ABORT;
                }
                break;
            } else {
                decode_len += len;
            }
            len = rpm_ack_encode_apdu_object_begin(
                &Temp_Buf[0],
                object_type, object_instance);
            copy_len = apdu_copy(
                &Handler_Transmit_Buffer[npdu_len], &Temp_Buf[0],
                apdu_len, len,
                sizeof(Handler_Transmit_Buffer));
            if (!copy_len) {
                apdu_len = abort_encode_apdu(
                    &Handler_Transmit_Buffer[npdu_len],
                    service_data->invoke_id,
                    ABORT_REASON_SEGMENTATION_NOT_SUPPORTED,
                    true);
                goto RPM_ABORT;
            }
            /* do each property of this object of the RPM request */
            do
            {
                len = rpm_decode_object_property(
                    &service_request[decode_len],
                    service_len - decode_len,
                    &object_property,
                    &array_index);
                /* error - end of property list? */
                if (len < 0) {
                    break;
                } else {
                    decode_len += len;
                }
                /* handle the special properties */
                if ((object_property == PROP_ALL) ||
                    (object_property == PROP_REQUIRED) ||
                    (object_property == PROP_OPTIONAL))
                {
                    struct special_property_list_t property_list;
                    unsigned property_count = 0;
                    unsigned index = 0;
                    RPM_Property_List(object_type, &property_list);
                    property_count = RPM_Object_Property_Count(
                        &property_list,
                        object_property);
                    for (index = 0; index < property_count; index++)
                    {
                        temp_object_property = RPM_Object_Property(
                            &property_list,
                            object_property,
                            index);
                        len = rpm_ack_encode_apdu_object_property(
                            &Temp_Buf[0],
                            temp_object_property,
                            array_index);
                        copy_len = apdu_copy(
                            &Handler_Transmit_Buffer[npdu_len], &Temp_Buf[0],
                            apdu_len, len,
                            sizeof(Handler_Transmit_Buffer));
                        if (!copy_len) {
                            apdu_len = abort_encode_apdu(
                                &Handler_Transmit_Buffer[npdu_len],
                                service_data->invoke_id,
                                ABORT_REASON_SEGMENTATION_NOT_SUPPORTED, true);
                            goto RPM_ABORT;
                        }
                        application_data_len = Encode_Property_APDU(
                            &Application_Buf[0],
                            object_type,
                            object_instance,
                            temp_object_property,
                            array_index,
                            &error_class, &error_code);
                        if (application_data_len < 0) {
                            len = rpm_ack_encode_apdu_object_property_error(
                                &Temp_Buf[0],
                                error_class, error_code);
                        } else {
                            len = rpm_ack_encode_apdu_object_property_value(
                                &Temp_Buf[0],
                                &Application_Buf[0],
                                application_data_len);
                        }
                        copy_len = apdu_copy(
                            &Handler_Transmit_Buffer[npdu_len], &Temp_Buf[0],
                            apdu_len, len,
                            sizeof(Handler_Transmit_Buffer));
                        if (!copy_len) {
                            apdu_len = abort_encode_apdu(
                                &Handler_Transmit_Buffer[npdu_len],
                                service_data->invoke_id,
                                ABORT_REASON_SEGMENTATION_NOT_SUPPORTED,
                                true);
                            goto RPM_ABORT;
                        }
                    }
                } else {
                    /* handle an individual property */
                    application_data_len = Encode_Property_APDU(
                        &Application_Buf[0],
                        object_type,
                        object_instance,
                        object_property,
                        array_index,
                        &error_class, &error_code);
                    if (application_data_len < 0) {
                        len = rpm_ack_encode_apdu_object_property_error(
                            &Temp_Buf[0],
                            error_class, error_code);
                    } else {
                        len = rpm_ack_encode_apdu_object_property_value(
                            &Temp_Buf[0],
                            &Application_Buf[0],
                            application_data_len);
                    }
                    copy_len = apdu_copy(
                        &Handler_Transmit_Buffer[npdu_len], &Temp_Buf[0],
                        apdu_len, len,
                        sizeof(Handler_Transmit_Buffer));
                    if (!copy_len) {
                        apdu_len = abort_encode_apdu(
                            &Handler_Transmit_Buffer[npdu_len],
                            service_data->invoke_id,
                            ABORT_REASON_SEGMENTATION_NOT_SUPPORTED,
                            true);
                        goto RPM_ABORT;
                    }
                }
            } while(1);
            if (decode_len >= service_len) {
                break;
            }
        } while(1);
    }
RPM_ABORT:
    pdu_len = apdu_len + npdu_len;
    bytes_sent = datalink_send_pdu(
        src,
        &npdu_data,
        &Handler_Transmit_Buffer[0],
        pdu_len);
}
