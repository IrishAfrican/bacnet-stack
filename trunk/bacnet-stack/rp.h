/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2005 Steve Karg

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
#ifndef READPROPERTY_H
#define READPROPERTY_H

#include <stdint.h>
#include <stdbool.h>

// encode service  - use -1 for limit if you want unlimited
int rp_encode_apdu(
  uint8_t *apdu, 
  BACNET_OBJECT_TYPE object_type,
  uint32_t object_instance
  BACNET_PROPERTY_ID object_property,
  int32_t array_index);

// decode the service request only
int rp_decode_service_request(
  uint8_t *apdu,
  unsigned apdu_len,
  BACNET_OBJECT_TYPE *object_type,
  uint32_t *object_instance
  BACNET_PROPERTY_ID *object_property,
  int32_t *array_index);
  
int rp_decode_apdu(
  uint8_t *apdu,
  unsigned apdu_len,
  BACNET_OBJECT_TYPE *object_type,
  uint32_t *object_instance
  BACNET_PROPERTY_ID *object_property,
  int32_t *array_index);
  
#ifdef TEST
void test_ReadProperty(Test * pTest);
void test_ReadPropertyAck(Test * pTest);
#endif

#endif

