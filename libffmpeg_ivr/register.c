/**
 * This file is part of ffmpeg_ivr
 * 
 * Copyright (C) 2016  OpenSight (www.opensight.cn)
 * 
 * ffmpeg_ivr is an extension of ffmpeg to implements the new feature for IVR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/



    
#include "cached_segment.h"

#define REGISTER_MUXER(x)                                            \
    {                                                                   \
        extern AVOutputFormat ff_##x##_muxer;                           \
        av_register_output_format(&ff_##x##_muxer);                 \
    }
    

#define REGISTER_CSEG_WRITER(x)                                            \
    {                                                                   \
        extern CachedSegmentWriter cseg_##x##_writer;                           \
        register_segment_writer(&cseg_##x##_writer);                 \
    }



void ffmpeg_ivr_register(void)
{
    static int initialized = 0;

    if (initialized)
        return;
    initialized = 1;
    
    REGISTER_CSEG_WRITER(file);
    REGISTER_CSEG_WRITER(dummy);
    REGISTER_CSEG_WRITER(ivr);    
    
    REGISTER_MUXER(cached_segment);

}