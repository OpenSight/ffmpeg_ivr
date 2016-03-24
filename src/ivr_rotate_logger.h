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

#ifndef IVR_ROTATE_LOGGER_H
#define IVR_ROTATE_LOGGER_H

#include <stdarg.h>

void av_rotate_logger_callback(void* ptr, int level, const char* fmt, va_list vl);

int rotate_logger_init(char *base_name, 
                       int file_size, int file_num);
                       
void rotate_logger_uninit(void);

void check_rotate(void);

#endif /* IVR_COMPAT_H */
