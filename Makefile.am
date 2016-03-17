AUTOMAKE_OPTIONS=foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/src
AM_CFLAGS = $(libcurl_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS) 
AM_LDFLAGS = $(libcurl_CFLAGS) $(libavformat_LIBS) $(libavutil_CFLAGS) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libffmpeg_ivr.pc


SUBDIRS = . 

lib_LTLIBRARIES=libffmpeg_ivr.la

libffmpeg_ivr_la_SOURCES =  src/register.c \
    src/cached_segment.c \
    src/cached_segment.h \
    src/cJSON.c \
    src/cJSON.h \
    src/seg_writers/cseg_dummy_writer.c \
    src/seg_writers/cseg_file_writer.c \
    src/seg_writers/cseg_ivr_writer.c

libffmpeg_ivr_la_LDFLAGS = -version-info 0:0:0 $(AM_LDFLAGS)   
                    
include_HEADERS = include/ffmpeg_ivr.h

