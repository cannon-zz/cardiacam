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


/*
 * doing a fit to the time-of-arrivals, I measured the intrinsic mean
 * sample rate of my device to be about 29.78805087 Hz.  the timing
 * residuals exhibited strong quantization, being almost exlusively (at one
 * part per million) small integer multiples of +/- 1 ms away from the
 * expected time-of-arrival.  I assume that the samples are being captured
 * within the device at a uniform sample rate, i.e. that the
 * time-of-arrival variability has its origin in the USB transfer not in
 * the device's DAQ.  this code, therefore, implements a software PLL to
 * reconstruct the true time of each sample.  after reclocking, the samples
 * are interpolated onto a time series having an integer sample rate (for
 * compatibility with gstreamer) using a windowed sinc kernel.
 */


/*
 * ============================================================================
 *
 *                                  Preamble
 *
 * ============================================================================
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>


#include <libusb.h>


#include <glib.h>
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbasesrc.h>


#include <wilddevine.h>


#define WILDDEVINE_VEND_ID 0x14fa
#define WILDDEVINE_PROD_ID 0x0001
#define WILDDEVINE_INTERFACE 0
#define WILDDEVINE_ENDPOINT 0x81
#define WILDDEVINE_TIMEOUT 80		/* milliseconds */


#define RATE 50				/* Hertz */
#define UNIT_SIZE (2*sizeof(float))	/* bytes (2 floats) */
#define KERNEL_LENGTH 10		/* samples */


#undef PLL_DEBUG			/* dump PLL debug info */
#ifdef PLL_DEBUG
#include <stdio.h>
#endif


struct queued_sample {
	GstClockTime t;
	GstClockTimeDiff dt;
	gfloat scl;
	gfloat ppg;
};


/*
 * ============================================================================
 *
 *                                Boilerplate
 *
 * ============================================================================
 */


#define GST_CAT_DEFAULT gst_wilddevine_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);


static void additional_initializations(void)
{
	GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "wilddevine", 0, "wilddevine element");
}


G_DEFINE_TYPE_WITH_CODE(GstWildDevine, gst_wilddevine, GST_TYPE_BASE_SRC, additional_initializations(););


/*
 * ============================================================================
 *
 *                             Internal Functions
 *
 * ============================================================================
 */


/*
 * sample clock PLL
 */


struct pll {
	GstClockTime t;
	GstClockTimeDiff dt;
};


static struct pll *pll_new(void)
{
	struct pll *pll = g_new(struct pll, 1);

	pll->t = GST_CLOCK_TIME_NONE;
	pll->dt = 0;

	return pll;
}


static void pll_free(struct pll *pll)
{
	g_free(pll);
}


static GstClockTime pll_correct(struct pll *pll, GstClockTime t, gboolean *locked)
{
	GstClockTimeDiff error;

	g_assert(GST_CLOCK_TIME_IS_VALID(t));

	if(!GST_CLOCK_TIME_IS_VALID(pll->t)) {
		pll->t = t;
		*locked = FALSE;
	} else {
		if(!pll->dt) {
			g_assert_cmpuint(t, >=, pll->t);
			pll->dt = t - pll->t;
		}
#ifdef PLL_DEBUG
		fprintf(stderr, "a: %lu %lu %ld\n", t, pll->t, pll->dt);
#endif
		pll->t += pll->dt;

		error = GST_CLOCK_DIFF(pll->t, t);	/* t - pll->t */
		*locked = pll->dt >= 2 * labs(error);
#ifdef PLL_DEBUG
		fprintf(stderr, "c: %ld\n", error);
#endif

		pll->t += error / 128;
		pll->dt += error / 1024;
		g_assert_cmpint(pll->dt, >, 0);
	}

	return pll->t;
}


static GstClockTimeDiff pll_period(const struct pll *pll)
{
	return pll->dt;
}


/*
 * wrapper to retrieve a regex match, parse as base 16 int, and return
 * result.
 */


static unsigned long fetch_wilddevine_sample(const GMatchInfo *match_info, gint n)
{
	gchar *s = g_match_info_fetch(match_info, n);
	unsigned long x = strtol(s, NULL, 16);
	g_free(s);
	return x;
}


/*
 * open WildDevine device
 */


static gboolean open_device(GstWildDevine *element)
{
	libusb_device **list;
	libusb_device *device = NULL;
	ssize_t n = libusb_get_device_list(element->usb_context, &list);
	ssize_t i;

	if(n < 0) {
		GST_ERROR_OBJECT(element, "libusbb_get_devie_list() failed");
		goto done;
	}

	for(i = 0; i < n; i++) {
		struct libusb_device_descriptor desc;
		if(libusb_get_device_descriptor(list[i], &desc)) {
			GST_ERROR_OBJECT(element, "libusb_get_device_descriptor() failed");
			goto done;
		}
		if(desc.idVendor == WILDDEVINE_VEND_ID && desc.idProduct == WILDDEVINE_PROD_ID) {
			device = list[i];
			break;
		}
	}

	if(!device) {
		GST_ERROR_OBJECT(element, "device not found");
		goto done;
	}

	if(libusb_open(device, &element->usb_handle)) {
		GST_ERROR_OBJECT(element, "libusb_open() failed");
		device = NULL;
		goto done;
	}

done:
	libusb_free_device_list(list, 1);
	return device != NULL;
}


/*
 * sample collection thread
 */


static gpointer collect_thread(gpointer _element)
{
	GstWildDevine *element = GST_WILDDEVINE(_element);
#if 1
	GstClock *clock = gst_system_clock_obtain();
	GstClockTime base_time = gst_clock_get_time(clock);
#else	/* why doesn't this work!? */
	GstClock *clock = gst_element_get_clock(GST_ELEMENT(_element));
	GstClockTime base_time = gst_element_get_base_time(GST_ELEMENT(_element));
#endif
	GRegex *raw_pattern, *ser_pattern, *ver_pattern;
	GString *buffer = g_string_new("");
	struct pll *pll = pll_new();

	raw_pattern = g_regex_new("<RAW>([[:xdigit:]]{4}) ([[:xdigit:]]{4})<\\\\RAW>", G_REGEX_MULTILINE | G_REGEX_OPTIMIZE, 0, NULL);
	ser_pattern = g_regex_new("<SER>([[:xdigit:]]+)<\\\\SER>", G_REGEX_MULTILINE | G_REGEX_OPTIMIZE, 0, NULL);
	ver_pattern = g_regex_new("<VER>([[:xdigit:]]+)<\\\\VER>", G_REGEX_MULTILINE | G_REGEX_OPTIMIZE, 0, NULL);
	g_assert(raw_pattern && ser_pattern && ver_pattern);

	while(1) {
		unsigned char read[8];
		int n;
		GstClockTime t;
		GMatchInfo *match_info;

		if(element->stop_requested) {
			element->collect_status = GST_FLOW_EOS;
			break;
		}

		switch(libusb_interrupt_transfer(element->usb_handle, WILDDEVINE_ENDPOINT, read, sizeof(read), &n, WILDDEVINE_TIMEOUT)) {
		case 0:
			/* success */
			t = gst_clock_get_time(clock) - base_time;
			if(n != sizeof(read)) {
				/* ... but bad read size */
				GST_ERROR_OBJECT(element, "read %d bytes, expected %lu", n, sizeof(read));
				element->collect_status = GST_FLOW_ERROR;
				goto done;
			}
			break;

		case LIBUSB_ERROR_TIMEOUT:
			/* timeout */
			GST_ERROR_OBJECT(element, "timeout");
			element->collect_status = GST_FLOW_ERROR;
			goto done;

		case LIBUSB_ERROR_PIPE:
			/* transfer was halted */
			GST_ERROR_OBJECT(element, "transfer halted");
			element->collect_status = GST_FLOW_EOS;
			goto done;

		case LIBUSB_ERROR_OVERFLOW:
			/* too much data arrived */
			GST_ERROR_OBJECT(element, "transfer overflow");
			element->collect_status = GST_FLOW_ERROR;
			goto done;

		case LIBUSB_ERROR_NO_DEVICE:
			/* device has been unplugged */
			GST_ERROR_OBJECT(element, "unplugged");
			element->collect_status = GST_FLOW_EOS;
			goto done;

		default:
			/* other error */
			GST_ERROR_OBJECT(element, "unknown error");
			element->collect_status = GST_FLOW_ERROR;
			goto done;
		}

		/* first byte recieved gives number of remaining bytes that
		 * contain data */
		g_string_append_len(buffer, (const char *) &read[1], read[0]);

		/* check for version number */
		g_regex_match(ver_pattern, buffer->str, 0, &match_info);
		if(g_match_info_matches(match_info)) {
			guint64 version = fetch_wilddevine_sample(match_info, 1);
			if(version != element->version) {
				element->version = version;
				GST_INFO_OBJECT(element, "version = %lu", element->version);
				g_object_notify(G_OBJECT(element), "version");
			}
		}
		g_match_info_free(match_info);

		/* check for serial number */
		g_regex_match(ser_pattern, buffer->str, 0, &match_info);
		if(g_match_info_matches(match_info)) {
			guint64 serial = fetch_wilddevine_sample(match_info, 1);
			if(serial != element->serial) {
				element->serial = serial;
				GST_INFO_OBJECT(element, "serial = %lu", element->serial);
				g_object_notify(G_OBJECT(element), "serial");
			}
		}
		g_match_info_free(match_info);

		/* loop over sample matches, and remove data from buffer */
		n = 0;
		g_regex_match(raw_pattern, buffer->str, 0, &match_info);
		g_mutex_lock(&element->queue_lock);
		while(g_match_info_matches(match_info)) {
			struct queued_sample *sample = g_new(struct queued_sample, 1);
			gboolean locked;
			sample->t = pll_correct(pll, t, &locked);
			sample->dt = pll_period(pll);
			sample->scl = fetch_wilddevine_sample(match_info, 1) / 65536.0;
			sample->ppg = fetch_wilddevine_sample(match_info, 2) / 65536.0;

			g_match_info_fetch_pos(match_info, 0, NULL, &n);
			g_match_info_next(match_info, NULL);

			if(sample->dt > 0) {
				element->queue = g_list_prepend(element->queue, sample);
				g_cond_broadcast(&element->queue_data_avail);
			} else
				g_free(sample);
			if(locked != element->pll_locked) {
				element->pll_locked = locked;
				GST_INFO_OBJECT(element, locked ? "PLL locked" : "PLL unlocked");
				g_object_notify(G_OBJECT(element), "pll-locked");
			}
		}
		g_mutex_unlock(&element->queue_lock);
		g_match_info_free(match_info);
		g_string_erase(buffer, 0, n);
	}

done:
	pll_free(pll);
	g_string_free(buffer, TRUE);
	g_regex_unref(ver_pattern);
	g_regex_unref(ser_pattern);
	g_regex_unref(raw_pattern);
	gst_object_unref(clock);
	g_cond_broadcast(&element->queue_data_avail);
	return NULL;
}


/*
 * (t1 - t0) / dt
 */


static double sample_diff(GstClockTime t1, GstClockTime t0, GstClockTimeDiff dt)
{
	return ((double) GST_CLOCK_DIFF(t0, t1)) / dt;
}


/*
 * how many samples separate the head of the queue from t
 */


static int queued_look_ahead(const GList *list, GstClockTime t)
{
	struct queued_sample *sample;
	if(!list)
		return -1;
	sample = (struct queued_sample *) list->data;
	return round(sample_diff(sample->t, t, sample->dt));
}


/*
 * sinc kernel
 */


static double sinc(double x)
{
	x *= M_PI;
	return x != 0. ? sin(x) / x : 1.0;
}


/*
 * ============================================================================
 *
 *                             GstBaseSrc Methods
 *
 * ============================================================================
 */


static gboolean start(GstBaseSrc *src)
{
	GstWildDevine *element = GST_WILDDEVINE(src);
	gboolean success = TRUE;

	if(!open_device(element)) {
		success = FALSE;
		goto done;
	}

	libusb_detach_kernel_driver(element->usb_handle, WILDDEVINE_INTERFACE);
	if(libusb_claim_interface(element->usb_handle, WILDDEVINE_INTERFACE)) {
		GST_ERROR_OBJECT(element, "libusb_claim_interface() failed");
		libusb_close(element->usb_handle);
		element->usb_handle = NULL;
		success = FALSE;
		goto done;
	}

	element->next_offset = 0;

	g_assert(element->queue == NULL);

	element->stop_requested = FALSE;
	element->pll_locked = FALSE;
	element->collect_thread = g_thread_new(NULL, collect_thread, element);

done:
	return success;
}


static gboolean stop(GstBaseSrc *src)
{
	GstWildDevine *element = GST_WILDDEVINE(src);
	gboolean success = TRUE;

	element->stop_requested = TRUE;
	g_thread_join(element->collect_thread);
	element->collect_thread = NULL;

	g_list_free_full(element->queue, g_free);
	element->queue = NULL;

	libusb_release_interface(element->usb_handle, WILDDEVINE_INTERFACE);
	libusb_close(element->usb_handle);
	element->usb_handle = NULL;

	return success;
}


static gboolean unlock(GstBaseSrc *src)
{
	GST_WILDDEVINE(src)->stop_requested = TRUE;
	return TRUE;
}


static GstFlowReturn fill(GstBaseSrc *src, guint64 offset, guint size, GstBuffer *buf)
{
	GstWildDevine *element = GST_WILDDEVINE(src);
	GList *queue = NULL;
	GstMapInfo dstmap;
	gfloat *data;
	gint length, i;
	GstFlowReturn result = GST_FLOW_OK;

	g_assert(size % UNIT_SIZE == 0);

	/*
	 * set metadata
	 */

	GST_BUFFER_OFFSET(buf) = element->next_offset;
	element->next_offset += length = size / UNIT_SIZE;
	GST_BUFFER_OFFSET_END(buf) = element->next_offset;
	GST_BUFFER_PTS(buf) = GST_BUFFER_DTS(buf) = gst_util_uint64_scale_int_round(GST_BUFFER_OFFSET(buf), GST_SECOND, RATE);
	GST_BUFFER_DURATION(buf) = gst_util_uint64_scale_int_round(GST_BUFFER_OFFSET_END(buf), GST_SECOND, RATE) - GST_BUFFER_PTS(buf);

	/*
	 * wait for data.  after this, we can use queue without holding
	 * lock.  list manipulation performed by collect_thread() will not
	 * modify this part of the list, nor will what we do interfer with
	 * what it's doing
	 */

	g_mutex_lock(&element->queue_lock);
	while(element->collect_status == GST_FLOW_OK && queued_look_ahead(element->queue, GST_BUFFER_PTS(buf) + GST_BUFFER_DURATION(buf)) < KERNEL_LENGTH / 2)
		g_cond_wait(&element->queue_data_avail, &element->queue_lock);
	result = element->collect_status;
	queue = element->queue;
	g_mutex_unlock(&element->queue_lock);

	if(result != GST_FLOW_OK)
		goto done;

	/*
	 * fill buffer
	 */

	gst_buffer_map(buf, &dstmap, GST_MAP_WRITE);
	data = (gfloat *) dstmap.data;
	memset(data, 0, size);
	for(data = (gfloat *) dstmap.data, i = 0; i < length; data += 2, i++) {
		GstClockTime t = GST_BUFFER_PTS(buf) + gst_util_uint64_scale_int_round(i, GST_SECOND, RATE);
		GList *queue_elem;

		/*
		 * loop over queued data
		 */

		for(queue_elem = queue; queue_elem; queue_elem = g_list_next(queue_elem)) {
			struct queued_sample *sample = queue_elem->data;
			double dn = sample_diff(sample->t, t, sample->dt);
			double kernel;

			if(dn > KERNEL_LENGTH / 2)
				/* sample is too new */
				continue;

			if(dn < -KERNEL_LENGTH / 2) {
				/*
				 * from this sample to the end of the queue
				 * are no longer needed, remove them.  it
				 * should be impossible that this sequence
				 * includes the first element in the list
				 */

				g_assert(g_list_previous(queue_elem) != NULL);
				g_list_previous(queue_elem)->next = NULL;
				g_list_free_full(queue_elem, g_free);
				queue_elem = NULL;
				break;
			}

			/*
			 * channel 0:  skin conductance
			 * channel 1:  photoplethysmograph
			 */

			kernel = sinc(dn);
			data[0] += kernel * sample->scl;
			data[1] += kernel * sample->ppg;
		}
	}
	gst_buffer_unmap(buf, &dstmap);

done:
	return result;
}


/*
 * ============================================================================
 *
 *                              GObject Methods
 *
 * ============================================================================
 */


enum property {
	ARG_VERSION = 1,
	ARG_SERIAL,
	ARG_PLL_LOCKED
};


static void get_property(GObject *object, enum property prop_id, GValue *value, GParamSpec *pspec)
{
	GstWildDevine *element = GST_WILDDEVINE(object);

	GST_OBJECT_LOCK(element);

	switch(prop_id) {
	case ARG_VERSION:
		g_value_set_uint64(value, element->version);
		break;

	case ARG_SERIAL:
		g_value_set_uint64(value, element->serial);
		break;

	case ARG_PLL_LOCKED:
		g_value_set_boolean(value, element->pll_locked);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

	GST_OBJECT_UNLOCK(element);
}


static void finalize(GObject *object)
{
	GstWildDevine *element = GST_WILDDEVINE(object);

	g_assert(element->collect_thread == NULL);
	g_assert(element->queue == NULL);

	libusb_exit(element->usb_context);
	element->usb_context = NULL;
	g_mutex_clear(&element->queue_lock);
	g_cond_clear(&element->queue_data_avail);

	/*
	 * chain to parent class' finalize() method
	 */

	G_OBJECT_CLASS(gst_wilddevine_parent_class)->finalize(object);
}


static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw, " \
			"format = (string) " GST_AUDIO_NE(F32) ", " \
			"channels = (int) 2, " \
			"rate = (int) " G_STRINGIFY(RATE) ", " \
			"layout = (string) interleaved, " \
			"channel-mask = (bitmask) 0"
	)
);


static void gst_wilddevine_class_init(GstWildDevineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstBaseSrcClass *src_class = GST_BASE_SRC_CLASS(klass);

	object_class->finalize = GST_DEBUG_FUNCPTR(finalize);
	object_class->get_property = GST_DEBUG_FUNCPTR(get_property);

	src_class->start = GST_DEBUG_FUNCPTR(start);
	src_class->stop = GST_DEBUG_FUNCPTR(stop);
	src_class->unlock = GST_DEBUG_FUNCPTR(unlock);
	src_class->fill = GST_DEBUG_FUNCPTR(fill);

	gst_element_class_set_details_simple(element_class, 
		"WildDevine",
		"Source/Audio",
		"Retrieves skin conductance level and photoplethysmograph time series from a WildDevine capture device.",
		"Kipp Cannon <kipp.cannon@ligo.org>"
	);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));

	g_object_class_install_property(
		object_class,
		ARG_VERSION,
		g_param_spec_uint64(
			"version",
			"Version",
			"Hardware version number.",
			0, G_MAXUINT64, 0,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
		)
	);

	g_object_class_install_property(
		object_class,
		ARG_SERIAL,
		g_param_spec_uint64(
			"serial",
			"Serial number",
			"Hardware serial number.",
			0, G_MAXUINT64, 0,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
		)
	);

	g_object_class_install_property(
		object_class,
		ARG_PLL_LOCKED,
		g_param_spec_boolean(
			"pll-locked",
			"PLL Locked",
			"Software sample clock reconstruction is stable.",
			FALSE,
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
		)
	);
}


static void gst_wilddevine_init(GstWildDevine *element)
{
	GstBaseSrc *basesrc = GST_BASE_SRC(element);

	GST_OBJECT_FLAG_SET(element, GST_ELEMENT_FLAG_REQUIRE_CLOCK);

	gst_base_src_set_live(basesrc, TRUE);
	gst_base_src_set_format(basesrc, GST_FORMAT_TIME);
	gst_base_src_set_blocksize(basesrc, RATE / 10 * UNIT_SIZE);	/* 1/10th of a second */

	libusb_init(&element->usb_context);
	element->usb_handle = NULL;

	element->version = element->serial = -1;

	element->queue = NULL;
	g_mutex_init(&element->queue_lock);
	g_cond_init(&element->queue_data_avail);
	element->collect_thread = NULL;
	element->collect_status = GST_FLOW_OK;
}
