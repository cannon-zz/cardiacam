/*
 * GstWildDevine
 *
 * Copyright (C) 2014  Kipp Cannon
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


#ifndef __WILDDEVINE_H__
#define __WILDDEVINE_H__


/*
 * ============================================================================
 *
 *                                  Preamble
 *
 * ============================================================================
 */


#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <libusb.h>


G_BEGIN_DECLS


/*
 * ============================================================================
 *
 *                                    Type
 *
 * ============================================================================
 */


#define GST_TYPE_WILDDEVINE \
	(gst_wilddevine_get_type())
#define GST_WILDDEVINE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_WILDDEVINE, GstWildDevine))
#define GST_WILDDEVINE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WILDDEVINE, GstWildDevineClass))
#define GST_WILDDEVINE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_WILDDEVINE, GstWildDevineClass))
#define GST_IS_WILDDEVINE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_WILDDEVINE))
#define GST_IS_WILDDEVINE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WILDDEVINE))


typedef struct _GstWildDevineClass GstWildDevineClass;
typedef struct _GstWildDevine GstWildDevine;


struct _GstWildDevineClass {
	GstBaseSrcClass parent_class;
};


/**
 * GstWildDevine
 */


struct _GstWildDevine {
	GstBaseSrc basesrc;

	/*
	 * usb context
	 */

	libusb_context *usb_context;
	libusb_device_handle *usb_handle;

	/*
	 * hardware information
	 */

	guint64 version;
	guint64 serial;

	/*
	 * sample index
	 */

	guint64 next_offset;

	/*
	 * sample collection thread
	 */

	GList *queue;
	GMutex queue_lock;
	GCond queue_data_avail;
	GThread *collect_thread;
	gboolean pll_locked;
	gboolean stop_requested;
	GstFlowReturn collect_status;
};


/*
 * ============================================================================
 *
 *                                Exported API
 *
 * ============================================================================
 */


GType gst_wilddevine_get_type(void);


G_END_DECLS


#endif	/* __WILDDEVINE_H__ */
