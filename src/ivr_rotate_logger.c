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
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "ivr_rotate_logger.h"
#include <libavutil/avutil.h>   
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if HAVE_LIBPTHREAD
#include <pthread.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif


#define MAX_FILE_PATH 2048
static char g_base_name[MAX_FILE_PATH];
static int g_file_size = 0;
static int g_rotate_num = 0;
static int g_fd = -1;


#define LINE_SZ 1024

static void print_time(void);
static void print_to_log(const char* line);
static void check_rotate_internal(void);
static int open_log_file(void);
static void shift_log_file(void);
static int is_too_large(void);
static void close_log_file(void);


void av_rotate_logger_callback(void* ptr, int level, const char* fmt, va_list vl)
{
    static int print_prefix = 1;
    static int count;
    static char prev[LINE_SZ];
    char line[LINE_SZ];
    unsigned tint = 0;

    if (level >= 0) {
        tint = level & 0xff00;
        level &= 0xff;
    }

    if (level > av_log_get_level())
        return;
    
    if(level < 0){
        //no output
        return;
    }
#if HAVE_PTHREADS
    pthread_mutex_lock(&mutex);
#endif
    if(print_prefix){
        print_time();
    }        
    av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);
    
    if (print_prefix && (av_log_get_flags() & AV_LOG_SKIP_REPEATED) && !strcmp(line, prev) &&
        *line && line[strlen(line) - 1] != '\r'){
        count++;
        goto end;
    }
    if (count > 0) {
        char repeat_line[LINE_SZ];
        sprintf(repeat_line, "    Last message repeated %d times\n", count);
        count = 0;
    }
    strcpy(prev, line);
    print_to_log(line);
    
end:
#if HAVE_PTHREADS
    pthread_mutex_unlock(&mutex);
#endif    
}

static void print_time(void)
{
    char time_str_buf[48];
    time_t curtime = time (NULL);
    char *time_str;    
    
    memset(time_str_buf, 0, sizeof(time_str_buf));
    time_str_buf[0] = '[';
    time_str = ctime_r(&curtime, time_str_buf+1);    
    time_str_buf[strlen(time_str_buf) - 1] = ']'; //remove the end NEWLINE char
    time_str_buf[strlen(time_str_buf)] = ' ';
    time_str_buf[strlen(time_str_buf) +1] = 0;       
    
    check_rotate_internal();  
      
    if(g_fd >= 0){
        write(g_fd, time_str_buf, strlen(time_str_buf));      
    }        
}
static void print_to_log(const char* line)
{
    if(g_fd >= 0){
        write(g_fd, line, strlen(line));            
    }
}


int rotate_logger_init(char * base_name, 
                       int file_size, int rotate_num)
{
    if(base_name == NULL || strlen(base_name) == 0 ||
       strlen(base_name) >= MAX_FILE_PATH){
        return -1;
    }
    
    if(g_fd >= 0){
        //already init
        return 0;
    }

    strncpy(g_base_name, base_name, LINE_SZ);
    g_base_name[LINE_SZ - 1] = 0;
    g_file_size = file_size;
    g_rotate_num = rotate_num;
    
    return open_log_file();
                           
}
void rotate_logger_uninit(void)
{
    close_log_file();
}



void check_rotate(void)
{
    
#if HAVE_PTHREADS
    pthread_mutex_lock(&mutex);
#endif
    
    if(g_fd < 0){
        perror("log rotate module is not init");         
        goto end;
    }
    
    check_rotate_internal();
    
end:    
#if HAVE_PTHREADS
    pthread_mutex_unlock(&mutex);
#endif    
}

static void check_rotate_internal(void)
{
    if(is_too_large()){
        close_log_file();
        shift_log_file();
        open_log_file();        
    }
}


static int open_log_file(void)
{
    int ret;
    
    int fd = open(g_base_name, 
                  O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 0777);                
   
    if(fd < 0){
        perror("Open log file failed"); 
        return -1;
    }
    lseek(fd, 0, SEEK_END);
    
    g_fd = fd;
    
    return 0;
}

static void close_log_file(void)
{
    if(g_fd >= 0){
        close(g_fd);
        g_fd = -1;
    }      
}  



static void shift_log_file(void)
{
    char log_file[1024];
    char log_file_old[1024];
     int ret;   
    char *src=NULL;
    char *dst=NULL;
    char *tmp = NULL;
    int i;
    
    memset(log_file, 0, 1024);
    memset(log_file_old, 0, 1024); 

    
    //
    // remove the last rotate file
    if(g_rotate_num > 0){
        snprintf(log_file, sizeof(log_file)-1, "%s.%d", g_base_name, g_rotate_num);        
    }else{
        snprintf(log_file, sizeof(log_file)-1, "%s", g_base_name);        
    }    
    ret = remove(log_file);

    
    //
    //change the file name;
    src=log_file_old;
    dst=log_file;
    tmp = NULL;
    for (i = g_rotate_num - 1; i >= 0; i--) {
        if(i > 0){
            snprintf(src, sizeof(log_file) -1, "%s.%d", g_base_name, i);            
        }else{
            snprintf(src, sizeof(log_file) -1, "%s", g_base_name);
        }        
        ret = rename(src,dst);
        tmp = dst;
        dst = src;
        src = tmp;
    }
}

static int is_too_large(void)
{
    off_t len = 0;
    if(g_fd < 0){
        // no open log file
        return 0;
    }
    
    len = lseek(g_fd, 0, SEEK_CUR);
    if (len < 0) {
        return 0;
    }

    return (len >= g_file_size);
  
}