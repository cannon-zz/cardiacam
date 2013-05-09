/*
 * GstAudioRationalResample
 *
 * Copyright (C) 2012  Kipp Cannon
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


#include <audiorationalresample.h>


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


GST_BOILERPLATE(GstAudioRationalResample, gst_audio_rationalresample, GstBin, GST_TYPE_BIN);


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
 *                                Exported API
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


static void gst_audio_rationalresample_base_init(gpointer klass)
{
	/* no-op */
}


static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw-float, " \
			"channels = (int) [1, MAX], " \
			"rate = (int) [1, MAX], " \
			"width = (int) {32, 64}; " \
		"audio/x-raw-float, " \
			"channels = (int) [1, MAX], " \
			"rate = (fraction) [0/1, MAX], " \
			"width = (int) {32, 64}"
	)
);


static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw-float, " \
		"channels = (int) [1, MAX], " \
		"rate = (int) [1, MAX], " \
		"width = (int) {32, 64}; " \
		"audio/x-raw-float, " \
		"channels = (int) [1, MAX], " \
		"rate = (fraction) [0/1, MAX], " \
		"width = (int) {32, 64}"
	)
);


static void gst_audio_rationalresample_class_init(GstAudioRationalResampleClass *klass)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	gst_element_class_set_details_simple(element_class,
		"Rational sample rate audio resampler",
		"Filter/Converter/Audio",
		"Resamples audio",
		"Kipp Cannon <kipp.cannon@ligo.org>"
	);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}


static void gst_audio_rationalresample_init(GstAudioRationalResample *resample, GstAudioRationalResampleClass *klass)
{
	GstBin *bin = GST_BIN(resample);
	GstElement *element = GST_ELEMENT(resample);
	GstElement *prefaker, *precaps, *audioresample, *postcaps, *postfaker;
	GstPad *sink, *src;

	sink = gst_ghost_pad_new_no_target_from_template("sink", gst_element_class_get_pad_template(GST_ELEMENT_CLASS(klass), "sink"));
	gst_element_add_pad(element, sink);
	src = gst_ghost_pad_new_no_target_from_template("src", gst_element_class_get_pad_template(GST_ELEMENT_CLASS(klass), "src"));
	gst_element_add_pad(element, src);

	gst_bin_add_many(bin,
		prefaker = gst_element_factory_make("audioratefaker", "prefaker"),
		precaps = gst_element_factory_make("capsfilter", "precaps"),
		audioresample = gst_element_factory_make("audioresample", "audioresample"),
		postcaps = gst_element_factory_make("capsfilter", "postcaps"),
		postfaker = gst_element_factory_make("audioratefaker", "postfaker"),
		NULL
	);

	gst_ghost_pad_set_target(GST_GHOST_PAD(sink), gst_element_get_pad(prefaker, "sink"));
	gst_ghost_pad_set_target(GST_GHOST_PAD(src), gst_element_get_pad(postfaker, "src"));

	gst_element_link_many(prefaker, precaps, audioresample, postcaps, postfaker, NULL);
}
