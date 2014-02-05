/*
 * GstFace2RGB
 *
 * Copyright (C) 2013,2014  Kipp Cannon
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
#include <gst/audio/audio.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>


#include <face2rgb.h>


#define DEFAULT_GAMMA 1.0
#define DEFAULT_FACE_X 0
#define DEFAULT_FACE_Y 0
#define DEFAULT_FACE_WIDTH 0
#define DEFAULT_FACE_HEIGHT 0
#define DEFAULT_NOSE_X 0
#define DEFAULT_NOSE_Y 0
#define DEFAULT_NOSE_WIDTH 0
#define DEFAULT_NOSE_HEIGHT 0
#define DEFAULT_EYES_X 0
#define DEFAULT_EYES_Y 0
#define DEFAULT_EYES_WIDTH 0
#define DEFAULT_EYES_HEIGHT 0


enum mask_t {
	MASK_BG = 0,
	MASK_FOREHEAD,
	MASK_CHEEK,
	MASK_UNUSED
};


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


#define GST_CAT_DEFAULT gst_face_2_rgb_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);


static void additional_initializations(void)
{
	GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "face2rgb", 0, "face2rgb element");
}


G_DEFINE_TYPE_WITH_CODE(GstFace2RGB, gst_face_2_rgb, GST_TYPE_BASE_TRANSFORM, additional_initializations(););


/*
 * ============================================================================
 *
 *                             Internal Functions
 *
 * ============================================================================
 */


static void make_mask(GstFace2RGB *element)
{
	gint *mask = element->mask;
	gint x, y;
	gint area_forehead = 0, area_cheek = 0, area_bg = 0;
	gint face_width = element->face_width > 0 ? element->face_width : element->width;
	gint face_height = element->face_height > 0 ? element->face_height : element->height;
	const gdouble x_scale = 2.0 / face_width;
	const gdouble y_scale = 2.0 / face_height;

	/*
	 * mark background, forehead, and cheek areas in mask
	 *
	 * face_x, face_y are centred and scaled so that the face is the
	 * unit circle
	 */

	for(y = 0; y < element->height; y++) {
		gdouble face_y = (y - element->face_y) * y_scale - 1.0;
		gdouble face_y_squared = face_y * face_y;
		/* short-cut this whole row if possible */
		if(face_y_squared > 1.0) {
			for(x = 0; x < element->width; x++, mask++) {
				*mask = MASK_BG;
				area_bg++;
			}
			continue;
		}
		for(x = 0; x < element->width; x++, mask++) {
			gdouble face_x = (x - element->face_x) * x_scale - 1.0;
			if(face_x * face_x + face_y_squared > 1.0) {
				*mask = MASK_BG;
				area_bg++;
			} else if(y < element->eyes_y) {
				*mask = MASK_FOREHEAD;
				area_forehead++;
			} else if(y >= element->eyes_y + element->eyes_height && (x < element->nose_x || x >= element->nose_x + element->nose_width)) {
				*mask = MASK_CHEEK;
				area_cheek++;
			} else {
				/* unused pixel */
				*mask = MASK_UNUSED;
			}
		}
	}

	/*
	 * non-face pixel count to face pixel count ratio
	 */

	GST_DEBUG_OBJECT(element, "forehead is %d pixels, cheeks are %d pixels", area_forehead, area_cheek);
	element->bg_over_forehead_area_ratio = area_forehead ? (double) area_bg / area_forehead : 0;
	element->bg_over_cheek_area_ratio = area_cheek ? (double) area_bg / area_cheek : 0;
}


/*
 * ============================================================================
 *
 *                          GstBaseTransform Methods
 *
 * ============================================================================
 */


static gboolean get_unit_size(GstBaseTransform *trans, GstCaps *caps, gsize *size)
{
	GstStructure *str;
	gboolean success = TRUE;

	str = gst_caps_get_structure(caps, 0);
	if(!g_strcmp0(gst_structure_get_name(str), "audio/x-raw")) {
		/* can't use gst_audio_info_from_caps():  doesn't
		 * understand non-integer sample rates */
		*size = 6 * 8;	/* FIXME:  hard-coded to 6 double-precision channels */
	} else if(!g_strcmp0(gst_structure_get_name(str), "video/x-raw")) {
		GstVideoInfo info;
		success = gst_video_info_from_caps(&info, caps);
		if(success)
			*size = GST_VIDEO_INFO_SIZE(&info);
	} else
		success = FALSE;

	if(!success)
		GST_ERROR_OBJECT(trans, "could not parse caps");

	return success;
}


static GstCaps *transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter)
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
	GstVideoInfo info;
	gboolean success = TRUE;

	success &= gst_video_info_from_caps(&info, incaps);
	if(success) {
		element->width = GST_VIDEO_INFO_WIDTH(&info);
		element->height = GST_VIDEO_INFO_HEIGHT(&info);
		element->stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
		element->mask = g_realloc_n(element->mask, element->height * element->width, sizeof(*element->mask));
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
	const gdouble gamma = element->gamma;
	GstMapInfo srcmap, dstmap;
	gdouble *out;
	guchar *row, *last_row;
	gint *mask;
	gdouble forehead_r = 0.0, cheek_r = 0.0, bg_r = 0.0;
	gdouble forehead_g = 0.0, cheek_g = 0.0, bg_g = 0.0;
	gdouble forehead_b = 0.0, cheek_b = 0.0, bg_b = 0.0;
	gdouble bg_y;

	if(element->need_new_mask) {
		make_mask(element);
		element->need_new_mask = FALSE;
	}
	mask = element->mask;
	g_return_val_if_fail(mask != NULL, GST_FLOW_ERROR);

	/*
	 * apply gamma correction, and sum forehead, cheek, and background
	 * RGB components
	 */

	gst_buffer_map(inbuf, &srcmap, GST_MAP_READ);
	row = (guchar *) srcmap.data;
	last_row = row + element->height * element->stride;
	for(; row < last_row; row += element->stride) {
		guchar *in = row;
		guchar *last_col = in + 3 * element->width;
		for(; in < last_col;) {
			/* we don't need to scale these into the range [0,
			 * 1] because the factor of 255 will appear (raised
			 * to the power gamma) in both the face and
			 * background components, and therefore will cancel
			 * itself out of the output */
			gdouble r = *in++;
			gdouble g = *in++;
			gdouble b = *in++;
			if(gamma != 1.0) {
				r = pow(r, gamma);
				g = pow(g, gamma);
				b = pow(b, gamma);
			}
			switch((enum mask_t) *mask++) {
			case MASK_BG:
				bg_r += r;
				bg_g += g;
				bg_b += b;
				break;

			case MASK_FOREHEAD:
				forehead_r += r;
				forehead_g += g;
				forehead_b += b;
				break;

			case MASK_CHEEK:
				cheek_r += r;
				cheek_g += g;
				cheek_b += b;
				break;

			case MASK_UNUSED:
				break;
			}
		}
	}
	gst_buffer_unmap(inbuf, &srcmap);

	/*
	 * compute background brightness
	 */

	bg_y = 0.2126 * bg_r + 0.7152 * bg_g + 0.0722 * bg_b;

	/*
	 * set output sample values
	 */

	gst_buffer_map(outbuf, &dstmap, GST_MAP_WRITE);
	out = (gdouble *) dstmap.data;
	out[0] = forehead_r * element->bg_over_forehead_area_ratio / bg_y;
	out[1] = forehead_g * element->bg_over_forehead_area_ratio / bg_y;
	out[2] = forehead_b * element->bg_over_forehead_area_ratio / bg_y;
	out[3] = cheek_r * element->bg_over_cheek_area_ratio / bg_y;
	out[4] = cheek_g * element->bg_over_cheek_area_ratio / bg_y;
	out[5] = cheek_b * element->bg_over_cheek_area_ratio / bg_y;
	gst_buffer_unmap(outbuf, &dstmap);

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
	ARG_FACE_HEIGHT,
	ARG_NOSE_X,
	ARG_NOSE_Y,
	ARG_NOSE_WIDTH,
	ARG_NOSE_HEIGHT,
	ARG_EYES_X,
	ARG_EYES_Y,
	ARG_EYES_WIDTH,
	ARG_EYES_HEIGHT,
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

	case ARG_NOSE_X:
		element->nose_x = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_NOSE_Y:
		element->nose_y = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_NOSE_WIDTH:
		element->nose_width = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_NOSE_HEIGHT:
		element->nose_height = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_EYES_X:
		element->eyes_x = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_EYES_Y:
		element->eyes_y = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_EYES_WIDTH:
		element->eyes_width = g_value_get_int(value);
		element->need_new_mask = TRUE;
		break;

	case ARG_EYES_HEIGHT:
		element->eyes_height = g_value_get_int(value);
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

	case ARG_NOSE_X:
		g_value_set_int(value, element->nose_x);
		break;

	case ARG_NOSE_Y:
		g_value_set_int(value, element->nose_y);
		break;

	case ARG_NOSE_WIDTH:
		g_value_set_int(value, element->nose_width);
		break;

	case ARG_NOSE_HEIGHT:
		g_value_set_int(value, element->nose_height);
		break;

	case ARG_EYES_X:
		g_value_set_int(value, element->eyes_x);
		break;

	case ARG_EYES_Y:
		g_value_set_int(value, element->eyes_y);
		break;

	case ARG_EYES_WIDTH:
		g_value_set_int(value, element->eyes_width);
		break;

	case ARG_EYES_HEIGHT:
		g_value_set_int(value, element->eyes_height);
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

	G_OBJECT_CLASS(gst_face_2_rgb_parent_class)->finalize(object);
}


static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
	GST_BASE_TRANSFORM_SINK_NAME,
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"video/x-raw, " \
		"format = (string) RGB, " \
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
		"audio/x-raw, " \
			"format = (string) " GST_AUDIO_NE(F64) ", " \
			"channels = (int) 6, " \
			"rate = (fraction) [0/1, MAX], " \
			"layout = (string) interleaved, " \
			"channel-mask = (bitmask) 0"
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
		"Convert a video stream to forehead and cheek, red, green, blue time series triples",
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

	g_object_class_install_property(
		gobject_class,
		ARG_NOSE_X,
		g_param_spec_int(
			"nose-x",
			"nose X",
			"Left edge of nose.",
			G_MININT, G_MAXINT, DEFAULT_NOSE_X,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_NOSE_Y,
		g_param_spec_int(
			"nose-y",
			"nose y",
			"Top edge of nose.",
			G_MININT, G_MAXINT, DEFAULT_NOSE_Y,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_NOSE_WIDTH,
		g_param_spec_int(
			"nose-width",
			"nose width",
			"Width of nose (0 = use width of video).",
			G_MININT, G_MAXINT, DEFAULT_NOSE_WIDTH,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_NOSE_HEIGHT,
		g_param_spec_int(
			"nose-height",
			"nose height",
			"Height of nose (0 = use height of video).",
			G_MININT, G_MAXINT, DEFAULT_NOSE_HEIGHT,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_EYES_X,
		g_param_spec_int(
			"eyes-x",
			"eyes X",
			"Left edge of eyes.",
			G_MININT, G_MAXINT, DEFAULT_EYES_X,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_EYES_Y,
		g_param_spec_int(
			"eyes-y",
			"eyes y",
			"Top edge of eyes.",
			G_MININT, G_MAXINT, DEFAULT_EYES_Y,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_EYES_WIDTH,
		g_param_spec_int(
			"eyes-width",
			"eyes width",
			"Width of eyes (0 = use width of video).",
			G_MININT, G_MAXINT, DEFAULT_EYES_WIDTH,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	g_object_class_install_property(
		gobject_class,
		ARG_EYES_HEIGHT,
		g_param_spec_int(
			"eyes-height",
			"eyes height",
			"Height of eyes (0 = use height of video).",
			G_MININT, G_MAXINT, DEFAULT_EYES_HEIGHT,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT
		)
	);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
}


static void gst_face_2_rgb_init(GstFace2RGB *element)
{
	gst_base_transform_set_gap_aware(GST_BASE_TRANSFORM(element), TRUE);

	element->mask = NULL;
	element->need_new_mask = TRUE;
}
