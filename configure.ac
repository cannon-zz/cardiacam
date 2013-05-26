AC_INIT([cardiacam],[0.1.0],[kcannon@cita.utoronto.ca],[cardiacam])
AC_COPYRIGHT([Copyright (C) 2012 Kipp Cannon])
# a file whose existance can be used to use to check that we are in the
# top-level directory of the source tree
AC_CONFIG_SRCDIR([gnuscripts/ltmain.sh])
AC_CONFIG_AUX_DIR([gnuscripts])
AC_CONFIG_MACRO_DIR([gnuscripts])
AM_INIT_AUTOMAKE([1.9 tar-ustar foreign])
AC_CONFIG_FILES([ \
	Makefile \
	gnuscripts/Makefile \
	src/Makefile \
	src/bin/Makefile \
	src/gst/Makefile \
	src/gst/python/Makefile \
	src/python/Makefile
])

#
# Check for programs
#


AC_PROG_CC_C99
AC_PROG_INSTALL
# libtool incorrectly determines library path on SL6.  FIXME:  remove when
# no longer needed
case `cat /etc/redhat-release 2> /dev/null` in
"Scientific Linux"*|"CentOS"*)
	AC_MSG_NOTICE([hacking round broken libtool multilib support on RedHat systems])
	lt_cv_sys_lib_dlsearch_path_spec="/lib64 /usr/lib64"
	;;
esac
LT_INIT
PKG_PROG_PKG_CONFIG()


#
# Check for Python
#


AM_PATH_PYTHON([2.6],,)
AX_PYTHON_DEVEL()
# hack to remove default lib dirs from PYTHON_LDFLAGS.  only tested on
# Debian and SL6.  FIXME:  find a way to do this properly
PYTHON_LDFLAGS="`echo $PYTHON_LDFLAGS | sed -e 'sX-L/usr/lib\(64\)\?\(/\)\?[[:space:]]*XXg'`"


#
# CFLAGS
#


AX_CFLAGS_WARN_ALL([AM_CFLAGS])
AM_CFLAGS="$AM_CFLAGS -Wextra -Wno-missing-field-initializers -Wno-unused-parameter"    # extra gcc-specific stuff
AC_SUBST([AM_CFLAGS])


#
# LDFLAGS
#


# turn on error messages for undefined symbols
AM_LDFLAGS="$AM_LDFLAGS -no-undefined"
AC_SUBST([AM_LDFLAGS])


#
# Check for GStreamer
#


AC_SUBST([GSTREAMER_RELEASE], [0.10])
AC_SUBST([MIN_GSTREAMER_VERSION], [0.10.32])
PKG_CHECK_MODULES([gstreamer], [gstreamer-${GSTREAMER_RELEASE} >= ${MIN_GSTREAMER_VERSION} gstreamer-base-${GSTREAMER_RELEASE} >= ${MIN_GSTREAMER_VERSION} gstreamer-controller-${GSTREAMER_RELEASE} >= ${MIN_GSTREAMER_VERSION}])
AC_SUBST([gstreamer_CFLAGS])
AC_SUBST([gstreamer_LIBS])
AC_SUBST([plugindir], [${libdir}/gstreamer-${GSTREAMER_RELEASE}])
AC_MSG_NOTICE([If you have chosen to install the software in a location not included in your gstreamer default search path, you might need to add the directory

        $plugindir

to your GST_PLUGIN_PATH environment variable.])
AC_SUBST([CARDIACAM_PLUGIN_LDFLAGS], ["-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*"])


#
# Check for GStreamer Video Library
#


AC_SUBST([GSTREAMER_VIDEO_RELEASE], [0.10])
AC_SUBST([MIN_GSTREAMER_VIDEO_VERSION], [0.10.32])
PKG_CHECK_MODULES([gstreamer_video], [gstreamer-video-${GSTREAMER_VIDEO_RELEASE} >= ${MIN_GSTREAMER_VIDEO_VERSION}])
AC_SUBST([gstreamer_video_CFLAGS])
AC_SUBST([gstreamer_video_LIBS])


#
# Check for GStreamer Audio Library
#


AC_SUBST([GSTREAMER_AUDIO_RELEASE], [0.10])
AC_SUBST([MIN_GSTREAMER_VIDEO_VERSION], [0.10.32])
PKG_CHECK_MODULES([gstreamer_audio], [gstreamer-audio-${GSTREAMER_VIDEO_RELEASE} >= ${MIN_GSTREAMER_VIDEO_VERSION}])
AC_SUBST([gstreamer_audio_CFLAGS])
AC_SUBST([gstreamer_audio_LIBS])


#
# Output configure information
#


AC_PROG_MAKE_SET
AC_OUTPUT
