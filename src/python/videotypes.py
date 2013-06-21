import numpy


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
