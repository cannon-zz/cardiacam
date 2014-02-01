/*
 * GstAudioRationalResample
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
#include <gst/audio/audio.h>


#include <audiorationalresample.h>


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


#define GST_CAT_DEFAULT gst_audiorationalresample_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);


static void additional_initializations(void)
{
	GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "audiorationalresample", 0, "audiorationalresample element");
}


G_DEFINE_TYPE_WITH_CODE(GstAudioRationalResample, gst_audiorationalresample, GST_TYPE_BIN, additional_initializations(););


/*
 * ============================================================================
 *
 *                             Internal Functions
 *
 * ============================================================================
 */


/*
 * caps_notify_handler()
 */


static void caps_notify_handler(GObject *object, GParamSpec *pspec, gpointer user_data)
{
	GstAudioRationalResample *element = GST_AUDIO_RATIONALRESAMPLE(gst_pad_get_parent(object));
	GstCaps *incaps, *outcaps;
	GstStructure *s;
	gint inrate_num, inrate_den = 1;
	gint outrate_num, outrate_den = 1;
	gboolean success = TRUE;

	incaps = gst_pad_get_current_caps(element->sinkpad);
	outcaps = gst_pad_get_current_caps(element->srcpad);
	GST_DEBUG_OBJECT(element, "incaps = %" GST_PTR_FORMAT ", outcaps = %" GST_PTR_FORMAT, incaps, outcaps);

	if(!incaps || !outcaps) {
		GST_DEBUG_OBJECT(element, "incomplete caps, not continuing with configuration");
		goto done;
	}

	s = gst_caps_get_structure(incaps, 0);
	if(!gst_structure_get_int(s, "rate", &inrate_num))
		success &= gst_structure_get_fraction(s, "rate", &inrate_num, &inrate_den);

	s = gst_caps_get_structure(outcaps, 0);
	if(!gst_structure_get_int(s, "rate", &outrate_num))
		success &= gst_structure_get_fraction(s, "rate", &outrate_num, &outrate_den);

	if(success) {
		/* least common multiple of denominators = smallest
		 * multiplicative factor that makes both in and out rates
		 * integers */
		gint den_lcm = inrate_den / gst_util_greatest_common_divisor(inrate_den, outrate_den) * outrate_den;

		GST_DEBUG_OBJECT(element, "input rate %d/%d, output rate %d/%d, denominators' least common multiple = %d", inrate_num, inrate_den, outrate_num, outrate_den, den_lcm);

		/* multiply in and out rates by den_lcm.  makes both integers,
		 * so can evalute ratios */
		inrate_num *= den_lcm / inrate_den;
		outrate_num *= den_lcm / outrate_den;

		GST_DEBUG_OBJECT(element, "audioresample will believe input rate is %d, output rate is %d", inrate_num, outrate_num);

		/* make rate fakers to do their thing */
		g_object_set(element->precaps, "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, inrate_num, NULL), NULL);
		g_object_set(element->postcaps, "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, outrate_num, NULL), NULL);
	} else
		GST_ERROR_OBJECT(element, "could not determine input and/or output sample rates");

done:
	if(incaps)
		gst_caps_unref(incaps);
	if(outcaps)
		gst_caps_unref(outcaps);
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
	GstAudioRationalResample *element = GST_AUDIO_RATIONALRESAMPLE(object);

	gst_object_unref(element->sinkpad);
	element->sinkpad = NULL;
	gst_object_unref(element->srcpad);
	element->srcpad = NULL;
	gst_object_unref(element->precaps);
	element->precaps = NULL;
	gst_object_unref(element->postcaps);
	element->postcaps = NULL;

	G_OBJECT_CLASS(gst_audiorationalresample_parent_class)->finalize(object);
}


static void gst_audiorationalresample_class_init(GstAudioRationalResampleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	gobject_class->finalize = GST_DEBUG_FUNCPTR(finalize);

	gst_element_class_set_details_simple(element_class,
		"Rational sample rate audio resampler",
		"Filter/Converter/Audio",
		"Resamples audio",
		"Kipp Cannon <kipp.cannon@ligo.org>"
	);
}


static void gst_audiorationalresample_init(GstAudioRationalResample *resample)
{
	GstBin *bin = GST_BIN(resample);
	GstElement *element = GST_ELEMENT(resample);
	GstElement *prefaker, *audioresample, *postfaker;

	resample->sinkpad = gst_ghost_pad_new_no_target("sink", GST_PAD_SINK);
	g_signal_connect_after(resample->sinkpad, "notify::caps", (GCallback) caps_notify_handler, NULL);
	gst_object_ref(resample->sinkpad);	/* now two refs */
	gst_element_add_pad(element, resample->sinkpad);	/* consume one ref */

	resample->srcpad = gst_ghost_pad_new_no_target("src", GST_PAD_SRC);
	g_signal_connect_after(resample->srcpad, "notify::caps", (GCallback) caps_notify_handler, NULL);
	gst_object_ref(resample->srcpad);	/* now two refs */
	gst_element_add_pad(element, resample->srcpad);	/* consume one ref */

	resample->precaps = gst_element_factory_make("capsfilter", "precaps"),
	resample->postcaps = gst_element_factory_make("capsfilter", "postcaps"),
	gst_object_ref(resample->precaps);	/* now two refs */
	gst_object_ref(resample->postcaps);	/* now two refs */
	gst_bin_add_many(bin,
		prefaker = gst_element_factory_make("audioratefaker", "prefaker"),
		resample->precaps,	/* consume one ref */
		audioresample = gst_element_factory_make("audioresample", "audioresample"),
		resample->postcaps,	/* consume one ref */
		postfaker = gst_element_factory_make("audioratefaker", "postfaker"),
		NULL
	);

	gst_ghost_pad_set_target(GST_GHOST_PAD(resample->sinkpad), gst_element_get_static_pad(prefaker, "sink"));
	gst_ghost_pad_set_target(GST_GHOST_PAD(resample->srcpad), gst_element_get_static_pad(postfaker, "src"));

	gst_element_link_many(prefaker, resample->precaps, audioresample, resample->postcaps, postfaker, NULL);
}
