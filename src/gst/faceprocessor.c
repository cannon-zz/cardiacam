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
	GstElement *resample, *capsfilter, *bandpass, *tsvenc, *sink;

	/* FIXME:  implement whitening filter.  also use for band pass? */
	faceprocessor->face2rgb = gst_element_factory_make("face2rgb", "face2rgb"),
	gst_object_ref(faceprocessor->face2rgb);	/* now two refs */
	gst_bin_add_many(bin,
		faceprocessor->face2rgb,	/* consume one ref */
		resample = gst_element_factory_make("audiorationalresample", "audiorationalresample"),
		capsfilter = gst_element_factory_make("capsfilter", "capsfilter"),
		bandpass = gst_element_factory_make("audiochebband", "audiochebband"),
		tsvenc = gst_element_factory_make("tsvenc", "tsvenc"),
		sink = gst_element_factory_make("fdsink", "sink"),
		NULL
	);

	gst_element_add_pad(element, gst_ghost_pad_new("sink", gst_element_get_static_pad(faceprocessor->face2rgb, "sink")));

	/* FIXME;  auto-adjust output rate to something suitable for video
	 * frame rate */
	g_object_set(G_OBJECT(capsfilter), "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, 500, NULL), NULL);
	g_object_set(G_OBJECT(bandpass), "lower-frequency", 0.5, "upper-frequency", 5.0, "poles", 4, NULL);
	g_object_set(G_OBJECT(sink), "fd", 1, "sync", FALSE, "async", FALSE, NULL);

	gst_element_link_many(faceprocessor->face2rgb, resample, capsfilter, bandpass, tsvenc, sink, NULL);
}
