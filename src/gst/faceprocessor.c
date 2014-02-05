/*
 * GstFaceProcessor
 *
 * Copyright (C) 2012--2014  Kipp Cannon
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


/*
 * ============================================================================
 *
 *                                  Preamble
 *
 * ============================================================================
 */


#include <math.h>


#include <glib.h>
#include <gst/gst.h>


#include <faceprocessor.h>


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


#define GST_CAT_DEFAULT gst_face_processor_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);


static void additional_initializations(void)
{
	GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "faceprocessor", 0, "faceprocessor element");
}


G_DEFINE_TYPE_WITH_CODE(GstFaceProcessor, gst_face_processor, GST_TYPE_BIN, additional_initializations(););


/*
 * ============================================================================
 *
 *                             Internal Functions
 *
 * ============================================================================
 */


static void caps_notify_handler(GObject *object, GParamSpec *pspec, gpointer user_data)
{
	GstFaceProcessor *element = GST_FACE_PROCESSOR(gst_pad_get_parent(object));
	GstCaps *caps;
	GstStructure *s;
	gint rate_num, rate_den;
	gboolean success = TRUE;

	caps = gst_pad_get_current_caps(GST_PAD(object));
	if(!caps || !gst_caps_is_fixed(caps))
		goto done;

	s = gst_caps_get_structure(caps, 0);
	success &= gst_structure_get_fraction(s, "framerate", &rate_num, &rate_den);

	if(success) {
		gint rate = ceil((double) rate_num / rate_den);

		GST_DEBUG_OBJECT(element, "input caps = %" GST_PTR_FORMAT "; RGB time series will be resampled from %d/%d Hz to %d Hz", caps, rate_num, rate_den, rate);

		/* FIXME:  does this leak memory? */
		g_object_set(G_OBJECT(element->capsfilter), "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, rate, NULL), NULL);
	} else
		GST_ERROR_OBJECT(element, "could not determine framerate from input caps %" GST_PTR_FORMAT, caps);

done:
	if(caps)
		gst_caps_unref(caps);
	gst_object_unref(element);
}


/*
 * ============================================================================
 *
 *                              GObject Methods
 *
 * ============================================================================
 */


static void finalize(GObject *object)
{
	GstFaceProcessor *element = GST_FACE_PROCESSOR(object);

	gst_object_unref(element->face2rgb);
	element->face2rgb = NULL;
	gst_object_unref(element->capsfilter);
	element->capsfilter = NULL;

	G_OBJECT_CLASS(gst_face_processor_parent_class)->finalize(object);
}


static void gst_face_processor_class_init(GstFaceProcessorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	gobject_class->finalize = GST_DEBUG_FUNCPTR(finalize);

	gst_element_class_set_details_simple(element_class,
		"Cardiacam Face Processor",
		"Filter/Converter/Video",
		"Cardiac pulse extractor",
		"Kipp Cannon <kipp.cannon@ligo.org>"
	);
}


static void gst_face_processor_init(GstFaceProcessor *faceprocessor)
{
	GstBin *bin = GST_BIN(faceprocessor);
	GstElement *element = GST_ELEMENT(faceprocessor);
	GstElement *resample, *bandpass, *tsvenc, *sink;
	GstPad *pad;

	/* FIXME:  implement whitening filter.  also use for band pass? */
	faceprocessor->face2rgb = gst_element_factory_make("face2rgb", "face2rgb"),
	gst_object_ref(faceprocessor->face2rgb);	/* now two refs */
	faceprocessor->capsfilter = gst_element_factory_make("capsfilter", "capsfilter"),
	gst_object_ref(faceprocessor->capsfilter);	/* now two refs */
	gst_bin_add_many(bin,
		faceprocessor->face2rgb,	/* consume one ref */
		resample = gst_element_factory_make("audiorationalresample", "audiorationalresample"),
		faceprocessor->capsfilter,
		bandpass = gst_element_factory_make("audiochebband", "audiochebband"),
		tsvenc = gst_element_factory_make("tsvenc", "tsvenc"),
		sink = gst_element_factory_make("fdsink", "sink"),
		NULL
	);

	pad = gst_ghost_pad_new("sink", gst_element_get_static_pad(faceprocessor->face2rgb, "sink"));
	g_signal_connect_after(pad, "notify::caps", (GCallback) caps_notify_handler, NULL);
	gst_element_add_pad(element, pad);

	g_object_set(G_OBJECT(bandpass), "lower-frequency", 0.5, "upper-frequency", 5.0, "poles", 4, NULL);
	g_object_set(G_OBJECT(sink), "fd", 1, "sync", FALSE, "async", FALSE, NULL);

	gst_element_link_many(faceprocessor->face2rgb, resample, faceprocessor->capsfilter, bandpass, tsvenc, sink, NULL);
}
