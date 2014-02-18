SUBDIRS = python

plugin_LTLIBRARIES = libcardiacam.la libwilddevine.la

libcardiacam_la_SOURCES = \
	cardiacam.c \
	audiorationalresample.c audiorationalresample.h \
	audioratefaker.c audioratefaker.h \
	videoratefaker.c videoratefaker.h \
	faceprocessor.c faceprocessor.h \
	face2rgb.c face2rgb.h
libcardiacam_la_CFLAGS = $(AM_CFLAGS) $(gstreamer_CFLAGS) $(gstreamer_audio_CFLAGS) $(gstreamer_video_CFLAGS)
libcardiacam_la_LDFLAGS = $(AM_LDFLAGS) $(gstreamer_LIBS) $(gstreamer_audio_LIBS) $(gstreamer_video_LIBS)  $(CARDIACAM_PLUGIN_LDFLAGS) -lm

libwilddevine_la_SOURCES = \
	wilddevine_plugin.c \
	wilddevine.c wilddevine.h
libwilddevine_la_CFLAGS = $(AM_CFLAGS) $(gstreamer_CFLAGS) $(gstreamer_audio_CFLAGS) $(libusb_CFLAGS)
libwilddevine_la_LDFLAGS = $(AM_LDFLAGS) $(gstreamer_LIBS) $(gstreamer_audio_LIBS) $(libusb_LIBS) $(CARDIACAM_PLUGIN_LDFLAGS) -lm
