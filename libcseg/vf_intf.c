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

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "vf_intf.h"

#include "min_cached_segment.h"


static int32_t io_timeout = 30000;   /* 30 sec */
static uint64_t start_sequence = 0; 

#define AAC_SAMPLE_PER_FRAME   1024

#define MIN_TIMESTAMP   (time_t)1000000000

#define START_PTS       (int64_t)90000

#define PTS_MS_250      22500
#define PTS_MS_500      45000

#define CST_DIFF        (-28800.0)

#define  BUF_INIT_SIZE    (32768)    //32KB

int vf_init_cseg_muxer(const char * filename,
                       av_stream_t* streams, uint8_t stream_count,
                       double segment_time,
                       int max_nb_segments,
                       uint32_t max_seg_size, 
                       double pre_recoding_time, 
                       double start_ts,
                       int need_cst_adjust,
                       CachedSegmentContext **cseg)
{
    vf_private * vf = NULL;
    int i;
    int ret = 0;
    int video_index = -1;

    
    if(stream_count > MAX_STREAM_NUM){
        cseg_log(CSEG_LOG_ERROR,
               "stream count is over the limitation\n");
        return CSEG_ERROR(EINVAL);          
    }
    
    vf = cseg_malloc(sizeof(vf_private));
    if(!vf){
        cseg_log(CSEG_LOG_ERROR,
               "no memory for vf structure\n");
        return CSEG_ERROR(ENOMEM);          
    }
    memset(vf, 0, sizeof(vf_private));
    
    vf->is_started = 0;
    vf->start_tp = -1.0;
    vf->need_cst_adjust = need_cst_adjust;
    vf->audio_stream_index = -1;  
#ifdef VF_SEI_FILTER      
    vf->buf_size = BUF_INIT_SIZE;
    vf->frame_buf = (uint8_t *)cseg_malloc(BUF_INIT_SIZE);
    if(!vf->frame_buf){
        cseg_log(CSEG_LOG_ERROR,
               "no memory for vf structure\n");
        ret = CSEG_ERROR(ENOMEM);     
        goto fail;
    }   
#endif    

    for(i=0;i<stream_count;i++){
        if(streams[i].type == AV_STREAM_TYPE_VIDEO){
            video_index = i;
        }else if(streams[i].type == AV_STREAM_TYPE_AUDIO){
            vf->audio_stream_index = i;            
        }
        
        vf->stream_last_pts[i] = NOPTS_VALUE;

    }
    if(video_index < 0){
        cseg_log(CSEG_LOG_ERROR,
               "video stream absent\n");
        ret = CSEG_ERROR(EINVAL);  
        goto fail;
    }
    
    
    
    ret = init_cseg_muxer(filename,
                          streams, stream_count,
                          start_sequence,
                          segment_time, 
                          max_nb_segments,
                          max_seg_size,
                          pre_recoding_time,
                          start_ts,
                          io_timeout,
                          vf,
                          cseg);
    if(ret){
        goto fail;
    }
    
    return 0;

fail:
    if(vf){
#ifdef VF_SEI_FILTER          
        if(vf->frame_buf){
            cseg_free(vf->frame_buf);
            vf->frame_buf = NULL;
        }
#endif
        cseg_free(vf);
        vf = NULL;
    }
    return ret;
}

#ifdef  VF_SEI_FILTER  
static int vf_realloc_frame_buf(vf_private * vf, size_t size)
{
    //check if need realloc?
    if(vf->buf_size >= size && vf->frame_buf != NULL){
        return 0;
    }
    
    if(vf->buf_size == 0){
        vf->buf_size = BUF_INIT_SIZE;
    }
    
    //find a size bigger than target size
    while(vf->buf_size < size) {
        vf->buf_size = vf->buf_size * 2;
    }
    
    //realloc a buffer
    if(vf->frame_buf != NULL){
        cseg_free(vf->frame_buf);
        vf->frame_buf = NULL;
    }
    vf->frame_buf = (uint8_t *)cseg_malloc(vf->buf_size);
    if(vf->frame_buf == NULL){
        vf->buf_size = 0;
        return CSEG_ERROR(ENOMEM);  
    }
    
    return 0;
    
}


/* NAL unit types */
enum {
    NAL_SLICE           = 1,
    NAL_DPA             = 2,
    NAL_DPB             = 3,
    NAL_DPC             = 4,
    NAL_IDR_SLICE       = 5,
    NAL_SEI             = 6,
    NAL_SPS             = 7,
    NAL_PPS             = 8,
    NAL_AUD             = 9,
    NAL_END_SEQUENCE    = 10,
    NAL_END_STREAM      = 11,
    NAL_FILLER_DATA     = 12,
    NAL_SPS_EXT         = 13,
    NAL_AUXILIARY_SLICE = 19,
    NAL_FF_IGNORE       = 0xff0f001,
};

static void vf_filter_sei(const uint8_t *src, uint8_t *dst, uint32_t *len)
{
    uint32_t src_len = (*len);
    uint32_t dst_len = 0;
    uint8_t *nal_start = NULL;
    uint8_t nal_type = 0;
    uint32_t nal_len = 0;
    uint32_t i;
    
    for(i=0; i + 4 < src_len; i++){
        if(src[i] == 0 && src[i+1] == 0 && src[i+2] == 0 && src[i+3] == 1){ 
            //now, we get a nal at i
            
            if(nal_start != NULL){
                nal_len = src + i - nal_start;
                
                if(nal_type == NAL_AUD || nal_type == NAL_SPS || 
                   nal_type == NAL_PPS || nal_type <= 5){
                    
                    memcpy(dst + dst_len, nal_start, nal_len);
                    dst_len += nal_len;
                }
            }
            
            nal_start = src + i;
            nal_type = src[i+4] & 0x1f;
            
            if(nal_type <= 5){ //VCL
                break;//as for vcl nal, quick copy the left
            }
        }//if(src[i] == 0 && src[i+1] == 0 && src[i+2] == 0 && src[i+3] == 1){
    }//for(i=0; i + 4 < src_len; i++){
    
    if(nal_start != NULL){
        nal_len = src + src_len - nal_start;
                
        if(nal_type == NAL_AUD || nal_type == NAL_SPS || 
            nal_type == NAL_PPS || nal_type <= 5){
                    
            memcpy(dst + dst_len, nal_start, nal_len);
            dst_len += nal_len;
        }
    }    
    
    (*len) = dst_len; 
    
    
}
#endif

int vf_cseg_sendAV(CachedSegmentContext *cseg, 
                   int stream_index,                   
                   const uint8_t* frame_data, 
                   uint32_t frame_len, 
                   int codec_type, int frame_rate, 
                   int key)
{
    vf_private * vf = (vf_private *)get_cseg_muxer_private(cseg);
    av_stream_t *streams = (av_stream_t *)cseg->streams;
    int ret = 0;
    av_packet_t pkt;
    
    if(stream_index > cseg->stream_count){
        cseg_log(CSEG_LOG_ERROR,
               "stream_index invalid\n");
        ret = CSEG_ERROR(EINVAL);
        return ret;        
    }
    
    if(streams[stream_index].codec != codec_type){
        cseg_log(CSEG_LOG_ERROR,
               "codec type error\n");
        ret = CSEG_ERROR(EINVAL);
        return ret;
    }
    
    //check start    
    if(!vf->is_started){
        //check now        
        
        if(streams[stream_index].type == AV_STREAM_TYPE_VIDEO && key){
            struct timeval now;
            struct timespec now_tp;
            gettimeofday(&now, NULL);
            if(now.tv_sec > MIN_TIMESTAMP){                
                ret = clock_gettime(CLOCK_MONOTONIC, &now_tp);
                if(ret){
                    cseg_log(CSEG_LOG_ERROR,
                           "clock_gettime failed\n");
                    ret = CSEG_ERROR(errno);
                    return ret;    
                }
                vf->is_started = 1;
                vf->start_tp = (double)now_tp.tv_sec + (double)now_tp.tv_nsec / 1000000000.0;
                
                if(vf->need_cst_adjust && cseg->start_ts < 0.000001){
                    cseg->start_ts = (double)now.tv_sec + ((double)now.tv_usec) / 1000000.0 
                                     +  CST_DIFF;
                }
            }
        }
        
    }
    if(!vf->is_started){
        //stream not start, ignore this packet
        return 0;
    }
    
    //construct packet
    memset(&pkt, 0, sizeof(av_packet_t));
    pkt.av_stream_index = stream_index;
    if(key){
        pkt.flags |= AV_PACKET_FLAGS_KEY; 
    }
    pkt.data = (uint8_t *)frame_data;
    pkt.size = frame_len;
    pkt.dts = NOPTS_VALUE;

    
#ifdef VF_SEI_FILTER  
    if(streams[stream_index].codec == AV_STREAM_CODEC_H264 && key){
        //trick: delete the SEI nal in the IDR frame
        uint32_t new_frame_len = frame_len;
        
        ret = vf_realloc_frame_buf(vf, frame_len);
        if(ret){
            cseg_log(CSEG_LOG_ERROR,
                       "realloc frame buf failed\n");
            return ret;              
        }
        
        vf_filter_sei(frame_data, vf->frame_buf, &new_frame_len);
        pkt.data = vf->frame_buf;
        pkt.size = new_frame_len;
    }
#endif        
    //calculate pts
    if(vf->stream_last_pts[stream_index] == NOPTS_VALUE){
        struct timespec now_tp;
        double now_d;
        ret = clock_gettime(CLOCK_MONOTONIC, &now_tp);
        if(ret){
            cseg_log(CSEG_LOG_ERROR,
                   "clock_gettime failed\n");
            ret = CSEG_ERROR(errno);
            return ret;            
        }
        now_d = (double)now_tp.tv_sec + (double)now_tp.tv_nsec / 1000000000.0;
        if( now_d - vf->start_tp <= 0.001){
            pkt.pts = START_PTS;
        }else{
            pkt.pts = (now_d - vf->start_tp) * TS_TIME_BASE + START_PTS;            
        }
        
    }else{
        if(streams[stream_index].type == AV_STREAM_TYPE_VIDEO){
            pkt.pts = vf->stream_last_pts[stream_index] + (int64_t)TS_TIME_BASE / frame_rate;
            if(vf->audio_stream_index != -1 && 
               vf->stream_last_pts[vf->audio_stream_index] != NOPTS_VALUE){
                //sync PTS with audio 
                if(pkt.pts + PTS_MS_250 < vf->stream_last_pts[vf->audio_stream_index]){
                    pkt.pts = vf->stream_last_pts[vf->audio_stream_index];
                }else if(pkt.pts > vf->stream_last_pts[vf->audio_stream_index] +  PTS_MS_500){
                    frame_rate <<= 3;
                    pkt.pts = vf->stream_last_pts[stream_index] + (int64_t)TS_TIME_BASE / frame_rate;
                }else if(pkt.pts > vf->stream_last_pts[vf->audio_stream_index] +  PTS_MS_250){
                    frame_rate <<= 1;
                    pkt.pts = vf->stream_last_pts[stream_index] + (int64_t)TS_TIME_BASE / frame_rate;
                }                   
            }
        }else{
            if(streams[stream_index].codec == AV_STREAM_CODEC_AAC ||
               streams[stream_index].codec == AV_STREAM_CODEC_AAC_WITH_ADTS){
                pkt.pts = vf->stream_last_pts[stream_index] + 
                          (int64_t)(TS_TIME_BASE * AAC_SAMPLE_PER_FRAME) / frame_rate;   
            }else{
                cseg_log(CSEG_LOG_ERROR,
                       "audio codec %d not support\n", streams[stream_index].codec);
                ret = CSEG_ERROR(EPFNOSUPPORT);
                return ret;                   
            }
            
        }
        
    }//if(vf->stream_last_pts[stream_index] == NOPTS_VALUE)   
    vf->stream_last_pts[stream_index] = pkt.pts;
    
    return cseg_write_packet(cseg, &pkt);    
                       
}


void vf_release_cseg_muxer(CachedSegmentContext *cseg)
{
    vf_private * vf = (vf_private *)get_cseg_muxer_private(cseg);
    release_cseg_muxer(cseg);
    if(vf){      
#ifdef VF_SEI_FILTER          
        if(vf->frame_buf){
            cseg_free(vf->frame_buf);
            vf->frame_buf = NULL;
        }    
#endif
        cseg_free(vf);
    }
    
}


