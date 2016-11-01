/**
 * This file is part of libcseg library, which belongs to ffmpeg_ivr
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
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

#ifndef VF_INTF_H
#define VF_INTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "libcseg.h"


/* private data structure used */
typedef struct vf_private{
    int is_started;
    double start_tp;
    int64_t stream_last_pts[MAX_STREAM_NUM];
    int audio_stream_index;
    int need_cst_adjust;
#ifdef VF_SEI_FILTER      
    uint8_t * frame_buf;
    size_t   buf_size; 
#endif  
}vf_private;

/**
 * allocate and initialize a cseg context
 *
 * This function will malloc a cseg context and configure it according to the 
 * given parameters. And the returned cseg context must be freed with 
 * release_cseg_muxer() at last
 *
 * @param filename URL to write
 * @param streams  the arrays contains the configuration of each stream
 * @param stream_count   the stream number available
 * @param start_sequence  the start sequence for the first segment
 * @param segment_time  the segment duration in seconds
 * @param max_nb_segments  the max segment number in the cached list
 * @param max_seg_size  the max size of one segment in bytes
 * @param pre_recoding_time  the pre-record time in seconds when writer is mute
 * @param start_ts  the start timestamp (in seconds, from epoch) of the first segment
 *                  when it is given 0 or negative, this context would tate the 
 *                  system current time as it.
 * @param need_cst_adjust  change the local timestamp from utc to cst 
 * @param cseg  the output cseg muxer context
 * 
 * @return 0 on success, a negative number on error. 
 *
 */
int vf_init_cseg_muxer(const char * filename,
                       av_stream_t* streams, uint8_t stream_count,
                       double segment_time,
                       int max_nb_segments,
                       uint32_t max_seg_size, 
                       double pre_recoding_time, 
                       double start_ts, 
                       int need_cst_adjust,
                       CachedSegmentContext **cseg);
                    
/**
 * write a video/audio frame to cseg context
 *
 * This function would send one video/audio frame to hls segment of 
 * this context.
 * If the segment is finished, it would be send to ivr cloud
 *
 * @param cseg context the write
 * @param frame_data  pointer to the buffer holding the video/audio data
 *                    which contains the private header
 * @param frame_len   the size of the video/audio data
 * @param codec_type  the codec type for the above data, which is used to 
 *                    check with the stream configuration
 * @param frm_rate     the current fps for video data, or the sample rate for
 *                    video data
 * @param frm_rate    key frame or not, for audio codec, every frame should be 
 *                    key frame
 * 
 * @return 0 on success, a negative number on error. 
 *
 */
int vf_cseg_sendAV(CachedSegmentContext *cseg, 
                   int stream_index,
                   const uint8_t* frame_data, 
                   uint32_t frame_len, 
                   int codec_type, int frame_rate, int key);

/**
 * release the cseg muxer context
 *
 * this function is to free the cseg muxer context which must be 
 * allocated and initialized by init_cseg_muxer() before.
 * all the cseg muxer context initialized by init_cseg_muxer()
 * must be free by this function
 * 
 *
 * @param cseg the cseg muxer to release
 * 
 *
 */
void vf_release_cseg_muxer(CachedSegmentContext *cseg);



#ifdef __cplusplus
}
#endif


#endif //VF_INTF_H





