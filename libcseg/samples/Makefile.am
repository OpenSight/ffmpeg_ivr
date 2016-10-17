AUTOMAKE_OPTIONS=foreign subdir-objects


AM_CPPFLAGS = -I$(srcdir)/../ -I$(srcdir)/../utils/http_client -I$(srcdir)



noinst_PROGRAMS = HTTPClientSample ffmpeg_test HTTPClientSampleKeepalive

HTTPClientSample_SOURCES = HTTPClientSample.h \
    HTTPClientSample.c
    

HTTPClientSample_LDADD = $(builddir)/../libcseg.la

HTTPClientSampleKeepalive_SOURCES = HTTPClientSampleKeepalive.h \
    HTTPClientSampleKeepalive.c
    

HTTPClientSampleKeepalive_LDADD = $(builddir)/../libcseg.la


ffmpeg_test_SOURCES = ffmpeg_test.c  \
    auxiliary/get_bits.h \
    auxiliary/mpeg4audio.h  \
    auxiliary/mpeg4audio.c
    
ffmpeg_test_LDADD = $(builddir)/../libcseg.la    

ffmpeg_test_CFLAGS = $(libavformat_CFLAGS) $(libavutil_CFLAGS) \
    $(libavcodec_CFLAGS) $(libavfilter_CFLAGS) $(libswscale_CFLAGS) $(libswresample_CFLAGS)  \
    $(libavdevice_CFLAGS)
ffmpeg_test_LDFLAGS = $(libavformat_LIBS) $(libavutil_LIBS) \
    $(libavcodec_LIBS) $(libavfilter_LIBS) $(libswscale_LIBS) $(libswresample_LIBS) \
    $(libavdevice_LIBS)
