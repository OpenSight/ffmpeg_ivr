AUTOMAKE_OPTIONS=foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir)/libffmpeg_ivr -I$(srcdir)/src
AM_CFLAGS = $(libcurl_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS) \
    $(libavcodec_CFLAGS) $(libavfilter_CFLAGS) $(libswscale_CFLAGS) $(libswresample_CFLAGS)  \
    $(libavdevice_CFLAGS)
AM_LDFLAGS = $(libcurl_LIBS) $(libavformat_LIBS) $(libavutil_LIBS) \
    $(libavcodec_LIBS) $(libavfilter_LIBS) $(libswscale_LIBS) $(libswresample_LIBS) \
    $(libavdevice_LIBS)


SUBDIRS = libffmpeg_ivr . 

if INCLUDE_LIBCSEG
SUBDIRS += libcseg
endif


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
    

ffmpeg_ivr_LDADD = $(builddir)/libffmpeg_ivr/libffmpeg_ivr.la

