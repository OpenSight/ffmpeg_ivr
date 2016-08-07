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



#include <float.h>
#include <stdint.h>
#include <unistd.h>
#include "libavformat/avformat.h"


#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif


    





struct FfmpegCachedSegmentContext {
    const AVClass *context_class;  // Class for private options.
    
    char *filename;
    char *format_options_str;   //mpegts options string
    AVDictionary *format_options; //mpegts options
    
    unsigned number;
    int64_t sequence;
    
    AVOutputFormat *oformat;
    AVFormatContext *avf;
    
    CachedSegment * cur_segment;
    unsigned char * out_buffer;
    
    int64_t start_sequence;
    double start_ts;        //the timestamp for the start_pts, start ts for the whole video
    double time;            // Set by a private option.
    int max_nb_segments;   // Set by a private option.
    uint32_t max_seg_size;      // max size for a segment in bytes, set by a private option
    uint32_t flags;        // enum HLSFlags

    int64_t recording_time;  // segment length in 1/AV_TIME_BASE sec
    int has_video;
    int has_subtitle;

    double pre_recoding_time;   // at least pre_recoding_time should be kept in cached
                                // when segment persistence is disabbled, 
    

    int32_t writer_timeout;
};

extern AVOutputFormat ff_cached_segment_muxer;


void register_cseg(void);

#ifdef __cplusplus
}
#endif