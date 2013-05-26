/*
 * GstAudioRationalResample
 *
 * Copyright (C) 2012,2013  Kipp Cannon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef __AUDIO_RATIONALRESAMPLE_H__
#define __AUDIO_RATIONALRESAMPLE_H__


/*
 * ============================================================================
 *
 *                                  Preamble
 *
 * ============================================================================
 */


#include <glib.h>
#include <gst/gst.h>


G_BEGIN_DECLS


/*
 * ============================================================================
 *
 *                                    Type
 *
 * ============================================================================
 */


#define GST_TYPE_AUDIO_RATIONALRESAMPLE \
	(gst_audio_rationalresample_get_type())
#define GST_AUDIO_RATIONALRESAMPLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AUDIO_RATIONALRESAMPLE, GstAudioRationalResample))
#define GST_AUDIO_RATIONALRESAMPLE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AUDIO_RATIONALRESAMPLE, GstAudioRationalResampleClass))
#define GST_AUDIO_RATIONALRESAMPLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_AUDIO_RATIONALRESAMPLE, GstAudioRationalResampleClass))
#define GST_IS_AUDIO_RATIONALRESAMPLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AUDIO_RATIONALRESAMPLE))
#define GST_IS_AUDIO_RATIONALRESAMPLE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AUDIO_RATIONALRESAMPLE))


typedef struct _GstAudioRationalResampleClass GstAudioRationalResampleClass;
typedef struct _GstAudioRationalResample GstAudioRationalResample;


struct _GstAudioRationalResampleClass {
	GstBinClass parent_class;
};


/**
 * GstAudioRationalResample
 */


struct _GstAudioRationalResample {
	GstBin bin;

	GstPad *sinkpad;
	GstPad *srcpad;
	GstElement *precaps;
	GstElement *postcaps;
};


/*
 * ============================================================================
 *
 *                                Exported API
 *
 * ============================================================================
 */


GType gst_audio_rationalresample_get_type(void);


G_END_DECLS


#endif	/* __AUDIO_RATIONALRESAMPLE_H__ */
