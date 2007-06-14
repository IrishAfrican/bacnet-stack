/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2006 Steve Karg

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to:
 The Free Software Foundation, Inc.
 59 Temple Place - Suite 330
 Boston, MA  02111-1307, USA.

 As a special exception, if other files instantiate templates or
 use macros or inline functions from this file, or you compile
 this file and link it with other works to produce a work based
 on this file, this file does not by itself cause the resulting
 work to be covered by the GNU General Public License. However
 the source code for this file must still be made available in
 accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work
 based on this file might be covered by the GNU General Public
 License.
 -------------------------------------------
####COPYRIGHTEND####*/
#ifndef TIMESYNC_H
#define TIMESYNC_H

#include <stdint.h>
#include <stdbool.h>
#include "bacdef.h"

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

/* encode service */
    int timesync_utc_encode_apdu(uint8_t * apdu,
        BACNET_DATE * my_date, BACNET_TIME * my_time);
    int timesync_encode_apdu(uint8_t * apdu,
        BACNET_DATE * my_date, BACNET_TIME * my_time);
/* decode the service request only */
    int timesync_decode_service_request(uint8_t * apdu,
        unsigned apdu_len, BACNET_DATE * my_date, BACNET_TIME * my_time);
    int timesync_utc_decode_apdu(uint8_t * apdu,
        unsigned apdu_len, BACNET_DATE * my_date, BACNET_TIME * my_time);
    int timesync_decode_apdu(uint8_t * apdu,
        unsigned apdu_len, BACNET_DATE * my_date, BACNET_TIME * my_time);

#ifdef TEST
#include "ctest.h"
    void testTimeSync(Test * pTest);
#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
