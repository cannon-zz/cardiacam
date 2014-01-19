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


#define GST_CAT_DEFAULT gst_audio_rationalresample_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);


static void additional_initializations(void)
{
	GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "audiorationalresample", 0, "audiorationalresample element");
}


G_DEFINE_TYPE_WITH_CODE(GstAudioRationalResample, gst_audio_rationalresample, GST_TYPE_BIN, additional_initializations(););


/*
 * ============================================================================
 *
 *                             Internal Functions
 *
 * ============================================================================
 */


/*
 * setcaps()
 */


static gboolean setcaps(GstPad *pad, GstCaps *caps)
{
	GstAudioRationalResample *element = GST_AUDIO_RATIONALRESAMPLE(gst_pad_get_parent(pad));
	GstCaps *incaps, *outcaps;
	GstStructure *s;
	gint inrate_num, inrate_den = 1;
	gint outrate_num, outrate_den = 1;
	gboolean success = TRUE;

	if(pad == element->sinkpad) {
		incaps = caps;
		outcaps = gst_pad_get_current_caps(element->srcpad);
	} else {
		incaps = gst_pad_get_current_caps(element->sinkpad);
		outcaps = caps;
	}
	GST_DEBUG_OBJECT(element, "incaps = %" GST_PTR_FORMAT ", outcaps = %" GST_PTR_FORMAT, incaps, outcaps);

	if(!incaps || !outcaps)
		/* don't have complete caps yet, say they're OK */
		goto done;

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

		GST_DEBUG_OBJECT(element, "input rate %d/%d, output rate %d/%d, denominator least common multiple = %d", inrate_num, inrate_den, outrate_num, outrate_den, den_lcm);

		/* multiply in and out rates by den_lcm.  makes both integers,
		 * so can evalute ratios */
		inrate_num *= den_lcm / inrate_den;
		outrate_num *= den_lcm / outrate_den;

		GST_DEBUG_OBJECT(element, "input rate rescaled to %d, output rate rescaled to %d", inrate_num, outrate_num);

		/* make rate fakers to do their thing */
		g_object_set(element->precaps, "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, inrate_num, NULL), NULL);
		g_object_set(element->postcaps, "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, outrate_num, NULL), NULL);
	} else
		GST_ERROR_OBJECT(element, "could not determine input and output sample rates");

done:
	return success;
}


/*
 * event()
 */


static gboolean event(GstPad *pad, GstObject *parent, GstEvent *event)
{
	GstCaps *caps;

	switch(GST_EVENT_TYPE(event)) {
	case GST_EVENT_CAPS:
		gst_event_parse_caps(event, &caps);
		if(!setcaps(pad, caps))
			return FALSE;

	default:
		return gst_pad_event_default(pad, parent, event);
	}
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

	G_OBJECT_CLASS(gst_audio_rationalresample_parent_class)->finalize(object);
}


#define AUDIO_FORMATS "(string) {" GST_AUDIO_NE(F32) ", " GST_AUDIO_NE(F64) "}"

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw, " \
			"format = " AUDIO_FORMATS ", " \
			"channels = (int) [1, MAX], " \
			"rate = (int) [1, MAX], " \
			"layout = (string) interleaved; " \
		"audio/x-raw, " \
			"format = " AUDIO_FORMATS ", " \
			"channels = (int) [1, MAX], " \
			"rate = (fraction) [0/1, MAX], " \
			"layout = (string) interleaved"
	)
);


static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS( ""
		"audio/x-raw, " \
			"format = " AUDIO_FORMATS ", " \
			"channels = (int) [1, MAX], " \
			"rate = (int) [1, MAX], " \
			"layout = (string) interleaved; " \
		"audio/x-raw, " \
			"format = " AUDIO_FORMATS ", " \
			"channels = (int) [1, MAX], " \
			"rate = (fraction) [0/1, MAX], " \
			"layout = (string) interleaved"
	)
);


static void gst_audio_rationalresample_class_init(GstAudioRationalResampleClass *klass)
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

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}


static void gst_audio_rationalresample_init(GstAudioRationalResample *resample)
{
	GstBin *bin = GST_BIN(resample);
	GstElement *element = GST_ELEMENT(resample);
	GstElement *prefaker, *audioresample, *postfaker;

	resample->sinkpad = gst_ghost_pad_new_no_target_from_template("sink", gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(resample), "sink"));
	gst_pad_set_event_function(resample->sinkpad, event);
	gst_object_ref(resample->sinkpad);	/* now two refs */
	gst_element_add_pad(element, resample->sinkpad);	/* consume one ref */

	resample->srcpad = gst_ghost_pad_new_no_target_from_template("src", gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(resample), "src"));
	gst_pad_set_event_function(resample->srcpad, event);
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

	/* FIXME:  remove when I can get caps negotiation working */
	g_object_set(resample->precaps, "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, 1000000, NULL), NULL);
	g_object_set(resample->postcaps, "caps", gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, 48 * 33333, NULL), NULL);
}
