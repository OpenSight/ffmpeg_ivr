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

#ifndef MIN_CACHED_SEGMENT_H
#define MIN_CACHED_SEGMENT_H


#include <float.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"

#include "libcseg.h"

#include "ts_muxer.h"

#ifdef __cplusplus
extern "C" {
#endif


/* error handling */
//#if EDOM > 0
//#define CSEG_ERROR(e) (-(e))   ///< Returns a negative error code from a POSIX error code, to return from library functions.
//#else
/* Some platforms have E* and errno already negated. */
//#define CSEG_ERROR(e) (e)
//#endif

#define CSEG_ERROR(e)  (((e)>0)?(-(e)):(e))

/**
 * Something went really wrong and we will crash now.
 */
#define CSEG_LOG_PANIC     0
/**
 * Something went wrong and recovery is not possible.
 * For example, no header was found for a format which depends
 * on headers or an illegal combination of parameters is used.
 */
#define CSEG_LOG_FATAL     8
/**
 * Something went wrong and cannot losslessly be recovered.
 * However, not all future data is affected.
 */
#define CSEG_LOG_ERROR    16
/**
 * Something somehow does not look correct. This may or may not
 * lead to problems. An example would be the use of '-vstrict -2'.
 */
#define CSEG_LOG_WARNING  24
/**
 * Standard information.
 */
#define CSEG_LOG_INFO     32
/**
 * Detailed information.
 */
#define CSEG_LOG_VERBOSE  40
/**
 * Stuff which is only useful for libav* developers.
 */
#define CSEG_LOG_DEBUG    48
/**
 * Extremely verbose debugging, useful for libav* development.
 */
#define CSEG_LOG_TRACE    56


struct CachedSegmentContext;

typedef struct fragment {
    struct fragment * next;
    uint8_t buffer[0];    
} fragment;


typedef struct CachedSegment {
    //uint8_t *buffer;
    int size;
    double start_ts; /* start timestamp, in seconds */
    double duration; /* in seconds */
    int64_t start_pts; /* start pts, in timebase */
    int64_t pos;
    int buffer_max_size;   
    int64_t sequence;
    struct CachedSegment *next;
    fragment * head;
    fragment * tail;

} CachedSegment;


typedef struct CachedSegmentList {
    uint32_t seg_num;
    struct CachedSegment *first, *last;
} CachedSegmentList;

void init_segment_list(CachedSegmentList * seg_list);
void put_segment_list(CachedSegmentList *seg_list, CachedSegment * segment);
CachedSegment * get_segment_list(CachedSegmentList *seg_list);
void free_segment_list(CachedSegmentList *seg_list);



typedef struct CachedSegmentWriter {
    
    /**
     * the short name of the writer
     */     
    const char *name;

    /**
     * Descriptive name for the writer, meant to be more human-readable
     * than name. You should use the NULL_IF_CONFIG_SMALL() macro
     * to define it.
     */
    const char *long_name;
    
    /**
     * A comma separated list of protocols of the URL for the writer.
     */    
    const char *protos;
    
    struct CachedSegmentWriter * next; //next register writer
    
    //return 0 on success, a negative CSEG_ERROR on failure.
    int (*init)(CachedSegmentContext *cseg);
    
    //return 0 on success, 
    //       1 on writer pause for the moment
    //       otherwise, return a negative number for error
    int (*write_segment)(CachedSegmentContext *cseg, CachedSegment *segment,  
                         uint32_t queue_len, volatile int *consumer_active);
    
    void (*uninit)(CachedSegmentContext *cseg);
    
    
} CachedSegmentWriter;
    



struct CachedSegmentContext {
    
    char *filename;
    
    
    // current segment number
    unsigned number;
    
    // current segment sequence
    int64_t sequence;
    
   
    //the timestamp (seconds from epoch) for the start_pts, start ts for the whole video
    double start_ts;     

    //the duration (in seconds) for one segment   
    double time;            // Set by a private option.
    
    //max segment number in the cached list
    int max_nb_segments;   
    
    //max size (in bytes) for one segment
    uint32_t max_seg_size;      

    // segment length in 1/90K sec
    int64_t recording_time;  
    
    // if there is video stream
    int has_video;
    
    // start pts (in 1/90K sec) for the whole list
    int64_t start_pts;    

    // current segment starting position
    int64_t start_pos;    

    // at least pre_recoding_time should be kept in cached
    // when segment persistence is disabbled,     
    double pre_recoding_time;   
    
    //writer pthread id
    pthread_t consumer_thread_id;
    
    //writer pthread active flag
    volatile int consumer_active;
    
    //writer pthread exit code
    volatile int consumer_exit_code;    
    
    //flag to indicate cahed_list cong
    volatile int cache_list_congested;
    
    // writer IO timeout (in milli-sec)
    int32_t io_timeout;  
  
//#define CONSUMER_ERR_STR_LEN 1024
    //char consumer_err_str[CONSUMER_ERR_STR_LEN];
    
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    
    //segment in cache to write
    CachedSegmentList cached_list;
    
    
    //free segments
    CachedSegmentList free_list;
    
    CachedSegmentWriter *writer;
    void * writer_priv;

    
    //stream configuration
    av_stream_t streams[MAX_STREAM_NUM];
    uint8_t stream_count;
    
    //underlayer ts muxer for the cseg context
    ts_muxer_t* ts_muxer;
    
    // current writing segment
    CachedSegment * cur_segment;
    
    //user's private data
    void * private_data;
};






#ifdef __cplusplus
}
#endif

#endif //MIN_CACHED_SEGMENT_H