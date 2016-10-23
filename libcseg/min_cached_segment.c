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
#include <pthread.h>
#include <math.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>


#include "config.h"    
#include "min_cached_segment.h"
#include <sys/time.h>


#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))




//////////////////////////////////
// common basic operation

static size_t cseg_strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = 0;
    while (++len < size && *src)
        *dst++ = *src++;
    if (len <= size)
        *dst = 0;
    return len + strlen(src) - 1;
}
static void cseg_url_split(char *proto, int proto_size,
                  char *authorization, int authorization_size,
                  char *hostname, int hostname_size,
                  int *port_ptr, char *path, int path_size, const char *url)
{
    const char *p, *ls, *ls2, *at, *at2, *col, *brk;

    if (port_ptr)
        *port_ptr = -1;
    if (proto_size > 0)
        proto[0] = 0;
    if (authorization_size > 0)
        authorization[0] = 0;
    if (hostname_size > 0)
        hostname[0] = 0;
    if (path_size > 0)
        path[0] = 0;

    /* parse protocol */
    if ((p = strchr(url, ':'))) {
        cseg_strlcpy(proto, url, MIN(proto_size, p + 1 - url));
        p++; /* skip ':' */
        if (*p == '/')
            p++;
        if (*p == '/')
            p++;
    } else {
        /* no protocol means plain filename */
        cseg_strlcpy(path, url, path_size);
        return;
    }

    /* separate path from hostname */
    ls = strchr(p, '/');
    ls2 = strchr(p, '?');
    if (!ls)
        ls = ls2;
    else if (ls && ls2)
        ls = MIN(ls, ls2);
    if (ls)
        cseg_strlcpy(path, ls, path_size);
    else
        ls = &p[strlen(p)];  // XXX

    /* the rest is hostname, use that to parse auth/port */
    if (ls != p) {
        /* authorization (user[:pass]@hostname) */
        at2 = p;
        while ((at = strchr(p, '@')) && at < ls) {
            cseg_strlcpy(authorization, at2,
                       MIN(authorization_size, at + 1 - at2));
            p = at + 1; /* skip '@' */
        }

        if (*p == '[' && (brk = strchr(p, ']')) && brk < ls) {
            /* [host]:port */
            cseg_strlcpy(hostname, p + 1,
                       MIN(hostname_size, brk - p));
            if (brk[1] == ':' && port_ptr)
                *port_ptr = atoi(brk + 2);
        } else if ((col = strchr(p, ':')) && col < ls) {
            cseg_strlcpy(hostname, p,
                       MIN(col + 1 - p, hostname_size));
            if (port_ptr)
                *port_ptr = atoi(col + 1);
        } else
            cseg_strlcpy(hostname, p,
                       MIN(ls + 1 - p, hostname_size));
    }
}

static int av_match_name(const char *name, const char *names)
{
    const char *p;
    int len, namelen;

    if (!name || !names)
        return 0;

    namelen = strlen(name);
    while ((p = strchr(names, ','))) {
        len = MAX(p - names, namelen);
        if (!strncasecmp(name, names, len))
            return 1;
        names = p + 1;
    }
    return !strcasecmp(name, names);
}

//////////////////////////
//segment operation

CachedSegment * cached_segment_alloc(uint32_t max_size)
{
    CachedSegment * s;
    s = (CachedSegment *)cseg_malloc(sizeof(CachedSegment));
    if(s == NULL){
        return NULL;
    }
    memset(s, 0, sizeof(CachedSegment));
    s->next = NULL;
    s->buffer_max_size = max_size;
    s->start_ts = -1.0;
    s->duration = 0.0;
    //s->buffer = av_malloc(max_size);
    s->size = 0;
    s->start_pts = -1;
    s->pos = 0;
    s->next = NULL;
    s->head = NULL;
    s->tail = NULL;      
    
/*    
    cseg_log(CSEG_LOG_WARNING, 
           "new segment(size:%d) allocated\n",
           max_size);
*/
    return s;    
}
void cached_segment_free(CachedSegment * segment)
{
    /* clean up all fragments */
    fragment * cur = segment->head;
    while(cur != NULL){
        fragment * next = cur->next;
        cseg_free(cur);
        cur = next;
    }
    
    //av_free(segment->buffer);
    cseg_free(segment);
}

void cached_segment_reset(CachedSegment * segment)
{
    segment->start_ts = -1.0;
    segment->duration = 0.0;
    segment->pos = 0;
    segment->next = NULL;
    segment->sequence = 0;
    segment->size = 0;
    segment->start_pts = NOPTS_VALUE;
    segment->tail = segment->head;
}

int write_segment(void *opaque, const uint8_t *buf, size_t buf_size)
{  
    

//    int i;
    CachedSegment * segment = (CachedSegment *) opaque;
    int need_write = 0, ret = 0;    
/*    
    fprintf(stderr, "write_segment(size:%d)\n", buf_size);

    for(i = 0;i<buf_size;i++){
        fprintf(stderr, "%02hhx ",buf[i]);
    }

    fprintf(stderr, "\n");    
*/    
 //   fwrite(buf, 1, buf_size, stdout);
    
    if((segment->buffer_max_size - segment->size) < buf_size){
        need_write = ret = segment->buffer_max_size - segment->size;
    }else{
        need_write = ret = buf_size;
    }
    
    //check head fragment is ready
    if(segment->head == NULL){
        segment->head = segment->tail = (fragment *)cseg_malloc(FRAGMENT_SIZE);
        if(segment->head == NULL){
            //error log
            ret = -1;
            return ret;
        }
        segment->head->next = NULL;
    }
    
    while(need_write){
        int tail_pos = segment->size % FRAGMENT_BUF_SIZE;
        int left_space = FRAGMENT_BUF_SIZE - tail_pos;
        int copy_size = MIN(need_write, left_space);
        memcpy(segment->tail->buffer + tail_pos, buf, copy_size);
        buf += copy_size;
        segment->size += copy_size;
        need_write -= copy_size;
        left_space -= copy_size;
        
        if(left_space == 0){
            if(segment->tail->next == NULL){
                //need a new fragment and append to the list
                fragment * new_fragment = (fragment *)cseg_malloc(FRAGMENT_SIZE);                
                if(new_fragment == NULL){
                    //error log
                    ret = -1;
                    return ret;
                }
                new_fragment->next = NULL;
                segment->tail->next = new_fragment;
             
            }
            
            segment->tail = segment->tail->next; // move tail pointer to the new tail    
        }//if(left_space == 0)
        
    }//while(need_write)
    
    return ret;
} 


//////////////////////////
//segment list operation
void init_segment_list(CachedSegmentList * seg_list)
{
    seg_list->seg_num = 0;
    seg_list->first = seg_list->last = NULL;
}

void put_segment_list(CachedSegmentList *seg_list, CachedSegment * segment)
{
    segment->next = NULL;    
    if(seg_list->first == NULL){
        seg_list->first = segment;
    }else{
        seg_list->last->next = segment;
        
    }
    seg_list->last = segment;
    seg_list->seg_num ++;
}

CachedSegment * get_segment_list(CachedSegmentList *seg_list)
{
    CachedSegment * segment;
    if(seg_list->first == NULL){
        return NULL;
    }
    segment = seg_list->first;
    seg_list->first = segment->next;
    segment->next = NULL;
    if(seg_list->first == NULL){
        seg_list->last = NULL;
    }
    seg_list->seg_num --;   
    return segment;
    
}


void free_segment_list(CachedSegmentList *seg_list)
{
    CachedSegment * segment = seg_list->first;

    while(segment != NULL){
        CachedSegment * segment_to_free = segment;
        segment = segment->next;
        cached_segment_free(segment_to_free);
    }
    init_segment_list(seg_list);
}



/////////////////////////////
//writer operations 


static CachedSegmentWriter * first_writer = NULL;

void register_segment_writer(CachedSegmentWriter * writer)
{
    writer->next = first_writer;
    first_writer = writer;
}

CachedSegmentWriter *find_segment_writer(char * filename)
{
    char hostname[1024], proto[16];
    char auth[1024];
    char path[MAX_URL_SIZE];
    int port;//, i;
    CachedSegmentWriter * writer;
    

    cseg_url_split(proto, sizeof(proto), auth, sizeof(auth),
                 hostname, sizeof(hostname), &port,
                 path, sizeof(path), filename);
    if(strlen(proto) == 0){
        //no protocol, means file
        strcpy(proto, "file");
    }
    for(writer = first_writer; writer != NULL; writer = writer->next){
        if(av_match_name(proto, writer->protos)){
            return writer;
        }        
    }
    return NULL;
    
}

////////////////////////////
//cseg format operations


CachedSegment * get_free_segment(CachedSegmentContext *cseg)
{
    CachedSegment * segment = NULL;
    pthread_mutex_lock(&cseg->mutex);
    if(cseg->free_list.seg_num > 0){
        segment = get_segment_list(&(cseg->free_list));
        cached_segment_reset(segment);
    }
    pthread_mutex_unlock(&cseg->mutex);
    if(segment == NULL){
        segment = cached_segment_alloc(cseg->max_seg_size);
    }
    return segment;
}

void recycle_free_segment(CachedSegmentContext *cseg, CachedSegment * segment)
{
    cached_segment_reset(segment);
    pthread_mutex_lock(&cseg->mutex);        
    put_segment_list(&(cseg->free_list), segment);        
    pthread_mutex_unlock(&cseg->mutex);
}

/* append current segment to the cached segment list */
int append_cur_segment(CachedSegmentContext *cseg)
{
    CachedSegment * segment = cseg->cur_segment;
    
    if(segment == NULL){
        //no current segment, just finished
        return 0;
    }
    
    cseg->cur_segment = NULL;
       
    if(segment->start_ts <= 0.0 ||
       segment->duration < 1 || 
       segment->size <= 0){
        //segment is invalid
        recycle_free_segment(cseg, segment);
        return 0;
    }
        
    pthread_mutex_lock(&cseg->mutex);        
    
    if(cseg->cached_list.seg_num >= cseg->max_nb_segments){ 
        cseg->cache_list_congested = 1; //when cached list reach max segment, consider congested
        cseg_log(CSEG_LOG_WARNING, 
               "One Segment(size:%d, start_ts:%f, duration:%f, pos:%lld, sequence:%lld) "
               "is dropped because of slow writer\n", 
                segment->size, 
                segment->start_ts, segment->duration, 
                segment->pos, segment->sequence); 
        cached_segment_reset(segment);
        put_segment_list(&(cseg->free_list), segment);  

    }else if(cseg->cache_list_congested){
        cseg_log(CSEG_LOG_WARNING, 
               "One Segment(size:%d, start_ts:%f, duration:%f, pos:%lld, sequence:%lld) "
               "is dropped because of cache congested\n", 
                segment->size, 
                segment->start_ts, segment->duration, 
                segment->pos, segment->sequence); 
        cached_segment_reset(segment);
        put_segment_list(&(cseg->free_list), segment);          

    }else{
/*
        cseg_log(CSEG_LOG_INFO, 
                "One Segment(size:%d, start_ts:%f, duration:%f, pos:%lld, sequence:%lld) "
                "is added to cached list(len:%d)\n", 
                segment->size, 
                segment->start_ts, segment->duration, 
                segment->pos, segment->sequence, 
                cseg->cached_list.seg_num); 
*/
        put_segment_list(&(cseg->cached_list), segment);           
    }
    pthread_cond_signal(&cseg->not_empty); //wakeup comsumer    
    
    pthread_mutex_unlock(&cseg->mutex);
    
    return 0;
}

static void * consumer_routine(void *arg)
{
    CachedSegmentContext *cseg = 
        (CachedSegmentContext *)arg;
    CachedSegment * segment = NULL;
    int ret = 0;
   
    
    pthread_mutex_lock(&cseg->mutex);
    while(cseg->consumer_active){
        int keep_seg_num = 0; 
        uint32_t queue_len = 0; 
        
        //try write out all segment in cached list
        while(cseg->consumer_active && (segment = cseg->cached_list.first) != NULL){          
            queue_len = cseg->cached_list.seg_num;
            ret = 0;
            if(cseg->writer != NULL && cseg->writer->write_segment != NULL){                  
                pthread_mutex_unlock(&cseg->mutex);
                //because there is only one comsumer, the first segment is safe to access without lock
                //writer may cost much time to submit the segments
                ret = cseg->writer->write_segment(cseg, segment, queue_len, &(cseg->consumer_active));
                pthread_mutex_lock(&cseg->mutex);
            } 
            if(ret == 0){
                //successful
                
                //remove the segment from cached list
                segment = get_segment_list(&(cseg->cached_list));                
                cached_segment_reset(segment);          
                put_segment_list(&(cseg->free_list), segment);                        
                
            }else if(ret == 1){
                //should keep in fifo
                break;
            }else if(ret < 0){
                //error     
                pthread_mutex_unlock(&cseg->mutex);
                cseg->consumer_exit_code = ret;
                pthread_exit(NULL);     
            }else{
                //not support other ret code, consider error
                pthread_mutex_unlock(&cseg->mutex);
                cseg_log(CSEG_LOG_ERROR,  "[cseg] cannot support the writer return code:%d\n", ret);        
                cseg->consumer_exit_code = CSEG_ERROR(EINVAL);
                pthread_exit(NULL); 
            }
        }// while((segment = cseg->cached_list.first) != NULL){
        
        //clean up the expired segments
        if(cseg->pre_recoding_time < 0.001){
            keep_seg_num = 0;
        }else{
            keep_seg_num = MIN((uint32_t)ceil(cseg->pre_recoding_time / cseg->time), 
                            cseg->max_nb_segments - 1);    
        }
        while(cseg->cached_list.seg_num > keep_seg_num){
            //remove the segment from cached list
            segment = get_segment_list(&(cseg->cached_list));                
            cached_segment_reset(segment);          
            put_segment_list(&(cseg->free_list), segment);              
        }//while(cseg->cached_list.seg_num > keep_seg_num){
        
        cseg->cache_list_congested = 0;
        
        if(cseg->consumer_active){
            pthread_cond_wait(&(cseg->not_empty), &(cseg->mutex)); //wait for next time
        }
        
    }//while(cseg->consumer_active){
    pthread_mutex_unlock(&cseg->mutex);

    return NULL;    
}


static int cseg_start(CachedSegmentContext *cseg)
{
    int err = 0;
    CachedSegment * segment = NULL;
    

    segment = get_free_segment(cseg);
    if(!segment){
        err = CSEG_ERROR(ENOMEM);
        return err;      
    }
    
    err = ts_muxer_set_avio_context(cseg->ts_muxer, segment, write_segment);
    if(err){
        err = CSEG_ERROR(EINVAL);
        recycle_free_segment(cseg, segment);
        return err;
    }
    
    cseg->cur_segment = segment;
    cseg->number++;   
    segment->sequence = cseg->sequence++;
    
    ts_muxer_write_header(cseg->ts_muxer);

    return 0;
}

int libcseg_init(void)
{
#define REGISTER_CSEG_WRITER(x)                                            \
    {                                                                   \
        extern CachedSegmentWriter cseg_##x##_writer;                           \
        register_segment_writer(&cseg_##x##_writer);                 \
    }
    
    static int initialized = 0;

    if (initialized)
        return 0;
    initialized = 1;
    
    REGISTER_CSEG_WRITER(file);
    REGISTER_CSEG_WRITER(dummy);
    REGISTER_CSEG_WRITER(ivr);      
    return 0;
}


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
                    CachedSegmentContext **cseg)
{
    CachedSegmentContext *new_cseg = NULL;
    int ret, i;    
    
    //check parameters
    if(cseg == NULL){
        return CSEG_ERROR(EINVAL);
    }
    if(segment_time < 1.0){
        cseg_log(CSEG_LOG_ERROR,
               "segment time cannot be less than 1.0 second\n");    
        return CSEG_ERROR(EINVAL);
    }    

    if(streams == NULL || stream_count == 0){
        cseg_log(CSEG_LOG_ERROR,
               "streams config invalid\n");
        return CSEG_ERROR(EINVAL);        
    }
    if(stream_count > MAX_STREAM_NUM){
        cseg_log(CSEG_LOG_ERROR,
               "stream count is over the limitation\n");
        return CSEG_ERROR(EINVAL);          
    }
    
       
    //start to initialize the cseg    
    new_cseg = cseg_malloc(sizeof(CachedSegmentContext));
    if(new_cseg == NULL){
        return CSEG_ERROR(ENOMEM);
    }
    memset(new_cseg, 0, sizeof(CachedSegmentContext));
    
    pthread_mutex_init(&new_cseg->mutex, NULL);
    pthread_cond_init(&new_cseg->not_empty, NULL);
    new_cseg->time = segment_time;
    new_cseg->sequence       = (int64_t)start_sequence;
    new_cseg->recording_time = new_cseg->time * TS_TIME_BASE;
    new_cseg->start_pts = NOPTS_VALUE;
    new_cseg->start_pos = 0;
    new_cseg->number = 0;
    new_cseg->consumer_exit_code = 0;
    new_cseg->max_nb_segments = max_nb_segments;
    new_cseg->max_seg_size = max_seg_size;
    new_cseg->pre_recoding_time = pre_recoding_time;
    new_cseg->start_ts = start_ts;
    new_cseg->stream_count = stream_count;
    new_cseg->private_data = private_data;
    new_cseg->io_timeout = io_timeout;
    

    for (i = 0; i < stream_count; i++) {
        new_cseg->has_video +=
            streams[i].type == AV_STREAM_TYPE_VIDEO;
        memcpy(&(new_cseg->streams[i]), &(streams[i]), sizeof(av_stream_t)); 
    }

    if (new_cseg->has_video > 1)
        cseg_log(CSEG_LOG_WARNING,
               "More than a single video stream present, "
               "expect issues decoding it.\n");


    new_cseg->filename = cseg_strdup(filename);
    init_segment_list(&new_cseg->cached_list);
    init_segment_list(&new_cseg->free_list);    

        
    if((new_cseg->ts_muxer = new_ts_muxer(new_cseg->streams, stream_count)) == NULL){
        ret = CSEG_ERROR(ENOMEM);
        goto fail;
    }

    if ((ret = cseg_start(new_cseg)) < 0)
        goto fail;

    
    //find writer
    new_cseg->writer = find_segment_writer(new_cseg->filename);
    if(!new_cseg->writer){
        cseg_log(CSEG_LOG_ERROR, "No writer found for url:%s\n", new_cseg->filename);
        ret = CSEG_ERROR(ENOSYS);  
        goto fail;
    }
    if(new_cseg->writer->init){
        ret = new_cseg->writer->init(new_cseg);
        if(ret<0){
            cseg_log(CSEG_LOG_ERROR, "Writer(%s) init failed for url:%s\n", 
            new_cseg->writer->name,
            new_cseg->filename);  
            new_cseg->writer = NULL;
            goto fail;
        }
    }   
    
    //successful write header, start consumer
    new_cseg->consumer_active = 1;
    new_cseg->consumer_exit_code = 0;
    ret = pthread_create(&(new_cseg->consumer_thread_id), NULL, consumer_routine, new_cseg);
    if(ret){
        ret = CSEG_ERROR(ret);
        cseg_log(CSEG_LOG_ERROR, "Start consumer thread failed");
        new_cseg->consumer_active = 0;
        new_cseg->consumer_thread_id  = 0;
        goto fail;
    }    
    
    (*cseg) = new_cseg;
    
fail:
    if (ret) {
        if(new_cseg->writer){
            if(new_cseg->writer->uninit){
                new_cseg->writer->uninit(new_cseg);
            }
            new_cseg->writer = NULL;
        }
        
        
        if(new_cseg->cur_segment){
            cached_segment_free(new_cseg->cur_segment);
            new_cseg->cur_segment = NULL;
        }
        
        if(new_cseg->ts_muxer){
            free_ts_muxer(new_cseg->ts_muxer);
            new_cseg->ts_muxer = NULL;            
        }
        
        cseg_freep(&new_cseg->filename);
        
        pthread_cond_destroy(&new_cseg->not_empty);
        pthread_mutex_destroy(&new_cseg->mutex);  
        
        cseg_freep(&new_cseg);
   
    }
    return ret;
}




int cseg_write_packet(CachedSegmentContext *cseg, av_packet_t *pkt)
{

    av_stream_t *st = &(cseg->streams[pkt->av_stream_index]);
    int64_t end_pts = cseg->recording_time * cseg->number;
    int is_ref_pkt = 1;
    int ret, can_split = 1;
    int stream_index = pkt->av_stream_index;
//    int i;
   
    if(cseg->consumer_exit_code){
        //consumer error
/*
        cseg_log(CSEG_LOG_ERROR, 
               "Consumer Error:%s\n",
               cseg->consumer_err_str[0] == 0?
               "unknown":cseg->consumer_err_str); 
*/
        return cseg->consumer_exit_code;
    }
    
    if(cseg->cur_segment == NULL){
        //no current segment
        return CSEG_ERROR(EFAULT);;
    }

    if (cseg->start_pts == NOPTS_VALUE) {
        cseg->start_pts = pkt->pts;
        if(cseg->start_pts != NOPTS_VALUE){
            //start_pts is ready, check start_ts
            if(cseg->start_ts < 0.000001){
                //get current time for start ts
                struct timeval tv;
                gettimeofday(&tv, NULL);
                if(tv.tv_sec < 31536000){   //not valid
                    cseg->start_pts = NOPTS_VALUE;
                    cseg_log(CSEG_LOG_ERROR, 
                           "gettimeofday error, the timestamp is invalid\n");                     
                    return CSEG_ERROR(EFAULT);
                }
                cseg->start_ts = (double)tv.tv_sec + ((double)tv.tv_usec) / 1000000.0;
            }//if(cseg->start_ts < 0.0){
        }//if(cseg->start_pts != AV_NOPTS_VALUE){
    }//if (cseg->start_pts == AV_NOPTS_VALUE) {
        
    if(cseg->cur_segment->start_pts == NOPTS_VALUE){
        cseg->cur_segment->start_pts = pkt->pts;
        if(cseg->cur_segment->start_pts != NOPTS_VALUE && 
           cseg->cur_segment->start_ts <= 0.0){
            cseg->cur_segment->start_ts = cseg->start_ts;          
        }        
    }
    
/*   
    fprintf(stderr, "dump packet(size:%d)\n", (int)pkt->size);

    for(i = 0;i<10;i++){
        fprintf(stderr, "%02hhx ",pkt->data[i]);
    }

    fprintf(stderr, "\n");
*/   
    if (cseg->has_video) {
        can_split = st->type == AV_STREAM_TYPE_VIDEO &&
                    pkt->flags & AV_PACKET_FLAGS_KEY;
        is_ref_pkt = st->type == AV_STREAM_TYPE_VIDEO;
    }
    
    if (pkt->pts == NOPTS_VALUE)
        is_ref_pkt = can_split = 0;

    if (is_ref_pkt){
        cseg->cur_segment->duration = (double)(pkt->pts - cseg->cur_segment->start_pts)
                                   / (double)TS_TIME_BASE;
    }

    if (can_split && ((pkt->pts - cseg->start_pts) >= end_pts)) {
        
        
/*        
        printf("pts:%lld, start_pts:%lld, end_pts:%lld, split_end_pts:%lld\n",
               (long long)pkt->pts, (long long)cseg->start_pts, (long long)cseg->end_pts, (long long)end_pts);
*/
        // finished the current segment
        do{
            int64_t cur_segment_size = 0;
            
            ts_muxer_write_packet(cseg->ts_muxer, NULL);
                
            ts_muxer_set_avio_context(cseg->ts_muxer, NULL, NULL);
                    
            cur_segment_size = cseg->cur_segment->size;

            ret = append_cur_segment(cseg); // lose the control of cseg->cur_segment
            if (ret < 0){
                return ret;
            }
            cseg->start_pos += cur_segment_size;
        }while(0);
       
        //init new segment
        ret = cseg_start(cseg);
        if (ret < 0)
            return ret;
        cseg->cur_segment->start_ts = (double)(pkt->pts - cseg->start_pts)
                                            / (double)TS_TIME_BASE + cseg->start_ts;        
        cseg->cur_segment->pos = cseg->start_pos;
        cseg->cur_segment->start_pts = pkt->pts;
        cseg->cur_segment->duration = 0.0;
        
    }//if (can_split && av_compare_ts(pkt->pts - cseg->start_pts, st->time_base,
    
    ret = ts_muxer_write_packet(cseg->ts_muxer, pkt);
    if(ret){
        cseg_log(CSEG_LOG_ERROR, 
               "ts_muxer_write_packet faile(%d)\n", ret);         
        ret = CSEG_ERROR(EFAULT);
    }

    return ret;
}

void release_cseg_muxer(CachedSegmentContext *cseg)
{
    
    if(cseg == NULL){
        return;
    }    
    
    
    // flush the last segment
    do{
        ts_muxer_write_packet(cseg->ts_muxer, NULL); //flush muxer to IO context

        ts_muxer_set_avio_context(cseg->ts_muxer, NULL, NULL);
        
        pthread_mutex_lock(&cseg->mutex);
        
        if(cseg->cur_segment != NULL){
            cached_segment_reset(cseg->cur_segment);            
            put_segment_list(&(cseg->free_list), cseg->cur_segment);
            cseg->cur_segment = NULL;                
        }
        pthread_mutex_unlock(&cseg->mutex); 
    }while(0);    //do{
          
    if(cseg->consumer_thread_id != 0){
        void * res;
        int ret;
        pthread_mutex_lock(&cseg->mutex); 
        cseg->consumer_active = 0;          
        pthread_cond_signal(&cseg->not_empty); //wakeup comsumer
        pthread_mutex_unlock(&cseg->mutex);  
                
        ret = pthread_join(cseg->consumer_thread_id, &res);
        if (ret != 0){
            cseg_log(CSEG_LOG_ERROR, "stop consumer thread failed\n");
        }
        cseg->consumer_thread_id = 0;
        
    }
    
    if(cseg->writer){
        if(cseg->writer->uninit){
            cseg->writer->uninit(cseg);
        }
        cseg->writer = NULL;
    }    
        
    if(cseg->ts_muxer){
        free_ts_muxer(cseg->ts_muxer);
        cseg->ts_muxer = NULL;            
    }

    free_segment_list(&(cseg->cached_list));
    free_segment_list(&(cseg->free_list));

    cseg_freep(&cseg->filename);

 
    pthread_cond_destroy(&cseg->not_empty);
    pthread_mutex_destroy(&cseg->mutex); 
    
    cseg_free(cseg);

}

void * get_cseg_muxer_private(CachedSegmentContext *cseg)
{
    if(cseg == NULL){
        return NULL;
    }else{
        return cseg->private_data;
    }

}