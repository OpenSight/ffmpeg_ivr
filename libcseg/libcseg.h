/**
 * This file is part of ffmpeg_sender, which belongs to StreamSwitch
 * project. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 * 
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
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

#ifndef LIBCSEG_H
#define LIBCSEG_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AV_STREAM_TYPE_AUDIO,
    AV_STREAM_TYPE_VIDEO
}av_stream_type_t;

typedef enum {
    AV_STREAM_CODEC_H264,
    AV_STREAM_CODEC_AAC,
    AV_STREAM_CODEC_AAC_WITH_ADTS
}av_stream_codec_t;

typedef struct {
    av_stream_type_t type;
    av_stream_codec_t codec;
    uint8_t  audio_object_type;
    uint32_t audio_sample_rate;
    uint8_t  audio_channel_count;  // https://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Channel_Configurations
}av_stream_t;


#define AV_PACKET_FLAGS_KEY  0x01


/**
 * Undefined timestamp value
 *
 * Usually reported by demuxer that work on containers that do not provide
 * either pts or dts.
 */

#define   NOPTS_VALUE          ((int64_t)-1)

#define   TS_TIME_BASE        ((int64_t)90000)

#define  MAX_STREAM_NUM    4


typedef struct {
    int av_stream_index;
    uint8_t   flags;  // flag: bit8 for is_sync
    int64_t  pts;   // in 90khz
    int64_t  dts;   // in 90khz, -1 if not present
    uint8_t* data;  // for h264, NALU starts with 0x00000001
    size_t  size;
}av_packet_t;

typedef struct CachedSegmentContext CachedSegmentContext;



/**
 * init the libcseg globally
 *
 * Before using the other functions of libcseg, this function
 * must be invoked. It would register all supported writers and 
 * allocate other necessary resource for libcseg
 *
 * 
 * @return 0 on success, a negative number on error. 
 *
 */
int libcseg_init(void);


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
 * @param io_timeout  the timeout time (in milli-seconds) for writer IO
 * @param private_data the user private data
 * @param cseg  the output cseg muxer context
 * 
 * @return 0 on success, a negative number on error. 
 *
 */
int init_cseg_muxer(const char * filename,
                    av_stream_t* streams, uint8_t stream_count,
                    uint64_t start_sequence, 
                    double segment_time,
                    int max_nb_segments,
                    uint32_t max_seg_size, 
                    double pre_recoding_time,  
                    double start_ts,  
                    int32_t io_timeout,
                    void * private_data,                  
                    CachedSegmentContext **cseg);
                    
                    
/**
 * write a packet to the cseg context
 *
 * This function is used to write a video/audio frame to the cseg context which is
 * initialized by init_cseg_muxer(). the cseg muxer would muxing all the packet to
 * the TS muxer and divided them according to HLS
 *
 * @param cseg the cseg muxer  to write
 * @param pkt  the packet to write the cseg muxer
 * 
 * @return 0 on success, a negative number on error. 
 *
 */
int cseg_write_packet(CachedSegmentContext *cseg, av_packet_t *pkt);


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
void release_cseg_muxer(CachedSegmentContext *cseg);



/**
 * get the private data in cseg muxer context
 *
 * the private data is set when init_cseg_muxer() is invoked
 *
 * @param cseg the cseg muxer to release
 * 
 * 
 * @return the private data
 *
 */
void * get_cseg_muxer_private(CachedSegmentContext *cseg);

#ifdef __cplusplus
}
#endif

#endif //LIBCSEG_H





