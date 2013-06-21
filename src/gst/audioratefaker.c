/*
 * GstAudioRateFaker
 *
 * Copyright (C) 2013  Kipp Cannon
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
#include <gst/base/gstbasetransform.h>


#include <audioratefaker.h>


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


GST_BOILERPLATE(GstAudioRateFaker, gst_audio_rate_faker, GstBaseTransform, GST_TYPE_BASE_TRANSFORM);


/*
 * ============================================================================
 *
 *                          GstBaseTransform Methods
 *
 * ============================================================================
 */


static GstCaps *transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps)
{
	GstCaps *othercaps = NULL;
	guint i;

	switch(direction) {
	case GST_PAD_SRC:
		/*
		 * sink pad must have same channel count and sample width
		 * as source pad
		 */

		othercaps = gst_caps_copy(gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(trans)));
		for(i = 0; i < gst_caps_get_size(othercaps); i++) {
			gst_structure_set_value(gst_caps_get_structure(othercaps, i), "channels", gst_structure_get_value(gst_caps_get_structure(caps, 0), "channels"));
			gst_structure_set_value(gst_caps_get_structure(othercaps, i), "width", gst_structure_get_value(gst_caps_get_structure(caps, 0), "width"));
		}
		break;

	case GST_PAD_SINK:
		/*
		 * source pad must have same channel count and sample width
		 * as sink pad
		 */

		othercaps = gst_caps_copy(gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(trans)));
		for(i = 0; i < gst_caps_get_size(caps); i++) {
			gst_structure_set_value(gst_caps_get_structure(othercaps, i), "channels", gst_structure_get_value(gst_caps_get_structure(caps, 0), "channels"));
			gst_structure_set_value(gst_caps_get_structure(othercaps, i), "width", gst_structure_get_value(gst_caps_get_structure(caps, 0), "width"));
		}
		break;

	case GST_PAD_UNKNOWN:
		GST_ELEMENT_ERROR(trans, CORE, NEGOTIATION, (NULL), ("invalid direction GST_PAD_UNKNOWN"));
		gst_caps_unref(caps);
		return GST_CAPS_NONE;
	}

	return othercaps;
}


static gboolean set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
	GstAudioRateFaker *element = GST_AUDIO_RATE_FAKER(trans);
	GstStructure *s;
	gint inrate_num, inrate_den = 1;
	gint outrate_num, outrate_den = 1;
	gboolean success = TRUE;

	s = gst_caps_get_structure(incaps, 0);
	if(!gst_structure_get_int(s, "rate", &inrate_num))
		success &= gst_structure_get_fraction(s, "rate", &inrate_num, &inrate_den);

	s = gst_caps_get_structure(outcaps, 0);
	if(!gst_structure_get_int(s, "rate", &outrate_num))
		success &= gst_structure_get_fraction(s, "rate", &outrate_num, &outrate_den);

	if(success) {
		gint gcd;

		element->inrate_over_outrate_num = inrate_num * outrate_den;
		element->inrate_over_outrate_den = inrate_den * outrate_num;

		gcd = gst_util_greatest_common_divisor(element->inrate_over_outrate_num, element->inrate_over_outrate_den);

		element->inrate_over_outrate_num /= gcd;
		element->inrate_over_outrate_den /= gcd;
	}

	return success;
}


static gboolean event(GstBaseTransform *trans, GstEvent *event)
{
	GstAudioRateFaker *element = GST_AUDIO_RATE_FAKER(trans);

	switch(GST_EVENT_TYPE(event)) {
	case GST_EVENT_NEWSEGMENT:
		if(element->last_segment)
			gst_event_unref(element->last_segment);
		element->last_segment = event;
		return FALSE;

	default:
		return TRUE;
	}
}


static GstFlowReturn transform_ip(GstBaseTransform *trans, GstBuffer *buf)
{
	GstAudioRateFaker *element = GST_AUDIO_RATE_FAKER(trans);
	GstFlowReturn result = GST_FLOW_OK;

	if(element->last_segment) {
		gboolean update;
		gdouble rate;
		GstFormat format;
		gint64 start, stop, position;
		gst_event_parse_new_segment(element->last_segment, &update, &rate, &format, &start, &stop, &position);
		if(format == GST_FORMAT_TIME) {
			if(GST_CLOCK_TIME_IS_VALID(start))
				start = gst_util_uint64_scale_int_round(start, element->inrate_over_outrate_num, element->inrate_over_outrate_den);
			if(GST_CLOCK_TIME_IS_VALID(stop))
				stop = gst_util_uint64_scale_int_round(stop, element->inrate_over_outrate_num, element->inrate_over_outrate_den);
			if(GST_CLOCK_TIME_IS_VALID(position))
				position = gst_util_uint64_scale_int_round(position, element->inrate_over_outrate_num, element->inrate_over_outrate_den);
			gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(trans), gst_event_new_new_segment(update, rate, format, start, stop, position));
			gst_event_unref(element->last_segment);
		} else
			gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(trans), element->last_segment);
		element->last_segment = NULL;
	}

	if(GST_BUFFER_TIMESTAMP_IS_VALID(buf) && GST_BUFFER_DURATION_IS_VALID(buf)) {
		GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buf);
		GstClockTime duration = GST_BUFFER_DURATION(buf);

		GST_BUFFER_TIMESTAMP(buf) = gst_util_uint64_scale_int_round(timestamp, element->inrate_over_outrate_num, element->inrate_over_outrate_den);
		GST_BUFFER_DURATION(buf) = gst_util_uint64_scale_int_round(timestamp + duration, element->inrate_over_outrate_num, element->inrate_over_outrate_den) - GST_BUFFER_TIMESTAMP(buf);
	} else if(GST_BUFFER_TIMESTAMP_IS_VALID(buf))
		GST_BUFFER_TIMESTAMP(buf) = gst_util_uint64_scale_int_round(GST_BUFFER_TIMESTAMP(buf), element->inrate_over_outrate_num, element->inrate_over_outrate_den);
	else if(GST_BUFFER_DURATION_IS_VALID(buf))
		GST_BUFFER_DURATION(buf) = gst_util_uint64_scale_int_round(GST_BUFFER_DURATION(buf), element->inrate_over_outrate_num, element->inrate_over_outrate_den);

	gst_buffer_set_caps(buf, GST_PAD_CAPS(GST_BASE_TRANSFORM_SRC_PAD(trans)));

	return result;
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
	GstAudioRateFaker *element = GST_AUDIO_RATE_FAKER(object);

	if(element->last_segment)
		gst_event_unref(element->last_segment);
	element->last_segment = NULL;
}


static void gst_audio_rate_faker_base_init(gpointer klass)
{
	/* no-op */
}


static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
	GST_BASE_TRANSFORM_SINK_NAME,
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
	GST_BASE_TRANSFORM_SRC_NAME,
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


static void gst_audio_rate_faker_class_init(GstAudioRateFakerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS(klass);

	object_class->finalize = GST_DEBUG_FUNCPTR(finalize);

	transform_class->set_caps = GST_DEBUG_FUNCPTR(set_caps);
	transform_class->transform_caps = GST_DEBUG_FUNCPTR(transform_caps);
	transform_class->transform_ip = GST_DEBUG_FUNCPTR(transform_ip);
	transform_class->event = GST_DEBUG_FUNCPTR(event);
	transform_class->passthrough_on_same_caps = TRUE;

	gst_element_class_set_details_simple(element_class, 
		"Audio rate faker",
		"Filter/Audio",
		"Adjusts segments and audio buffer metadata to assign a new sample rate.  Allows input and/or output streams to have rational sample rates.",
		"Kipp Cannon <kipp.cannon@ligo.org>"
	);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}


static void gst_audio_rate_faker_init(GstAudioRateFaker *element, GstAudioRateFakerClass *klass)
{
	gst_base_transform_set_gap_aware(GST_BASE_TRANSFORM(element), TRUE);
}
