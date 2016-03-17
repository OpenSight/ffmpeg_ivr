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
AC_PROG_LIBTOOL

AC_CONFIG_SRCDIR([include/ffmpeg_ivr.h])
# AC_CONFIG_HEADERS([config.h])


# With feature list
AC_ARG_WITH([cseg-recorder],
[AS_HELP_STRING([--with-cseg-recorder],
[would contains the cseg_recorder program @<:@default=yes@:>@])],
[cseg_recorder=${withval}], [cseg_recorder=yes])

AM_CONDITIONAL([INCLUDE_CSEG_RECORDER], [test "x$cseg_recorder" = xyes])



# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_MAKE_SET
# AC_PROG_RANLIB
AM_PROG_CC_C_O

# Set some default features required by ffmpeg_ivr code.
CPPFLAGS="-D_REENTRANT -D_THREAD_SAFE $CPPFLAGS"
CFLAGS="-pthread $CFLAGS"
CXXFLAGS="-pthread $CXXFLAGS"


# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])

export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/lib64/pkgconfig:/lib/pkgconfig:/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig


PKG_CHECK_MODULES(libcurl, [libcurl >= 7.19.0])
AC_SUBST(libcurl_LIBS)
AC_SUBST(libcurl_CFLAGS)


PKG_CHECK_MODULES(libavformat, [libavformat >= 56.40.100])
PKG_CHECK_MODULES(libavutil, [libavutil >= 54.31.0])  
PKG_CHECK_MODULES(libavcodec, [libavcodec >= 56.60.100])  
AC_SUBST(libavformat_LIBS)
AC_SUBST(libavformat_CFLAGS)
AC_SUBST(libavutil_LIBS)
AC_SUBST(libavutil_CFLAGS)





# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/time.h unistd.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([dup2 gettimeofday memset strtol])

# generate Makefiles 
AC_CONFIG_FILES([Makefile \
                 libffmpeg_ivr.pc])

# Generate other makefiles


AC_OUTPUT



AC_MSG_NOTICE([Configure successfully!

ffmpeg_ivr version .................... : $PACKAGE_VERSION
ffmpeg_ivr Install prefix ............. : $prefix
ffmpeg_ivr exec prefix ................ : $exec_prefix
ffmpeg_ivr library path ............... : $libdir
ffmpeg_ivr include path ............... : $includedir
ffmpeg_ivr program path ............... : $bindir

cseg_recorder included................. : $cseg_recorder





 'make' will now compile StreamSwitch and 'su -c make install' will install it.
 Note: On non-Linux systems you might need to use 'gmake' instead of 'make'.
])