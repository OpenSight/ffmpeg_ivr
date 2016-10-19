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
#include <stdio.h>
#include <string.h>
#include <errno.h>



#include "../config.h"
#include "../min_cached_segment.h"


#define MIN(a,b) ((a) > (b) ? (b) : (a))


static int file_init(CachedSegmentContext *cseg)
{
    return 0;
}


#define MAX_FILE_NAME 1024
static int file_write_segment(CachedSegmentContext *cseg, CachedSegment *segment, uint32_t queue_len,
                              volatile int *consumer_active)
{
    char base_name[MAX_FILE_NAME];
    char file_name[MAX_FILE_NAME];
    char ext_name[32] = "";
    char *p;
    int ret;
    FILE * f;
    int len;
    fragment *cur_frag;
    
    
    //printf("file_write_segment is calle\n");
    if(strncasecmp(cseg->filename, "file://", 7) == 0){
        strncpy(base_name, cseg->filename + 7, MAX_FILE_NAME);        
        
    }else{
        strncpy(base_name, cseg->filename, MAX_FILE_NAME);
        
    }
    base_name[MAX_FILE_NAME - 1]='\0';

    p = strrchr(base_name, '/');  
    if(p){
        p = strrchr(p, '.');
        if (p){
            strncpy(ext_name, p, 32);
            ext_name[31] = 0;
            *p = '\0';
        }
    }

    
    snprintf(file_name, MAX_FILE_NAME - 1, "%s_%.3f_%.3f_%lld%s", 
             base_name, segment->start_ts, segment->duration, 
             (long long)segment->sequence, ext_name);
             
    f = fopen(file_name, "wb");
    if(!f){
        perror("Open File failed");
        return CSEG_ERROR(errno);
    }
    
    len = 0;
    cur_frag = segment->head;
    while(len < segment->size){
        int to_write = MIN(segment->size - len, FRAGMENT_BUF_SIZE);
        
        if(cur_frag == NULL){
            fclose(f);
            fprintf(stderr, "segment is invalid");
            return CSEG_ERROR(EFAULT);
        }
        ret = fwrite(cur_frag->buffer, 1, to_write, f);
        if(ret != to_write){
            fclose(f);
            perror("fwrite error(%s)");
            return CSEG_ERROR(errno);
        }
        
        len += to_write;
        cur_frag = cur_frag->next;
        ret = 0;        
    }

    fclose(f);
    
    return 0;
}

static void file_uninit(CachedSegmentContext *cseg)
{
   
}
CachedSegmentWriter cseg_file_writer = {
    .name           = "file_writer",
    .long_name      = "OpenSight file segment writer", 
    .protos         = "file", 
    .init           = file_init, 
    .write_segment  = file_write_segment, 
    .uninit         = file_uninit,
};

