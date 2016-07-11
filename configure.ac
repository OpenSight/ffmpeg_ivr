#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
version = 0.1.0

AC_PREREQ([2.63])
AC_INIT([ffmpeg_ivr], [0.1.0], [opensight@126.com])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])


AM_INIT_AUTOMAKE([tar-ustar])
AM_MAINTAINER_MODE
AM_SILENT_RULES


LT_PREREQ([2.2])
LT_INIT()
AC_PROG_LIBTOOL

AC_CONFIG_SRCDIR([src/ffmpeg_ivr.c])
AC_CONFIG_HEADERS([config.h])



# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
# AC_PROG_RANLIB


# Set some default features required by ffmpeg_ivr code.
CPPFLAGS="-D_REENTRANT -D_THREAD_SAFE $CPPFLAGS"
CFLAGS="-pthread $CFLAGS"
CXXFLAGS="-pthread $CXXFLAGS"


# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])

export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/lib64/pkgconfig:/lib/pkgconfig:/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig

# PKG checks

PKG_CHECK_MODULES(libcurl, [libcurl >= 7.19.0])
AC_SUBST(libcurl_LIBS)
AC_SUBST(libcurl_CFLAGS)


PKG_CHECK_MODULES(libavformat, [libavformat >= 56.40.100])
PKG_CHECK_MODULES(libavutil, [libavutil >= 54.31.0])  
PKG_CHECK_MODULES(libavcodec, [libavcodec >= 56.60.100])  
PKG_CHECK_MODULES(libavfilter, [libavfilter >= 5.40.100])  
PKG_CHECK_MODULES(libswscale, [libswscale >= 3.1.100])  
PKG_CHECK_MODULES(libswresample, [libswresample >= 1.2.100])  
PKG_CHECK_MODULES(libavdevice, [libavdevice >= 56.4.100])  
AC_SUBST(libavformat_LIBS)
AC_SUBST(libavformat_CFLAGS)
AC_SUBST(libavutil_LIBS)
AC_SUBST(libavutil_CFLAGS)
AC_SUBST(libavcodec_LIBS)
AC_SUBST(libavcodec_CFLAGS)
AC_SUBST(libavfilter_LIBS)
AC_SUBST(libavfilter_CFLAGS)
AC_SUBST(libswscale_LIBS)
AC_SUBST(libswscale_CFLAGS)
AC_SUBST(libswresample_LIBS)
AC_SUBST(libswresample_CFLAGS)
AC_SUBST(libavdevice_LIBS)
AC_SUBST(libavdevice_CFLAGS)


# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h limits.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h sys/select.h sys/resource.h termios.h unistd.h])
AC_CHECK_HEADERS([pthread.h], AC_DEFINE([HAVE_PTHREADS], [1], [make use of pthread in transcoding]))


# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([floor gettimeofday localtime_r memmove memset pow select strchr strcspn strerror strrchr strstr strtol])



# Self define macro
AC_DEFINE([CONFIG_THIS_YEAR], [2016], [PROJECT COMPILE YEAR])
AC_DEFINE([FFMPEG_CONFIGURATION], ["UNKONW"], [ffmpeg configuration])
AC_DEFINE([FFMPEG_DATADIR], ["/usr/local/ffmpeg_data"], [DATA DIR CONFIGURE])
AC_DEFINE([CONFIG_AVUTIL], [1], [support libavutil])
AC_DEFINE([CONFIG_THIS_YEAR], [2016], [PROJECT COMPILE YEAR])
AC_DEFINE([CONFIG_AVUTIL], [1], [support libavutil])
AC_DEFINE([CONFIG_AVCODEC], [1], [support libavcodec])
AC_DEFINE([CONFIG_AVFILTER], [1], [support libavfilter])
AC_DEFINE([CONFIG_AVFORMAT], [1], [support libavformat])
AC_DEFINE([CONFIG_AVDEVICE], [1], [support libavdevice])
AC_DEFINE([CONFIG_AVRESAMPLE], [0], [not support libavresample])
AC_DEFINE([CONFIG_POSTPROC], [0], [not support libpostproc])
AC_DEFINE([CONFIG_SWRESAMPLE], [1], [support libswresample])
AC_DEFINE([CONFIG_SWSCALE], [1], [support libswscale])
AC_DEFINE([CONFIG_NETWORK], [0], [not support ffmpeg network related function])
AC_DEFINE([CC_IDENT], ["gcc"], [only gcc])
AC_DEFINE([AVCONV_DATADIR], ["/usr/local/avconv_data"], [DATA DIR CONFIGURE])
AC_DEFINE([FFMPEG_IVR], [1], [identify in ffmpeg_ivr compilation context])


# generate Makefiles 
AC_CONFIG_FILES([Makefile \
                 libffmpeg_ivr/Makefile \
                 libffmpeg_ivr/libffmpeg_ivr.pc])

# Generate other makefiles


AC_OUTPUT



AC_MSG_NOTICE([Configure successfully!

ffmpeg_ivr version .................... : $PACKAGE_VERSION
ffmpeg_ivr Install prefix ............. : $prefix
ffmpeg_ivr exec prefix ................ : $exec_prefix
ffmpeg_ivr library path ............... : $libdir
ffmpeg_ivr include path ............... : $includedir
ffmpeg_ivr program path ............... : $bindir






 'make' will now compile StreamSwitch and 'su -c make install' will install it.
 Note: On non-Linux systems you might need to use 'gmake' instead of 'make'.
])