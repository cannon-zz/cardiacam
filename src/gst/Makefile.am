plugin_LTLIBRARIES = libcardiacam.la

libcardiacam_la_SOURCES = \
	cardiacam.c \
	audiorationalresample.c audiorationalresample.h \
	audioratefaker.c audioratefaker.h \
	videoratefaker.c videoratefaker.h \
	video2rgb.c video2rgb.h
libcardiacam_la_CFLAGS = $(AM_CFLAGS) $(gstreamer_CFLAGS)
libcardiacam_la_LDFLAGS = $(AM_LDFLAGS) $(gstreamer_LIBS) $(CARDIACAM_PLUGIN_LDFLAGS) -lm

SUBDIRS = python
