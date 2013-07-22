/*
 * GstFace2RGB
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


#include <math.h>


#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>


#include <face2rgb.h>


#define DEFAULT_GAMMA 1.0
#define DEFAULT_FACE_X 0
#define DEFAULT_FACE_Y 0
#define DEFAULT_FACE_WIDTH 0
#define DEFAULT_FACE_HEIGHT 0


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


#define GST_CAT_DEFAULT gst_face_2_rgb_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);


static void additional_initializations(GType type)
{
	GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "face2rgb", 0, "face2rgb element");
}


GST_BOILERPLATE_FULL(GstFace2RGB, gst_face_2_rgb, GstBaseTransform, GST_TYPE_BASE_TRANSFORM, additional_initializations);


/*
 * ============================================================================
 *
 *                             Internal Functions
 *
 * ============================================================================
 */


static void make_mask(GstFace2RGB *element)
{
	gint *mask = element->mask = g_realloc_n(element->mask, element->height * element->width, sizeof(*element->mask));
	gint x, y;
	gint count_in = 0, count_out = 0;
	gint face_width = element->face_width > 0 ? element->face_width : element->width;
	gint face_height = element->face_height > 0 ? element->face_height : element->height;

	/*
	 * set mask to 1 outside of face ellipse
	 */

	for(y = 0; y < element->height; y++) {
		gdouble y_squared = pow(2.0 * (y - element->face_y) / (face_height - 1) - 1, 2);
		for(x = 0; x < element->width; x++, mask++) {
			if(pow(2.0 * (x - element->face_x) / (face_width - 1) - 1, 2) + y_squared > 1) {
				*mask = 1;
				count_out++;
			} else {
				*mask = 0;
				count_in++;
			}
		}
	}

	/*
	 * non-face pixel count to face pixel count ratio
	 */

	element->out_over_in = (double) count_out / count_in;
}


/*
 * ============================================================================
 *
 *                          GstBaseTransform Methods
 *
 * ============================================================================
 */


static gboolean get_unit_size(GstBaseTransform *trans, GstCaps *caps, guint *size)
{
	GstStructure *str;
	gboolean success = TRUE;

	str = gst_caps_get_structure(caps, 0);
	if(!g_strcmp0(gst_structure_get_name(str), "audio/x-raw-float")) {
		gint width, channels;
		success &= gst_structure_get_int(str, "channels", &channels);
		success &= gst_structure_get_int(str, "width", &width);
		if(success)
			*size = width / 8 * channels;
	} else if(!g_strcmp0(gst_structure_get_name(str), "video/x-raw-rgb")) {
		gint width, height, bpp;
		success &= gst_structure_get_int(str, "width", &width);
		success &= gst_structure_get_int(str, "height", &height);
		success &= gst_structure_get_int(str, "bpp", &bpp);
		if(success) {
			gint stride = bpp / 8 * width;
			/* round up to multiple of 4 */
			if(stride & 0x3)
				stride += 4 - (stride & 0x3);
			*size = height * stride;
		}
	} else
		success = FALSE;

	if(!success)
		GST_ERROR_OBJECT(trans, "could not parse caps");

	return success;
}


static GstCaps *transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps)
{
	const GValue *rate;
	guint n;
	GstCaps *result;

	/*
	 * input framerate must be same as output rate
	 */

	switch(direction) {
	case GST_PAD_SRC:
		result = gst_caps_copy(gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(trans)));
		rate = gst_structure_get_value(gst_caps_get_structure(caps, 0), "rate");
		for(n = 0; n < gst_caps_get_size(result); n++)
			gst_structure_set_value(gst_caps_get_structure(result, n), "framerate", rate);
		break;

	case GST_PAD_SINK:
		result = gst_caps_copy(gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(trans)));
		rate = gst_structure_get_value(gst_caps_get_structure(caps, 0), "framerate");
		for(n = 0; n < gst_caps_get_size(result); n++)
			gst_structure_set_value(gst_caps_get_structure(result, n), "rate", rate);
		break;

	default:
		g_assert_not_reached();
		GST_ELEMENT_ERROR(trans, CORE, NEGOTIATION, (NULL), ("invalid direction"));
		gst_caps_ref(GST_CAPS_NONE);
		result = GST_CAPS_NONE;
		break;
	}

	return result;
}


static gboolean set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps)
{
	GstFace2RGB *element = GST_FACE_2_RGB(trans);
	GstStructure *str = gst_caps_get_structure(incaps, 0);
	gint width, height, bpp;
	gboolean success = TRUE;

	success &= gst_structure_get_int(str, "width", &width);
	success &= gst_structure_get_int(str, "height", &height);
	success &= gst_structure_get_int(str, "bpp", &bpp);
	if(success) {
		gint stride = bpp / 8 * width;
		/* round up to multiple of 4 */
		if(stride & 0x3)
			stride += 4 - (stride & 0x3);

		element->width = width;
		element->height = height;
		element->stride = stride;

		element->need_new_mask = TRUE;
	} else
		GST_ERROR_OBJECT(element, "could not parse caps");

	return success;
}


static gboolean start(GstBaseTransform *trans)
{
	GstFace2RGB *element = GST_FACE_2_RGB(trans);

	element->offset = 0;

	return TRUE;
}


static GstFlowReturn transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
	GstFace2RGB *element = GST_FACE_2_RGB(trans);
	gdouble *out = (gdouble *) GST_BUFFER_DATA(outbuf);
	guchar *row = GST_BUFFER_DATA(inbuf);
	guchar *last_row = row + element->height * element->stride;
	gint *mask;
	gdouble face_r = 0.0, bg_r = 0.0;
	gdouble face_g = 0.0, bg_g = 0.0;
	gdouble face_b = 0.0, bg_b = 0.0;
	gdouble face_y, bg_y;

	if(element->need_new_mask) {
		make_mask(element);
		element->need_new_mask = FALSE;
	}
	mask = element->mask;
	g_return_val_if_fail(mask != NULL, GST_FLOW_ERROR);

	/*
	 * apply gamma correction, and sum face and background RGB
	 * components
	 */

	for(; row < last_row; row += element->stride) {
		guchar *in = row;
		gint col;
		for(col = 0; col < element->width; col++) {
			gdouble r = pow(*in++ / 255.0, element->gamma);
			gdouble g = pow(*in++ / 255.0, element->gamma);
			gdouble b = pow(*in++ / 255.0, element->gamma);
			if(!*mask++) {
				face_r += r;
				face_g += g;
				face_b += b;
			} else {
#if 1
				bg_r += r;
				bg_g += g;
				bg_b += b;
#else
				bg_r++;
				bg_g++;
				bg_b++;
#endif
			}
		}
	}

	/*
	 * compute face and background brightness
	 */

	face_y = (0.2126 * face_r + 0.7152 * face_g + 0.0722 * face_b);
	bg_y = (0.2126 * bg_r + 0.7152 * bg_g + 0.0722 * bg_b);

	/*
	 * set output sample values
	 */

	out[0] = face_r * element->out_over_in / bg_y;
	out[1] = face_g * element->out_over_in / bg_y;
	out[2] = face_b * element->out_over_in / bg_y;

	/*
	 * fix offset on output buffers
	 */

	GST_BUFFER_OFFSET(outbuf) = element->offset;
	element->offset++;
	GST_BUFFER_OFFSET_END(outbuf) = element->offset;

	/*
	 * done
	 */

	return GST_FLOW_OK;
}


/*
 * ============================================================================
 *
 *                              GObject Methods
 *
 * ============================================================================
 */


enum property {
	ARG_GAMMA = 1,
	ARG_FACE_X,
	ARG_FACE_Y,
	ARG_FACE_WIDTH,
	ARG_FACE_HEIGHT
};


static void set_property(GObject *object, enum property prop_id, const GValue *value, GParamSpec *pspec)
{
	GstFace2RGB *element = GST_FACE_2_RGB(object);

	GST_OBJECT_LOCK(element);

	switch(prop_id) {
	case ARG_GAMMA:
		element->gamma = g_value_get_double(value);
		break;

	case ARG_FACE_X:
		element->face_x = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_FACE_Y:
		element->face_y = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_FACE_WIDTH:
		element->face_width = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_FACE_HEIGHT:
		element->face_height = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

	GST_OBJECT_UNLOCK(element);
}


static void get_property(GObject *object, enum property prop_id, GValue *value, GParamSpec *pspec)
{
	GstFace2RGB *element = GST_FACE_2_RGB(object);

	GST_OBJECT_LOCK(element);

	switch(prop_id) {
	case ARG_GAMMA:
		g_value_set_double(value, element->gamma);
		break;

	case ARG_FACE_X:
		g_value_set_int(value, element->face_x);
		break;

	case ARG_FACE_Y:
		g_value_set_int(value, element->face_y);
		break;

	case ARG_FACE_WIDTH:
		g_value_set_int(value, element->face_width);
		break;

	case ARG_FACE_HEIGHT:
		g_value_set_int(value, element->face_height);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

	GST_OBJECT_UNLOCK(element);
}


static void finalize(GObject *object)
{
	GstFace2RGB *element = GST_FACE_2_RGB(object);

	g_free(element->mask);
	element->mask = NULL;

	/*
	 * chain to parent class' finalize() method
	 */

	G_OBJECT_CLASS(parent_class)->finalize(object);
}


static void gst_face_2_rgb_base_init(gpointer klass)
{
	/* no-op */
}


static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
	GST_BASE_TRANSFORM_SINK_NAME,
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"video/x-raw-rgb, " \
		"bpp = (int) 24, " \
		"depth = (int) 24, " \
		"endianness = (int) 4321, " \
		"red_mask = (int) 16711680, " \
		"green_mask = (int) 65280, " \
		"blue_mask = (int) 255, " \
		"width = (int) [1, MAX], " \
		"height = (int) [1, MAX], " \
		"framerate = (fraction) [0/1, 2147483647/1]"
	)
);


static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
	GST_BASE_TRANSFORM_SRC_NAME,
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw-float, " \
		"channels = (int) 3, " \
		"endianness = (int) BYTE_ORDER, " \
		"rate = (fraction) [0/1, 2147483647/1], " \
		"width = (int) 64"
	)
);


static void gst_face_2_rgb_class_init(GstFace2RGBClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->set_property = GST_DEBUG_FUNCPTR(set_property);
	gobject_class->get_property = GST_DEBUG_FUNCPTR(get_property);
	gobject_class->finalize = GST_DEBUG_FUNCPTR(finalize);

	transform_class->get_unit_size = GST_DEBUG_FUNCPTR(get_unit_size);
	transform_class->transform_caps = GST_DEBUG_FUNCPTR(transform_caps);
	transform_class->set_caps = GST_DEBUG_FUNCPTR(set_caps);
	transform_class->start = GST_DEBUG_FUNCPTR(start);
	transform_class->transform = GST_DEBUG_FUNCPTR(transform);

	gst_element_class_set_details_simple(element_class, 
		"Face to RGB time series",
		"Filter/Video",
		"Convert a video stream to red, green, blue time series",
		"Kipp Cannon <kipp.cannon@ligo.org>"
	);

	g_object_class_install_property(
		gobject_class,
		ARG_GAMMA,
		g_param_spec_double(
			"gamma",
			"gamma",
			"Gamma correction.",
			0, G_MAXDOUBLE, DEFAULT_GAMMA,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_FACE_X,
		g_param_spec_int(
			"face-x",
			"face X",
			"Left edge of face.",
			G_MININT, G_MAXINT, DEFAULT_FACE_X,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_FACE_Y,
		g_param_spec_int(
			"face-y",
			"face y",
			"Top edge of face.",
			G_MININT, G_MAXINT, DEFAULT_FACE_Y,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_FACE_WIDTH,
		g_param_spec_int(
			"face-width",
			"face width",
			"Width of face (0 = use width of video).",
			G_MININT, G_MAXINT, DEFAULT_FACE_WIDTH,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_FACE_HEIGHT,
		g_param_spec_int(
			"face-height",
			"face height",
			"Height of face (0 = use height of video).",
			G_MININT, G_MAXINT, DEFAULT_FACE_HEIGHT,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}


static void gst_face_2_rgb_init(GstFace2RGB *element, GstFace2RGBClass *klass)
{
	gst_base_transform_set_gap_aware(GST_BASE_TRANSFORM(element), TRUE);

	element->mask = NULL;
	element->need_new_mask = TRUE;
}
