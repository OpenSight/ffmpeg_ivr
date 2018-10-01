AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)
AM_CFLAGS = $(libcurl_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS) \
    $(libavcodec_CFLAGS) $(libavfilter_CFLAGS) $(libswscale_CFLAGS) $(libswresample_CFLAGS)  \
    $(libavdevice_CFLAGS)
AM_LDFLAGS = $(libcurl_LIBS) $(libavformat_LIBS) $(libavutil_LIBS) \
    $(libavcodec_LIBS) $(libavfilter_LIBS) $(libswscale_LIBS) $(libswresample_LIBS) \
    $(libavdevice_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(srcdir)/libffmpeg_ivr.pc



lib_LTLIBRARIES=libffmpeg_ivr.la

libffmpeg_ivr_la_SOURCES =  register.c \
    cached_segment.c \
    cached_segment.h \
    cJSON.c \
    cJSON.h \
    seg_writers/cseg_dummy_writer.c \
    seg_writers/cseg_file_writer.c \
    seg_writers/cseg_ivt_writer.c \
    seg_writers/cseg_ivr_writer.c

libffmpeg_ivr_la_LDFLAGS = -version-info 0:0:0 $(AM_LDFLAGS)   
                    
include_HEADERS = $(srcdir)/libffmpeg_ivr.h


