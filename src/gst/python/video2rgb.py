import numpy


import pygtk
pygtk.require("2.0")
import gobject
import pygst
pygst.require('0.10')
import gst


__author__ = "Kipp Cannon <kipp.cannon@ligo.org>"
__version__ = "FIXME"
__date__ = "FIXME"


video_caps = (
	"video/x-raw-rgb, " +
	"bpp = (int) 24, " +
	"depth = (int) 24, " +
	"red_mask = (int) 16711680, " +
	"green_mask = (int) 65280, " +
	"blue_mask = (int) 255, " +
	"endianness = (int) 4321"
)


video_dtype = numpy.dtype({"names": ["r", "g", "b"], "formats": [numpy.uint8, numpy.uint8, numpy.uint8]})
timeseries_dtype = numpy.dtype({"names": ["r", "g", "b"], "formats": [numpy.double, numpy.double, numpy.double]})


def get_unit_size(caps):
	struct = caps[0]
	name = struct.get_name()
	if name in ("audio/x-raw-complex", "audio/x-raw-float", "audio/x-raw-int"):
		return struct["channels"] * struct["width"] // 8
	elif name == "video/x-raw-rgb":
		return struct["width"] * struct["height"] * struct["bpp"] // 8
	raise ValueError(caps)


class video2rgb(gst.BaseTransform):
	__gstdetails__ = (
		"Video to RGB time series",
		"Filter/Video",
		"Convert a video stream to red, green, blue time series",
		__author__
	)

	__gproperties__ = {
		"gamma": (
			gobject.TYPE_DOUBLE,
			"gamma",
			"Gamma factor.",
			0, gobject.G_MAXDOUBLE, 1.0,
			gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT
		)
	}

	__gsttemplates__ = (
		gst.PadTemplate("sink",
			gst.PAD_SINK,
			gst.PAD_ALWAYS,
			gst.caps_from_string(
				video_caps + ", " +
				"width = (int) [1, MAX], " +
				"height = (int) [1, MAX], " +
				"framerate = (fraction) [0/1, 2147483647/1]"
			)
		),
		gst.PadTemplate("src",
			gst.PAD_SRC,
			gst.PAD_ALWAYS,
			gst.caps_from_string(
				"audio/x-raw-float, " +
				"channels = (int) 3, " +
				"endianness = (int) BYTE_ORDER, " +
				"rate = (int) [1, MAX], " +
				"width = (int) 64"
			)
		)
	)


	def __init__(self):
		gst.BaseTransform.__init__(self)
		self.get_pad("sink").use_fixed_caps()
		self.get_pad("src").use_fixed_caps()


	def do_set_property(self, prop, val):
		if prop.name == "gamma":
			self.gamma = val
		else:
			raise AssertionError("no property %s" % prop.name)


	def do_get_property(self, prop):
		if prop.name == "gamma":
			return self.gamma
		raise AssertionError("no property %s" % prop.name)


	def do_set_caps(self, incaps, outcaps):
		self.framerate = incaps[0]["framerate"]
		self.width = incaps[0]["width"]
		self.height = incaps[0]["height"]
		y, x = numpy.indices((self.height, self.width), dtype = "double")
		x = 2 * x / (self.width - 1) - 1
		y = 2 * y / (self.height - 1) - 1
		self.mask = (x**2 + y**2) > 1
		return True


	def do_get_unit_size(self, caps):
		return get_unit_size(caps)


	def do_start(self):
		self.framecount = 0
		return True


	def do_transform(self, inbuf, outbuf):
		#
		# wrap input and output buffers in numpy arrays.  assigning
		# to .shape ensures the reshape is done as a zero-copy
		# transform.  if the .reshape() method is used it always
		# works but doesn't always do a zero-copy transform.
		#

		indata = numpy.ma.frombuffer(inbuf, dtype = video_dtype)
		indata.shape = (self.height, self.width)
		indata.mask = self.mask
		outdata = numpy.frombuffer(outbuf, dtype = timeseries_dtype)

		#
		# map to [0, 1], apply gamma correction and average RGB
		# values
		#

		outdata["r"][0] = ((indata["r"] / 255.0)**self.gamma).mean()
		outdata["g"][0] = ((indata["g"] / 255.0)**self.gamma).mean()
		outdata["b"][0] = ((indata["b"] / 255.0)**self.gamma).mean()

		#
		# set metadata on output buffer
		#

		outbuf.offset = self.framecount
		outbuf.offset_end = self.framecount + 1
		outbuf.timestamp = inbuf.timestamp
		outbuf.duration = inbuf.duration

		#
		# done
		#

		self.framecount += 1
		return gst.FLOW_OK


	def do_transform_caps(self, direction, caps):
		#
		# FIXME:  this is hacked to round fractional frame rates to
		# integer audio sample rates
		#

		if direction == gst.PAD_SRC:
			#
			# convert src pad's caps to sink pad's
			#

			framerate, = [struct["rate"] for struct in caps]
			result = gst.Caps()
			for struct in self.get_pad("sink").get_pad_template_caps():
				struct = struct.copy()
				struct["framerate"] = gst.Fraction(framerate, 1)
				result.append_structure(struct)
			return result

		elif direction == gst.PAD_SINK:
			#
			# convert sink pad's caps to src pad's
			#

			framerate, = [struct["framerate"] for struct in caps]
			result = gst.Caps()
			for struct in self.get_pad("src").get_pad_template_caps():
				struct = struct.copy()
				struct["rate"] = int(round(float(framerate.num) / float(framerate.denom)))
				result.append_structure(struct)
			return result

		raise ValueError(direction)


#
# register element class
#


gobject.type_register(video2rgb)

__gstelementfactory__ = (
	video2rgb.__name__,
	gst.RANK_NONE,
	video2rgb
)
