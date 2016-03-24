AUTOMAKE_OPTIONS=foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/src
AM_CFLAGS = $(libcurl_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS) \
    $(libavcodec_CFLAGS) $(libavfilter_CFLAGS) $(libswscale_CFLAGS) $(libswresample_CFLAGS)  \
    $(libavdevice_CFLAGS)
AM_LDFLAGS = $(libcurl_LIBS) $(libavformat_LIBS) $(libavutil_LIBS) \
    $(libavcodec_LIBS) $(libavfilter_LIBS) $(libswscale_LIBS) $(libswresample_LIBS) \
    $(libavdevice_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libffmpeg_ivr.pc


SUBDIRS = . 

lib_LTLIBRARIES=libffmpeg_ivr.la

libffmpeg_ivr_la_SOURCES =  src/libffmpeg_ivr/register.c \
    src/libffmpeg_ivr/cached_segment.c \
    src/libffmpeg_ivr/cached_segment.h \
    src/libffmpeg_ivr/cJSON.c \
    src/libffmpeg_ivr/cJSON.h \
    src/libffmpeg_ivr/seg_writers/cseg_dummy_writer.c \
    src/libffmpeg_ivr/seg_writers/cseg_file_writer.c \
    src/libffmpeg_ivr/seg_writers/cseg_ivr_writer.c

libffmpeg_ivr_la_LDFLAGS = -version-info 0:0:0 $(AM_LDFLAGS)   
                    
include_HEADERS = include/libffmpeg_ivr.h



bin_PROGRAMS = ffmpeg_ivr

ffmpeg_ivr_SOURCES = src/cmdutils.c \
    src/cmdutils.h \
    src/cmdutils_common_opts.h \
    src/ffmpeg_ivr.c \
    src/ffmpeg_ivr.h \
    src/ffmpeg_filter.c \
    src/ffmpeg_opt.c \
    src/ivr_rotate_logger.c \
    src/ivr_rotate_logger.h
    

ffmpeg_ivr_LDADD = $(builddir)/libffmpeg_ivr.la

