AUTOMAKE_OPTIONS=foreign subdir-objects


AM_CPPFLAGS = -I$(srcdir)/../ -I$(srcdir)/../utils/http_client



noinst_PROGRAMS = HTTPClientSample

HTTPClientSample_SOURCES = HTTPClientSample.h \
    HTTPClientSample.c
    

HTTPClientSample_LDADD = $(builddir)/../libcseg.la

