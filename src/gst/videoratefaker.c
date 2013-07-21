/*
 * GstVideoRateFaker
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


#include <videoratefaker.h>


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


#define GST_CAT_DEFAULT gst_video_rate_faker_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);


static void additional_initializations(GType type)
{
	GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "videoratefaker", 0, "videoratefaker element");
}


GST_BOILERPLATE_FULL(GstVideoRateFaker, gst_video_rate_faker, GstBaseTransform, GST_TYPE_BASE_TRANSFORM, additional_initializations);


/*
 * ============================================================================
 *
 *                          GstBaseTransform Methods
 *
 * ============================================================================
 */


static GstCaps *transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps)
{
	GstCaps *othercaps = gst_caps_copy(caps);
	guint i;

	for(i = 0; i < gst_caps_get_size(othercaps); i++)
		gst_structure_set(gst_caps_get_structure(othercaps, i), "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

	return othercaps;
}


static gboolean set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
	GstVideoRateFaker *element = GST_VIDEO_RATE_FAKER(trans);
	gint inrate_num, inrate_den;
	gint outrate_num, outrate_den;
	gboolean success = TRUE;

	success &= gst_structure_get_fraction(gst_caps_get_structure(incaps, 0), "framerate", &inrate_num, &inrate_den);
	success &= gst_structure_get_fraction(gst_caps_get_structure(outcaps, 0), "framerate", &outrate_num, &outrate_den);

	if(success) {
		gst_util_fraction_multiply(inrate_num, inrate_den, outrate_den, outrate_num, &element->inrate_over_outrate_num, &element->inrate_over_outrate_den);
		GST_DEBUG_OBJECT(element, "in rate / out rate = %d/%d", element->inrate_over_outrate_num, element->inrate_over_outrate_den);
	}

	return success;
}


static gboolean event(GstBaseTransform *trans, GstEvent *event)
{
	GstVideoRateFaker *element = GST_VIDEO_RATE_FAKER(trans);

	switch(GST_EVENT_TYPE(event)) {
	case GST_EVENT_NEWSEGMENT:
		if(element->last_segment)
			gst_event_unref(element->last_segment);
		element->last_segment = event;
		gst_event_ref(event);	/* FIXME:  is this needed? */
		return FALSE;

	default:
		return TRUE;
	}
}


static GstFlowReturn transform_ip(GstBaseTransform *trans, GstBuffer *buf)
{
	GstVideoRateFaker *element = GST_VIDEO_RATE_FAKER(trans);
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
	GstVideoRateFaker *element = GST_VIDEO_RATE_FAKER(object);

	if(element->last_segment)
		gst_event_unref(element->last_segment);
	element->last_segment = NULL;

	/*
	 * chain to parent class' finalize() method
	 */

	G_OBJECT_CLASS(parent_class)->finalize(object);
}


static void gst_video_rate_faker_base_init(gpointer klass)
{
	/* no-op */
}


static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
	GST_BASE_TRANSFORM_SINK_NAME,
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"video/x-raw-yuv; " \
		"video/x-raw-rgb; " \
		"video/x-raw-gray; " \
		"image/jpeg; " \
		"image/png"
	)
);


static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
	GST_BASE_TRANSFORM_SRC_NAME,
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"video/x-raw-yuv; " \
		"video/x-raw-rgb; " \
		"video/x-raw-gray; " \
		"image/jpeg; " \
		"image/png"
	)
);


static void gst_video_rate_faker_class_init(GstVideoRateFakerClass *klass)
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
		"Video rate faker",
		"Filter/Video",
		"Adjusts segments and video buffer metadata to assign a new frame rate.",
		"Kipp Cannon <kipp.cannon@ligo.org>"
	);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}


static void gst_video_rate_faker_init(GstVideoRateFaker *element, GstVideoRateFakerClass *klass)
{
	gst_base_transform_set_gap_aware(GST_BASE_TRANSFORM(element), TRUE);
}
