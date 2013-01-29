plugin_LTLIBRARIES = libcardiacam.la

libcardiacam_la_SOURCES = \
	cardiacam.c \
	audiorationalresample.h audiorationalresample.c
libcardiacam_la_CFLAGS = $(AM_CFLAGS) $(gstreamer_CFLAGS)
libcardiacam_la_LDFLAGS = $(AM_LDFLAGS) $(gstreamer_LIBS) -L/usr/lib/x86_64-linux-gnu/gstreamer-0.10 -lgstaudioresample $(CARDIACAM_PLUGIN_LDFLAGS)

SUBDIRS = python
