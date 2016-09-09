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

    
#include "../min_cached_segment.h"
#include "../utils/cJSON.h"
#include "../utils/http_client/HTTPClient.h"


#define   MSEC_PER_SEC   1000

#define MSEC_TO_SEC(ms)   ((((int32_t)(ms)) + MSEC_PER_SEC - 1) / MSEC_PER_SEC)

#define RAMDOM_SLEEP_MAX_MS     47

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
        return CSEG_ERROR(EINVAL);
    }else if(status_code == 404){
        return CSEG_ERROR(ENOENT);
    }else if (status_code >= 400 && status_code < 500){
        return CSEG_ERROR(EINVAL);
    }else if ( status_code >= 500 && status_code < 600){
        return CSEG_ERROR(EREMOTEIO);
    }else{
        return CSEG_ERROR(EPROTONOSUPPORT);
    }
        
}


#define  IVR_NAME_FIELD_KEY  "name"
#define  IVR_URI_FIELD_KEY  "uri"
#define  IVR_ERR_INFO_FIELD_KEY "info"

#define MAX_HTTP_RESULT_SIZE  4096

#define HTTP_DEFAULT_RETRY_NUM   2


static inline int http_need_retry(int ret)
{
    switch(ret){
    case HTTP_CLIENT_ERROR_SOCKET_TIME_OUT:
    case HTTP_CLIENT_ERROR_NO_MEMORY:
        return 0;
        break;
    case HTTP_CLIENT_SUCCESS:
        return 0;
        break;
    default:
        return 1;
        break;
    }
    return 0;

}

static int http_post(char * http_uri, 
                     int32_t io_timeout,  //in milli-seconds 
                     char * post_content_type, 
                     char * post_data, int post_len,
                     int32_t  retries,
                     int * status_code,
                     char * result_buf, int *buf_size)
{
    int ret = 0;
    HTTP_CLIENT             HTTPClient;
    HTTP_SESSION_HANDLE     pHTTP = 0;
    uint32_t                  nSize = 0, nTotal = 0;
    
    if(retries <= 0){
        retries = HTTP_DEFAULT_RETRY_NUM;
    }    
    
    while(retries-- > 0){
        pHTTP = 0;
        memset(&HTTPClient, 0, sizeof(HTTP_CLIENT));
        ret = HTTP_CLIENT_SUCCESS;
    
    
        pHTTP = HTTPClientOpenRequest(0);
        if(!pHTTP){
            ret = HTTP_CLIENT_ERROR_NO_MEMORY;    
            break;        
        }    

        if((ret = HTTPClientSetVerb(pHTTP,VerbPost)) != HTTP_CLIENT_SUCCESS)
        {
            break;
        }    
        
        if(post_content_type != NULL){
            if((ret = HTTPClientAddRequestHeaders(pHTTP, "Content-Type", post_content_type, 0)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }
        }else{
            if((ret = HTTPClientAddRequestHeaders(pHTTP, 
                                                  "Content-Type", 
                                                  "application/x-www-form-urlencoded", 0)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }        
        }      

        if((ret = HTTPClientSendRequest(pHTTP, http_uri, post_data,
                    post_len,TRUE, MSEC_TO_SEC(io_timeout), 0)) != HTTP_CLIENT_SUCCESS)
        {
            if(http_need_retry(ret)){
                //cleanup the current HTTP client session hanle
                HTTPClientCloseRequest(&pHTTP);
                pHTTP = 0;
                random_msleep();
                continue;
            }else{
                break;
            }
                
        }     
    

        // Retrieve the the headers and analyze them
        if((ret = HTTPClientRecvResponse(pHTTP,MSEC_TO_SEC(io_timeout))) != HTTP_CLIENT_SUCCESS)
        {
            if(http_need_retry(ret)){//check if need retry
                //cleanup the current HTTP client session hanle
                HTTPClientCloseRequest(&pHTTP);
                pHTTP = 0;
                random_msleep();
                continue;
            }else{
                break;
            }
        }
        
        HTTPClientGetInfo(pHTTP, &HTTPClient);
        if(status_code){
            *status_code = HTTPClient.HTTPStatusCode;
        }    
        

        // Get the data until we get an error or end of stream code
        // printf("Each dot represents %d bytes:\n",HTTP_BUFFER_SIZE );
        if(result_buf != NULL && buf_size != NULL && (*buf_size) != 0){
            nTotal = 0;
            ret = HTTP_CLIENT_SUCCESS;
            while(nTotal < HTTPClient.TotalResponseBodyLength)
            {
                if(nTotal >= (*buf_size)){
                    ret = HTTP_CLIENT_ERROR_NO_MEMORY;
                    break;
                }
                    
                // Set the size of our buffer
                nSize = (*buf_size) - nTotal;   

                // Get the data
                ret = HTTPClientReadData(pHTTP,result_buf+nTotal,nSize, MSEC_TO_SEC(io_timeout), &nSize);
                nTotal += nSize;
                if(ret == HTTP_CLIENT_EOS){
                    ret = HTTP_CLIENT_SUCCESS;
                    break;  // receive complete,
                }else if(ret != HTTP_CLIENT_SUCCESS){
                    break;  //error break;
                }

            }
            
            if(ret == HTTP_CLIENT_SUCCESS){
                (*buf_size) = nTotal;
            }else{      
                if(http_need_retry(ret)){
                    //cleanup the current HTTP client session hanle
                    HTTPClientCloseRequest(&pHTTP);
                    pHTTP = 0;
                    random_msleep();
                    continue;
                }else{
                    break;
                }            
            }//if(ret != HTTP_CLIENT_SUCCESS){
                
        }//if(result_buf != NULL && buf_size != NULL && (*buf_size) != 0){
        
        break; //success, go through
        
    }//while(retries-- > 0){
        
fail:    
    
    if(pHTTP){
        HTTPClientCloseRequest(&pHTTP);
        pHTTP = 0;
    }

    if(ret){
        cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] HTTP POST failed(%d)\n", ret);  
        ret = CSEG_ERROR(EIO);
    }
  
    return ret;
}

static int http_put(char * http_uri, 
                    int32_t io_timeout,  //in milli-seconds 
                    char * content_type, 
                    CachedSegment * segment,
                    int32_t  retries,
                    int * status_code)
{

    int ret = 0;
    HTTP_CLIENT             HTTPClient;
    HTTP_SESSION_HANDLE     pHTTP = 0;
    int32_t                  len = 0;
    fragment *cur_frag;    
    char segment_size_str[32];
    
    
    if(retries <= 0){
        retries = HTTP_DEFAULT_RETRY_NUM;
    }

    while(retries-- > 0){
        pHTTP = 0;
        memset(&HTTPClient, 0, sizeof(HTTP_CLIENT));
        ret = HTTP_CLIENT_SUCCESS;
    
        pHTTP = HTTPClientOpenRequest(0);
        if(!pHTTP){
            ret = HTTP_CLIENT_ERROR_NO_MEMORY;    
            break;       
        }
    
        if((ret = HTTPClientSetVerb(pHTTP,VerbPut)) != HTTP_CLIENT_SUCCESS)
        {
            break;  
        }    
    
        if(content_type != NULL){
            if((ret = HTTPClientAddRequestHeaders(pHTTP, "Content-Type", content_type, 0)) != HTTP_CLIENT_SUCCESS)
            {
                break;  
            }
        }else{
            if((ret = HTTPClientAddRequestHeaders(pHTTP, 
                                                  "Content-Type", 
                                                  "application/x-www-form-urlencoded", 0)) != HTTP_CLIENT_SUCCESS)
            {
                break;  
            }        
        }
        
        memset(segment_size_str,0, 32);
        IToA(segment_size_str, (int)segment->size);
        
        if((ret = HTTPClientAddRequestHeaders(pHTTP, "Content-Length", segment_size_str, 0)) != HTTP_CLIENT_SUCCESS)
        {
            break;  
        }    
        

        if((ret = HTTPClientSendRequest(pHTTP, http_uri, NULL,
                    0, FALSE, MSEC_TO_SEC(io_timeout), 0)) != HTTP_CLIENT_SUCCESS)
        {
            if(http_need_retry(ret)){
                //cleanup the current HTTP client session hanle
                HTTPClientCloseRequest(&pHTTP);
                pHTTP = 0;
                random_msleep();
                continue;
            }else{
                break;
            }  
        }     
    
        /* upload the segment */
        len = 0;
        cur_frag = segment->head;
        ret = HTTP_CLIENT_SUCCESS;
        while(len < segment->size){
            int to_write = MIN(segment->size - len, FRAGMENT_BUF_SIZE);
            
            if(cur_frag == NULL){
                fprintf(stderr, "segment is invalid");
                ret = HTTP_CLIENT_ERROR_INVALID_HANDLE;
                break;
            }
            
            ret  = HTTPClientWriteData(pHTTP, cur_frag->buffer, to_write, MSEC_TO_SEC(io_timeout));
            if(ret != HTTP_CLIENT_SUCCESS){
                break;
            }

            len += to_write;
            cur_frag = cur_frag->next;     
        }
    
        if(ret != HTTP_CLIENT_SUCCESS){
            if(http_need_retry(ret)){
                //cleanup the current HTTP client session hanle
                HTTPClientCloseRequest(&pHTTP);
                pHTTP = 0;
                random_msleep();
                continue;
            }else{
                break;
            }  
        }
    
    
        // Retrieve the the headers and analyze them
        if((ret = HTTPClientRecvResponse(pHTTP,io_timeout)) != HTTP_CLIENT_SUCCESS)
        {
            if(http_need_retry(ret)){
                //cleanup the current HTTP client session hanle
                HTTPClientCloseRequest(&pHTTP);
                pHTTP = 0;
                random_msleep();
                continue;
            }else{
                break;
            }  
        }
        HTTPClientGetInfo(pHTTP, &HTTPClient);
        if(status_code){
            *status_code = HTTPClient.HTTPStatusCode;
        }    
     
        break; //success, go through
    }//while(retries-- > 0){
    
fail:    
    
    if(pHTTP){
        HTTPClientCloseRequest(&pHTTP);
        pHTTP = 0;
    }

    if(ret){
        cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] HTTP PUT failed(%d)\n", ret);  
        ret = CSEG_ERROR(EIO);
    }
  
    return ret;
}


static int create_file(char * ivr_rest_uri, 
                       int32_t io_timeout, 
                       CachedSegment *segment, 
                       char * filename, int filename_size,
                       char * file_uri, int file_uri_size)
{
    char post_data_str[256];
    char * http_response_json = cseg_malloc(MAX_HTTP_RESULT_SIZE);
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
    
    memset(http_response_json, 0, MAX_HTTP_RESULT_SIZE);
    
    //prepare post_data
    sprintf(post_data_str, 
            "op=create&content_type=video%%2Fmp2t&size=%d&start=%.6f&duration=%.6f",
            segment->size,
            segment->start_ts, 
            segment->duration);
        
    //issue HTTP request
    ret = http_post(ivr_rest_uri, 
                    io_timeout,  //in milli-seconds
                    NULL, 
                    post_data_str, strlen(post_data_str), 
                    HTTP_DEFAULT_RETRY_NUM, 
                    &status_code,
                    http_response_json, &response_size);
    if(ret < 0){
        goto failed;       
    }

    //parse the result
    if(status_code >= 200 && status_code < 300){
        json_root = cJSON_Parse(http_response_json);
        if(json_root== NULL){
            ret = CSEG_ERROR(EPROTO);
            cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] HTTP response Json parse failed(%s)\n", http_response_json);
            goto failed;
        }
        json_name = cJSON_GetObjectItem(json_root, IVR_NAME_FIELD_KEY);
        if(json_name && json_name->type == cJSON_String && json_name->valuestring){
            strncpy(filename, json_name->valuestring, filename_size);
            filename[filename_size - 1] = 0;
        }
        json_uri = cJSON_GetObjectItem(json_root, IVR_URI_FIELD_KEY);
        if(json_uri && json_uri->type == cJSON_String && json_uri->valuestring){
            strncpy(file_uri, json_uri->valuestring, file_uri_size);
            file_uri[file_uri_size - 1] = 0;
        }
    }else{
        ret = http_status_to_av_code(status_code);
        if(response_size != 0){
            json_root = cJSON_Parse(http_response_json);
            if(json_root== NULL){
                cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] HTTP create file status code(%d):%s\n", 
                       status_code, "reason unknown");
            }else{
                json_info = cJSON_GetObjectItem(json_root, IVR_ERR_INFO_FIELD_KEY);
                if(json_info && json_info->type == cJSON_String && json_info->valuestring){            
                    cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] HTTP create file status code(%d):%s\n", 
                           status_code, json_info->valuestring);
                }        
            }//if(json_root== NULL)
        }
        goto failed;
        
    }//if(status_code >= 200 && status_code < 300){
    

failed:
    if(json_root){
        cJSON_Delete(json_root); 
        json_root = NULL;
    }
    cseg_free(http_response_json);  
        
    return ret;
}

static int upload_file(CachedSegment *segment, 
                       int32_t io_timeout,  //in milli-seconds
                       char * file_uri)
{
    int status_code = 200;
    int ret = 0;  
    ret = http_put(file_uri, io_timeout, "video/mp2t",
                   segment,
                   HTTP_DEFAULT_RETRY_NUM,
                   &status_code);
    if(ret < 0){
        return ret;
    }
    
    if(status_code < 200 || status_code >= 300){
        ret = http_status_to_av_code(status_code);
        cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] http upload file failed with status(%d)\n", 
                   status_code);       
        goto fail;
    }
    return 0;
fail:
    return ret;
}

static int save_file(char * ivr_rest_uri,
                      int32_t io_timeout,  //in milli-seconds
                      CachedSegment *segment, 
                      char * filename,
                      int success)
{
    char post_data_str[512];  
    int status_code = 200;
    int ret = 0;
    char * http_response_json = cseg_malloc(MAX_HTTP_RESULT_SIZE);
    cJSON * json_root = NULL;
    cJSON * json_info = NULL;   
    int response_size = MAX_HTTP_RESULT_SIZE - 1;
    
    memset(http_response_json, 0, MAX_HTTP_RESULT_SIZE);
    
    //prepare post_data
    if(success){
        sprintf(post_data_str, "op=save&name=%s&size=%d&start=%.6f&duration=%.6f",
                filename,
                segment->size,
                segment->start_ts, 
                segment->duration);
    }else{
        sprintf(post_data_str, "op=fail&name=%s&size=%d&start=%.6f&duration=%.6f",
                filename,
                segment->size,
                segment->start_ts, 
                segment->duration);        
    }

    //issue HTTP request
    ret = http_post(ivr_rest_uri, 
                    io_timeout,
                    NULL, 
                    post_data_str, strlen(post_data_str), 
                    HTTP_DEFAULT_RETRY_NUM, 
                    &status_code,
                    http_response_json, &response_size); 
    if(ret < 0){
        goto failed;
    }

    if(status_code < 200 || status_code >= 300){

        ret = http_status_to_av_code(status_code);
        if(response_size != 0){
            json_root = cJSON_Parse(http_response_json);
            if(json_root== NULL){
                cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] HTTP create file status code(%d):%s\n", 
                       status_code, "reason unknown");   
            }else{
                json_info = cJSON_GetObjectItem(json_root, IVR_ERR_INFO_FIELD_KEY);
                if(json_info && json_info->type == cJSON_String && json_info->valuestring){
                    cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] HTTP create file status code(%d):%s\n", 
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
    cseg_free(http_response_json);  
        
    return ret;

}


static int ivr_init(CachedSegmentContext *cseg)
{
   
    return 0;
}


#define MAX_FILE_NAME 256
#define MAX_URI_LEN 1024

static int ivr_write_segment(CachedSegmentContext *cseg, CachedSegment *segment)
{
    char ivr_rest_uri[MAX_URI_LEN] = "http";
    char file_uri[MAX_URI_LEN];
    char filename[MAX_FILE_NAME];
    char *p;
    int ret = 0;

    if(cseg->filename == NULL || strlen(cseg->filename) == 0){
        ret = CSEG_ERROR(EINVAL);
        cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] http filename absent\n");          
        goto fail;       
    }
    
    if(strlen(cseg->filename) > (MAX_URI_LEN - 5)){
        ret = CSEG_ERROR(EINVAL);
        cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] filename is too long\n");          
        goto fail;
    }

    p = strchr(cseg->filename, ':');  
    if(p){
        strncat(ivr_rest_uri, p, MAX_URI_LEN);
    }else{
        ret = CSEG_ERROR(EINVAL);
        cseg_log(CSEG_LOG_ERROR,  "[cseg_ivr_writer] filename malformat\n");
        goto fail;
    }
    
    //get URI of the file for segment
    ret = create_file(ivr_rest_uri, 
                      cseg->io_timeout,
                      segment, 
                      filename, MAX_FILE_NAME,
                      file_uri, MAX_URI_LEN);
                      
    if(ret){
        goto fail;
    }
   
    if(strlen(filename) == 0 || strlen(file_uri) == 0){
        ret = 1; //cannot upload at the moment
    }else{    
        //upload segment to the file URI
        ret = upload_file(segment, 
                          cseg->io_timeout,
                          file_uri);                      
        if(ret == 0){
            //save the file info to IVR db
            ret = save_file(ivr_rest_uri, 
                            cseg->io_timeout,
                            segment, filename, 1);

        }else{
            //fail the file, remove it from IVR
            ret = save_file(ivr_rest_uri, 
                            cseg->io_timeout,
                            segment, filename, 0);
    
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
    //curl_global_cleanup();
}


CachedSegmentWriter cseg_ivr_writer = {
    .name           = "ivr_writer",
    .long_name      = "IVR cloud storage segment writer", 
    .protos         = "ivr", 
    .init           = ivr_init, 
    .write_segment  = ivr_write_segment, 
    .uninit         = ivr_uninit,
};

