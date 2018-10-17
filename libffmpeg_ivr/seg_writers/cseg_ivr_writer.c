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
#include <curl/curl.h>

#include "libavutil/avstring.h"
#include "libavutil/opt.h"

#include "libavformat/avformat.h"
    
#include "../cached_segment.h"
#include "../cJSON.h"

#define MIN(a,b) ((a) > (b) ? (b) : (a))

#define HTTP_DEFAULT_RETRY_NUM    2

#define RAMDOM_SLEEP_MAX_MS     47

#define MAX_FILE_NAME 128
#define MAX_URI_LEN 1024
#define MAX_POST_STR_LEN 255

#define  IVR_NAME_FIELD_KEY  "name"
#define  IVR_URI_FIELD_KEY  "uri"
#define  IVR_ERR_INFO_FIELD_KEY "info"

#define MAX_HTTP_RESULT_SIZE  4096

#define ENABLE_CURLOPT_VERBOSE


typedef struct IvrWriterPriv {
    CURL * easyhandle;
    char ivr_rest_uri[MAX_URI_LEN];
    char last_filename[MAX_FILE_NAME];
    char http_response_buf[MAX_HTTP_RESULT_SIZE];
} IvrWriterPriv;

static void random_msleep()
{
    struct timeval tv;
    int ms;
    gettimeofday(&tv, NULL);
    ms = tv.tv_usec % RAMDOM_SLEEP_MAX_MS + 1;
    usleep(ms * 1000);
}

static int http_status_to_av_code(int status_code)
{
    if(status_code == 400){
        return AVERROR_HTTP_BAD_REQUEST;
    }else if(status_code == 404){
        return AVERROR_HTTP_NOT_FOUND;
    }else if (status_code > 400 && status_code < 500){
        return AVERROR_HTTP_OTHER_4XX;
    }else if ( status_code >= 500 && status_code < 600){
        return AVERROR_HTTP_SERVER_ERROR;
    }else{
        return AVERROR_UNKNOWN;
    }    
}


typedef struct HttpBuf{
    char * buf;
    int buf_size; 
    int pos;    
}HttpBuf;

static size_t http_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    int data_size = size * nmemb;
    HttpBuf * http_buf = (HttpBuf *)userdata;
        
    if(data_size > (http_buf->buf_size - http_buf->pos)){
        return 0;
    }
    memcpy(http_buf->buf + http_buf->pos, ptr, data_size);
    http_buf->pos += data_size;
    return data_size;    
}

static size_t http_read_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    HttpBuf * http_buf = (HttpBuf *)userdata;
    int data_size = MIN(size * nmemb, http_buf->buf_size - http_buf->pos);    
    
    memcpy(ptr, http_buf->buf + http_buf->pos, data_size);
    http_buf->pos += data_size;
    return data_size;
}

static int http_post(CURL * easyhandle,
                     char * http_uri, 
                     int32_t io_timeout,  //in milli-seconds 
                     char * post_content_type, 
                     char * post_data, int post_len,
                     int32_t retries,
                     int * status_code,
                     char * result_buf, int *buf_size)
{
    int ret = 0;
    struct curl_slist *headers=NULL;
    char content_type_header[128];
    long status;
    HttpBuf http_buf;
    char err_buf[CURL_ERROR_SIZE] = "unknown";
    CURLcode curl_res = CURLE_OK;

    memset(&http_buf, 0, sizeof(HttpBuf));  
    
    if(retries <= 0){
        retries =  HTTP_DEFAULT_RETRY_NUM;       
    }


    if(post_content_type != NULL){
        memset(content_type_header, 0, 128);
        snprintf(content_type_header, 127, 
                 "Content-Type: %s", post_content_type);
        headers = curl_slist_append(headers, content_type_header);

    }   
    curl_easy_reset(easyhandle);
    
    if(curl_easy_setopt(easyhandle, CURLOPT_ERRORBUFFER, err_buf)){
        ret = AVERROR_EXTERNAL;
        goto fail;               
    }    

    if(curl_easy_setopt(easyhandle, CURLOPT_URL, http_uri)){
        ret = AVERROR_EXTERNAL;
        goto fail;
    }   
        
    if(headers != NULL){
        if(curl_easy_setopt(easyhandle, CURLOPT_HTTPHEADER, headers)){
            ret = AVERROR_EXTERNAL;
            goto fail;               
        }            
    }
    if(curl_easy_setopt(easyhandle, CURLOPT_POSTFIELDS, post_data)){
        ret = AVERROR_EXTERNAL;
        goto fail;          
    }  
    if(curl_easy_setopt(easyhandle, CURLOPT_POSTFIELDSIZE, post_len)){
        ret = AVERROR_EXTERNAL;
        goto fail;             
    }  

    if(io_timeout > 0){
        if(curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT_MS , io_timeout)){
            ret = AVERROR_EXTERNAL;
            goto fail;                  
        }
    } 
    if(result_buf != NULL && buf_size != NULL && (*buf_size) != 0){
        http_buf.buf = result_buf;
        http_buf.buf_size = (*buf_size);
        http_buf.pos = 0;
            
        if(curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, http_write_callback)){
            ret = AVERROR_EXTERNAL;
            goto fail;             
        }
        if(curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &http_buf)){
            ret = AVERROR_EXTERNAL;
            goto fail;                
        }
    }  

 #ifdef ENABLE_CURLOPT_VERBOSE   
    if(curl_easy_setopt(easyhandle, CURLOPT_VERBOSE, 1)){
        ret = AVERROR_EXTERNAL;
        goto fail;               
    }    
 #endif
        
    while(retries-- > 0){
        ret = 0;
        strcpy(err_buf, "unknown");
        http_buf.pos = 0;
        
        if((curl_res = curl_easy_perform(easyhandle)) != CURLE_OK){
            ret = AVERROR_EXTERNAL;            
            if(curl_res == CURLE_OPERATION_TIMEDOUT ){
                break;
            }else{
                //retry
                random_msleep(); //sleep random 1 ~ 50 ms
                continue;
            }
        }
    
        if(curl_easy_getinfo(easyhandle, CURLINFO_RESPONSE_CODE, &status)){
            ret = AVERROR_EXTERNAL;
            break;
        }    
        
        if(status_code){
            *status_code = status;
        }
        if(buf_size != NULL){
            (*buf_size) = http_buf.pos;
        }

        break;   // successful, then exit the loop
    }//while(retries-- > 0){
    
    
fail:    
    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] HTTP POST failed:%s\n", err_buf);        
    }
    if(headers != NULL){
        curl_slist_free_all(headers);
        headers = NULL;
    }
    
    curl_easy_reset(easyhandle);   

    return ret;
}

static int http_put(CURL * easyhandle,
                    char * http_uri, 
                    int32_t io_timeout,  //in milli-seconds 
                    char * content_type, 
                    char * buf, int buf_size,
                    int32_t retries,
                    int * status_code)
{
    int ret = 0;
    struct curl_slist *headers=NULL;
    char content_type_header[128];
    char expect_header[128];
    long status;
    HttpBuf http_buf;
    char err_buf[CURL_ERROR_SIZE] = "unknown";   
    CURLcode curl_res = CURLE_OK; 
    
    if(retries <= 0){
        retries =  HTTP_DEFAULT_RETRY_NUM;       
    }    
    
    memset(&http_buf, 0, sizeof(HttpBuf));

    if(content_type != NULL){
        memset(content_type_header, 0, 128);
        snprintf(content_type_header, 127, 
                 "Content-Type: %s", content_type);
        headers = curl_slist_append(headers, content_type_header);
    }   
    //disable "Expect: 100-continue"  header
    memset(expect_header, 0, 128);
    strcpy(expect_header, "Expect:");
    headers = curl_slist_append(headers, expect_header);   

    curl_easy_reset(easyhandle);

    if(curl_easy_setopt(easyhandle, CURLOPT_ERRORBUFFER, err_buf)){
        ret = AVERROR_EXTERNAL;
        goto fail;               
    }

    if(curl_easy_setopt(easyhandle, CURLOPT_URL, http_uri)){
        ret = AVERROR_EXTERNAL;
        goto fail;                 
    }   
        
    if(curl_easy_setopt(easyhandle, CURLOPT_UPLOAD, 1L)){
        ret = AVERROR_EXTERNAL;
        goto fail;                 
    }   
    
  
    if(curl_easy_setopt(easyhandle, CURLOPT_HTTPHEADER, headers)){
        ret = AVERROR_EXTERNAL;
        goto fail;                   
    }
       
    if(curl_easy_setopt(easyhandle, CURLOPT_INFILESIZE, buf_size)){
        ret = AVERROR_EXTERNAL;
        goto fail;                  
    }    

    if(io_timeout > 0){
        if(curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT_MS , io_timeout)){
            ret = AVERROR_EXTERNAL;
            goto fail;                   
        }
    }   
    
    if(buf != NULL && buf_size != 0){
        http_buf.buf = buf;
        http_buf.buf_size = buf_size;
        http_buf.pos = 0;
            
        if(curl_easy_setopt(easyhandle, CURLOPT_READFUNCTION, http_read_callback)){
            ret = AVERROR_EXTERNAL;
            goto fail;                   
        }
        if(curl_easy_setopt(easyhandle, CURLOPT_READDATA, &http_buf)){
            ret = AVERROR_EXTERNAL;
            goto fail;                   
        }
    }
    
        
#ifdef ENABLE_CURLOPT_VERBOSE   
    if(curl_easy_setopt(easyhandle, CURLOPT_VERBOSE, 1)){
        ret = AVERROR_EXTERNAL;
        goto fail;                    
    }    
#endif

    while(retries-- > 0){
        
        ret = 0;
        strcpy(err_buf, "unknown");
        http_buf.pos = 0;  
        
        if((curl_res = curl_easy_perform(easyhandle)) != CURLE_OK){
            ret = AVERROR_EXTERNAL;            
            if(curl_res == CURLE_OPERATION_TIMEDOUT ){
                break;
            }else{
                //retry
                random_msleep(); //sleep random 1 ~ 50 ms
                continue;
            }
        }
    
        if(curl_easy_getinfo(easyhandle, CURLINFO_RESPONSE_CODE, &status)){
            ret = AVERROR_EXTERNAL;
            break;
        }    
        
        if(status_code){
            *status_code = status;
        }
        
        break;   // successful, then exit the loop
    }//while(retries-- > 0){    
fail:    

    if(ret < 0){
        av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] HTTP PUT failed:%s\n", err_buf);        
    }

    if(headers != NULL){
        curl_slist_free_all(headers);
        headers = NULL;
    }    

    curl_easy_reset(easyhandle);   
    
    return ret;
}


static int create_file(IvrWriterPriv * priv,
                       int32_t io_timeout, 
                       CachedSegment *segment, 
                       char * filename, int filename_size,
                       char * file_uri, int file_uri_size)
{
    char post_data_str[MAX_POST_STR_LEN + 1];
    char * http_response_json = priv->http_response_buf;
    cJSON * json_root = NULL;
    cJSON * json_name = NULL;
    cJSON * json_uri = NULL;    
    cJSON * json_info = NULL;        
    int ret;
    int status_code = 200;
    int response_size = MAX_HTTP_RESULT_SIZE - 1;
    
    if(filename_size){
        filename[0] = 0;
    }
    if(file_uri_size){
        file_uri[0] = 0;
    }    
    

    //av_md5_sum(checksum, segment->buffer, segment->size);
    //av_base64_encode(checksum_b64, 32, checksum, 16);
    //url_encode(checksum_b64_escape, checksum_b64);

    //prepare post_data
    if(strlen(priv->last_filename) == 0){
        snprintf(post_data_str,
                MAX_POST_STR_LEN,
                "op=create&content_type=video%%2Fmp2t&size=%d&start=%.6f&duration=%.6f",
                segment->size,
                segment->start_ts, 
                segment->duration);  
    }else{
        snprintf(post_data_str, 
                 MAX_POST_STR_LEN,
                "op=create&content_type=video%%2Fmp2t&size=%d&start=%.6f&duration=%.6f&last_file_name=%s",
                segment->size,
                segment->start_ts, 
                segment->duration,
                priv->last_filename);          
    }
    post_data_str[MAX_POST_STR_LEN] = 0;

    //issue HTTP request
    ret = http_post(priv->easyhandle,
                    priv->ivr_rest_uri, 
                    io_timeout,
                    NULL, 
                    post_data_str, strlen(post_data_str), 
                    HTTP_DEFAULT_RETRY_NUM,
                    &status_code,
                    http_response_json, &response_size);
    if(ret){
        goto failed;       
    }

    http_response_json[response_size] = 0;
    
    //parse the result
    if(status_code >= 200 && status_code < 300){
        json_root = cJSON_Parse(http_response_json);
        if(json_root== NULL){
            ret = AVERROR(EINVAL);
            av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] HTTP response Json parse failed(%s)\n", http_response_json);
            goto failed;
        }
        json_name = cJSON_GetObjectItem(json_root, IVR_NAME_FIELD_KEY);
        if(json_name && json_name->type == cJSON_String && json_name->valuestring){
            av_strlcpy(filename, json_name->valuestring, filename_size);
        }
        json_uri = cJSON_GetObjectItem(json_root, IVR_URI_FIELD_KEY);
        if(json_uri && json_uri->type == cJSON_String && json_uri->valuestring){
            av_strlcpy(file_uri, json_uri->valuestring, file_uri_size);
        }
    }else{

        ret = http_status_to_av_code(status_code);
        if(response_size != 0){
            json_root = cJSON_Parse(http_response_json);
            if(json_root== NULL){
                av_log(NULL, AV_LOG_ERROR, "[cseg_ivr_writer] HTTP create file (%s) status code(%d):%s\n", 
                       priv->ivr_rest_uri, status_code, http_response_json);
            }else{
                json_info = cJSON_GetObjectItem(json_root, IVR_ERR_INFO_FIELD_KEY);
                if(json_info && json_info->type == cJSON_String && json_info->valuestring){            
                    av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] HTTP create file status code(%d):%s\n", 
                           status_code, json_info->valuestring);
                }        
            }//if(json_root== NULL)
            
        }//if(response_size != 0)
        goto failed;
        
    }
    

failed:
    if(json_root){
        cJSON_Delete(json_root); 
        json_root = NULL;
    }
       
    return ret;
}

static int upload_file(IvrWriterPriv * priv,
                       CachedSegment *segment, 
                       int32_t io_timeout, 
                       char * file_uri)
{
    int status_code = 200;
    int ret = 0;  
    AVIOContext *file_context;
    
    if(strncmp(file_uri, "http://", 7) == 0){
        //for http upload
    
        ret = http_put(priv->easyhandle, 
                       file_uri, io_timeout, "video/mp2t",
                       segment->buffer, segment->size, 
                       HTTP_DEFAULT_RETRY_NUM,
                       &status_code);
        if(ret){
            return ret;
        }
        //Jam(2017-1-2): for some time, Aliyun OSS would return a error status for a normal operation, 
        // but try again we can get the correct result
        if(status_code >= 400){ //try to reconnect for one more time
            random_msleep();        
            ret = http_put(priv->easyhandle, 
                       file_uri, io_timeout, "video/mp2t",
                       segment->buffer, segment->size, 
                       HTTP_DEFAULT_RETRY_NUM,
                       &status_code);
            if(ret){
                return ret;
            }
        } 
        
        if(status_code < 200 || status_code >= 300){
            ret = http_status_to_av_code(status_code);
            av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] http upload file failed with status(%d)\n", 
                       status_code);       
            return ret;
        } 
    }else{
        //for file system
        ret = avio_open(&file_context, file_uri, AVIO_FLAG_WRITE);
        if(ret < 0){
            av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] open fs file failed, avio_open() failed with ret(%d)\n", 
                       ret);  
            return ret;
        }
        
        avio_write(file_context, segment->buffer, segment->size);
        ret = file_context->error;
        avio_closep(&file_context);
        
        if(ret < 0){
            
            av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] write fs file failed, avio_write() failed with ret(%d)\n", 
                       ret); 
            return ret;
        }
    }
    
    return 0;

    
}

static int save_file( IvrWriterPriv * priv,
                      int32_t io_timeout,
                      char * filename,
                      int success)
{
    char post_data_str[MAX_POST_STR_LEN + 1];  
    int status_code = 200;
    int ret = 0;
    char * http_response_json = priv->http_response_buf;
    cJSON * json_root = NULL;
    cJSON * json_info = NULL; 
    int response_size = MAX_HTTP_RESULT_SIZE - 1;    
    
    //prepare post_data
    if(success){
        snprintf(post_data_str, MAX_POST_STR_LEN, "op=save&name=%s", filename);
                
    }else{
        snprintf(post_data_str, MAX_POST_STR_LEN, "op=fail&name=%s", filename);        
    }
    post_data_str[MAX_POST_STR_LEN] = 0;
    
    //issue HTTP request
    ret = http_post(priv->easyhandle,
                    priv->ivr_rest_uri, 
                    io_timeout,
                    NULL, 
                    post_data_str, strlen(post_data_str), 
                    HTTP_DEFAULT_RETRY_NUM,
                    &status_code,
                    http_response_json, &response_size); 
    if(ret){
        goto failed;
    }
    
    http_response_json[response_size] = 0;    
    
    if(status_code < 200 || status_code >= 300){

        ret = http_status_to_av_code(status_code);
        if(response_size != 0){
            json_root = cJSON_Parse(http_response_json);
            if(json_root== NULL){
                av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] HTTP save file (%s) status code(%d):%s\n", 
                       priv->ivr_rest_uri, status_code, http_response_json);                           
            }else{
                json_info = cJSON_GetObjectItem(json_root, IVR_ERR_INFO_FIELD_KEY);
                if(json_info && json_info->type == cJSON_String && json_info->valuestring){
                    av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] HTTP create file status code(%d):%s\n", 
                       status_code, json_info->valuestring);
                }        
            }
        }
        
        goto failed;
      
    }

failed:
    if(json_root){
        cJSON_Delete(json_root); 
        json_root = NULL;
    }       
    return ret;
}




static int ivr_init(CachedSegmentContext *cseg)
{
    int ret = 0; 
    char *p;
    IvrWriterPriv * priv = NULL;

    //init curl lib
    curl_global_init(CURL_GLOBAL_ALL); 
    
    //check filename
    if(cseg->filename == NULL || strlen(cseg->filename) == 0){
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR, "[cseg_ivr_writer] http filename absent\n");          
        goto fail;       
    }
    
    if(strlen(cseg->filename) > (MAX_URI_LEN - 5)){
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] filename is too long\n");          
        goto fail;
    }
    
    priv = (IvrWriterPriv *)av_mallocz(sizeof(IvrWriterPriv));
    if(priv == NULL){
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    // ivr_rest_uri
    strcpy((char *)priv->ivr_rest_uri, "http"); 
    p = strchr(cseg->filename, ':');  
    if(p){
        strncat((char *)priv->ivr_rest_uri, p, (MAX_URI_LEN - 5));
    }else{
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR,  "[cseg_ivr_writer] filename malformat\n");
        goto fail;
    }  

    priv->easyhandle = curl_easy_init();
    if(priv->easyhandle == NULL){
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    
    cseg->writer_priv = priv;
  
    
    return 0;
    
fail:
 
    if(priv != NULL){  
        if(priv->easyhandle != NULL){
            curl_easy_cleanup(priv->easyhandle); 
            priv->easyhandle = NULL;
        }
        av_free(priv);
        priv = NULL;
    }
    
    curl_global_cleanup();
    return ret;       
}


#define MAX_FILE_NAME 1024
#define MAX_URI_LEN 1024

#define FILE_CREATE_TIMEOUT 10000
static int ivr_write_segment(CachedSegmentContext *cseg, CachedSegment *segment)
{
    IvrWriterPriv * priv = (IvrWriterPriv * )cseg->writer_priv;   
    char file_uri[MAX_URI_LEN];
    char filename[MAX_FILE_NAME];
    char *p;
    int ret = 0;

    
    //get URI of the file for segment
    ret = create_file(priv, 
                      FILE_CREATE_TIMEOUT,
                      segment, 
                      filename, MAX_FILE_NAME,
                      file_uri, MAX_URI_LEN);
                      
    if(ret){
        priv->last_filename[0] = 0;
        goto fail;
    }
    
    //clear filename after create
    priv->last_filename[0] = 0;
   
    if(strlen(filename) == 0 || strlen(file_uri) == 0){
        ret = 1; //cannot upload at the moment
        
    }else{    
        
        //upload segment to the file URI
        ret = upload_file(priv, segment, 
                          cseg->writer_timeout,
                          file_uri);                      
        if(ret == 0){
            //Jam: store the successful filename to send at next create
            strcpy(priv->last_filename, filename);

        }else{
            //fail the file, remove it from IVR
            ret = save_file(priv, 
                            FILE_CREATE_TIMEOUT,
                            filename, 0);
            priv->last_filename[0] = 0;
    
        }//if(ret == 0){
            
        if(ret){
            goto fail;
        } 
    }  

fail:
   
    return ret;
}

static void ivr_uninit(CachedSegmentContext *cseg)
{
    
    IvrWriterPriv * priv = (IvrWriterPriv * )cseg->writer_priv;    
    if(priv != NULL){
        if(strlen(priv->last_filename) != 0){
            //save the last file
            save_file(priv, FILE_CREATE_TIMEOUT, 
                      priv->last_filename, 1);   
            priv->last_filename[0] = 0;
        }

        if(priv->easyhandle != NULL){
            curl_easy_cleanup(priv->easyhandle); 
            priv->easyhandle = NULL;
        }
    
        av_free(priv);  
        cseg->writer_priv = NULL;      
    }     
    
    curl_global_cleanup();
}


CachedSegmentWriter cseg_ivr_writer = {
    .name           = "ivr_writer",
    .long_name      = "IVR cloud storage segment writer", 
    .protos         = "ivr", 
    .init           = ivr_init, 
    .write_segment  = ivr_write_segment, 
    .uninit         = ivr_uninit,
};

