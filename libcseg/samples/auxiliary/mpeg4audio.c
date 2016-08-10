/*
 * MPEG-4 Audio common code
 * Copyright (c) 2008 Baptiste Coudurier <baptiste.coudurier@free.fr>
 * Copyright (c) 2009 Alex Converse <alex.converse@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdint.h>
#include "mpeg4audio.h"
#include "get_bits.h"




/* XXX: make sure to update the copies in the different encoders if you change
 * this table */
const int avpriv_mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

const uint8_t ff_mpeg4audio_channels[8] = {
    0, 1, 2, 3, 4, 5, 6, 8
};

static inline int get_object_type(GetBitContext *gb)
{
    int object_type = get_bits(gb, 5);
    if (object_type == AOT_ESCAPE)
        object_type = 32 + get_bits(gb, 6);
    return object_type;
}

static inline int get_sample_rate(GetBitContext *gb, int *index)
{
    *index = get_bits(gb, 4);
    return *index == 0x0f ? get_bits(gb, 24) :
        avpriv_mpeg4audio_sample_rates[*index];
}

int avpriv_mpeg4audio_get_config(MPEG4AudioConfig *c, const uint8_t *buf,
                                 int bit_size, int sync_extension)
{
    GetBitContext gb;
    int specific_config_bitindex, ret;

    if (bit_size <= 0)
        return -1;

    ret = init_get_bits(&gb, buf, bit_size);
    if (ret < 0)
        return ret;

    c->object_type = get_object_type(&gb);
    c->sample_rate = get_sample_rate(&gb, &c->sampling_index);
    c->chan_config = get_bits(&gb, 4);
    if (c->chan_config < FF_ARRAY_ELEMS(ff_mpeg4audio_channels))
        c->channels = ff_mpeg4audio_channels[c->chan_config];
    
    return 0;
}
    


