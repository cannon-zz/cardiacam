/*
 * GstRateFaker
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


#ifndef __RATE_FAKER_H__
#define __RATE_FAKER_H__


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


#define GST_TYPE_RATE_FAKER \
	(gst_rate_faker_get_type())
#define GST_RATE_FAKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_RATE_FAKER, GstRateFaker))
#define GST_RATE_FAKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_RATE_FAKER, GstRateFakerClass))
#define GST_RATE_FAKER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_RATE_FAKER, GstRateFakerClass))
#define GST_IS_RATE_FAKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_RATE_FAKER))
#define GST_IS_RATE_FAKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_RATE_FAKER))


typedef struct _GstRateFakerClass GstRateFakerClass;
typedef struct _GstRateFaker GstRateFaker;


struct _GstRateFakerClass {
	GstBaseTransformClass parent_class;
};


/**
 * GstAudioRationalResample
 */


struct _GstRateFaker {
	GstBaseTransform basetransform;

	gint inrate_over_outrate_num;
	gint inrate_over_outrate_den;
};


/*
 * ============================================================================
 *
 *                                Exported API
 *
 * ============================================================================
 */


GType gst_rate_faker_get_type(void);


G_END_DECLS


#endif	/* __RATE_FAKER_H__ */
