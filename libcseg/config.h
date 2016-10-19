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
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAGMENT_BUF_SIZE  65536  //64KB
#define FRAGMENT_SIZE   (FRAGMENT_BUF_SIZE + 16)   //16Bytes is used for fragment header

#define MAX_URL_SIZE 4096

#define   cseg_malloc   malloc
#define   cseg_free     free
#define   cseg_log(level, fmt, args...)   fprintf(stderr, fmt, ##args)
#define   cseg_strdup   strdup

#define   cseg_freep(p)   \
    if((p) != NULL && (*(p)) != NULL){ \
        cseg_free(*(p));               \
        *(p) = NULL;                   \
    }


#ifdef __cplusplus
}
#endif