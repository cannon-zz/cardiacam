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


#ifndef __FACE_2_RGB_H__
#define __FACE_2_RGB_H__


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


G_BEGIN_DECLS


/*
 * ============================================================================
 *
 *                                    Type
 *
 * ============================================================================
 */


#define GST_TYPE_FACE_2_RGB \
	(gst_face_2_rgb_get_type())
#define GST_FACE_2_RGB(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_FACE_2_RGB, GstFace2RGB))
#define GST_FACE_2_RGB_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_FACE_2_RGB, GstFace2RGBClass))
#define GST_FACE_2_RGB_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_FACE_2_RGB, GstFace2RGBClass))
#define GST_IS_FACE_2_RGB(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_FACE_2_RGB))
#define GST_IS_FACE_2_RGB_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_FACE_2_RGB))


typedef struct _GstFace2RGBClass GstFace2RGBClass;
typedef struct _GstFace2RGB GstFace2RGB;


struct _GstFace2RGBClass {
	GstBaseTransformClass parent_class;
};


/**
 * GstVideoRationalResample
 */


struct _GstFace2RGB {
	GstBaseTransform basetransform;

	gdouble gamma;
	gint face_x, face_y;	/* pixels */
	gint face_width, face_height;	/* pixels */
	gint nose_x, nose_y;	/* pixels */
	gint nose_width, nose_height;	/* pixels */
	gint eyes_x, eyes_y;	/* pixels */
	gint eyes_width, eyes_height;	/* pixels */
	gboolean need_new_mask;

	gint width, height;	/* pixels */
	gint stride;	/* bytes */
	gint *mask;
	gdouble bg_over_forehead_area_ratio;
	gdouble bg_over_cheek_area_ratio;

	guint64 offset;
};


/*
 * ============================================================================
 *
 *                                Exported API
 *
 * ============================================================================
 */


GType gst_face_2_rgb_get_type(void);


G_END_DECLS


#endif	/* __FACE_2_RGB_H__ */
