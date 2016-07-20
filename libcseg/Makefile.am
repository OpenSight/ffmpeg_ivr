AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)
AM_CFLAGS = $(libavformat_CFLAGS) $(libavutil_CFLAGS) \
    $(libavcodec_CFLAGS) $(libavfilter_CFLAGS) $(libswscale_CFLAGS) $(libswresample_CFLAGS)  \
    $(libavdevice_CFLAGS)
AM_LDFLAGS = $(libavformat_LIBS) $(libavutil_LIBS) \
    $(libavcodec_LIBS) $(libavfilter_LIBS) $(libswscale_LIBS) $(libswresample_LIBS) \
    $(libavdevice_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(srcdir)/libcseg.pc

SUBDIRS = . samples

lib_LTLIBRARIES=libcseg.la

libcseg_la_SOURCES =  config.h \
    min_cached_segment.c \
    min_cached_segment.h \
    utils/cJSON.c \
    utils/cJSON.h \
    utils/http_client/HTTPClient.c \
    utils/http_client/HTTPClient.h  \
    utils/http_client/HTTPClientAuth.c \
    utils/http_client/HTTPClientAuth.h \
    utils/http_client/HTTPClientCommon.h \
    utils/http_client/HTTPClientString.h \
    utils/http_client/HTTPClientString.c \
    utils/http_client/HTTPClientWrapper.h \
    utils/http_client/HTTPClientWrapper.c \
    seg_writers/cseg_dummy_writer.c \
    seg_writers/cseg_file_writer.c 
#    seg_writers/cseg_ivr_writer.c

libcseg_la_LDFLAGS = -version-info 0:0:0 $(AM_LDFLAGS)   
                    
#include_HEADERS = $(srcdir)/libffmpeg_ivr.h


