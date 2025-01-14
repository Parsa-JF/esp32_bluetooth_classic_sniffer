/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "ad_parser.c"

// *****************************************************************************
//
// Advertising Data Parser
//
// *****************************************************************************

#include <stdint.h>
#include <string.h>

#include "bluetooth_data_types.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_cmd.h"

#include "ad_parser.h"

void ad_iterator_init(ad_context_t *context, uint8_t ad_len, const uint8_t *ad_data)
{
    context->data = ad_data;
    context->length = ad_len;
    context->offset = 0;
}

bool ad_iterator_has_more(const ad_context_t *context)
{
    // assert chunk_len and chunk_type are withing buffer
    if ((context->offset + 1u) >= context->length)
    {
        return false;
    }

    // assert chunk_len > 0
    uint8_t chunk_len = context->data[context->offset];
    if (chunk_len == 0u)
    {
        return false;
    }

    // assert complete chunk fits into buffer
    if ((context->offset + 1u + chunk_len) > context->length)
    {
        return false;
    }
    return true;
}

// pre: ad_iterator_has_more() == 1
void ad_iterator_next(ad_context_t *context)
{
    uint8_t chunk_len = context->data[context->offset];
    context->offset += 1u + chunk_len;
}

uint8_t ad_iterator_get_data_len(const ad_context_t *context)
{
    return context->data[context->offset] - 1u;
}

uint8_t ad_iterator_get_data_type(const ad_context_t *context)
{
    return context->data[context->offset + 1u];
}

const uint8_t *ad_iterator_get_data(const ad_context_t *context)
{
    return &context->data[context->offset + 2u];
}

uint16_t little_endian_read_16(const uint8_t *buffer, int pos)
{
    return (uint16_t)(((uint16_t)buffer[pos]) | (((uint16_t)buffer[(pos) + 1]) << 8));
}

const uint8_t bluetooth_base_uuid[] = {0x00, 0x00, 0x00, 0x00, /* - */ 0x00, 0x00, /* - */ 0x10, 0x00, /* - */
                                       0x80, 0x00, /* - */ 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};

void uuid_add_bluetooth_prefix(uint8_t *uuid, uint32_t shortUUID)
{
    (void)memcpy(uuid, bluetooth_base_uuid, 16);
    big_endian_store_32(uuid, 0, shortUUID);
}

void big_endian_store_32(uint8_t *buffer, uint16_t pos, uint32_t value)
{
    buffer[pos++] = value >> 24;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value;
}

void reverse_128(const uint8_t *src, uint8_t *dst)
{
    reverse_bytes(src, dst, 16);
}

void reverse_bytes(const uint8_t *src, uint8_t *dst, int len)
{
    int i;
    for (i = 0; i < len; i++)
        dst[len - 1 - i] = src[i];
}

bool ad_data_contains_uuid16(uint8_t ad_len, const uint8_t *ad_data, uint16_t uuid16)
{
    ad_context_t context;
    ad_iterator_init(&context, ad_len, ad_data);
    while (ad_iterator_has_more(&context))
    {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);

        uint8_t i;
        uint8_t ad_uuid128[16], uuid128_bt[16];

        switch (data_type)
        {
        case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
        case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            for (i = 0u; (i + 2u) <= data_len; i += 2u)
            {
                uint16_t uuid = (uint16_t)little_endian_read_16(data, (int)i);
                if (uuid == uuid16)
                {
                    return true;
                }
            }
            break;
        case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
        case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            uuid_add_bluetooth_prefix(ad_uuid128, uuid16);
            reverse_128(ad_uuid128, uuid128_bt);
            for (i = 0u; (i + 16u) <= data_len; i += 16u)
            {
                if (memcmp(uuid128_bt, &data[i], 16) == 0)
                {
                    return true;
                };
            }
            break;
        default:
            break;
        }
        ad_iterator_next(&context);
    }
    return false;
}

bool ad_data_contains_uuid128(uint8_t ad_len, const uint8_t *ad_data, const uint8_t *uuid128)
{
    ad_context_t context;
    // input in big endian/network order, bluetooth data in little endian
    uint8_t uuid128_le[16];
    reverse_128(uuid128, uuid128_le);
    ad_iterator_init(&context, ad_len, ad_data);
    while (ad_iterator_has_more(&context))
    {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);

        uint8_t i;
        uint8_t ad_uuid128[16];

        switch (data_type)
        {
        case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
        case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            for (i = 0u; (i + 2u) <= data_len; i += 2u)
            {
                uint16_t uuid16 = little_endian_read_16(data, (int)i);
                uuid_add_bluetooth_prefix(ad_uuid128, uuid16);

                if (memcmp(ad_uuid128, uuid128_le, 16) == 0)
                {
                    return true;
                }
            }

            break;
        case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
        case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            for (i = 0u; (i + 16u) <= data_len; i += 16u)
            {
                if (memcmp(uuid128_le, &data[i], 16) == 0)
                {
                    return true;
                }
            }
            break;
        default:
            break;
        }
        ad_iterator_next(&context);
    }
    return false;
}

#include <stdint.h>
#include <stddef.h>

#include "ad_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // ad parser uses uint88_t length
    if (size > 255)
        return 0;
    // test ad iterator by calling simple function that uses it
    ad_data_contains_uuid16(size, data, 0xffff);
    return 0;
}
