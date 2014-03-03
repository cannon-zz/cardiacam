import bisect
import itertools
import logging
import numpy
import sys
from cardiacam import ica

logging.basicConfig(level = logging.INFO)


transient = (7.0, 0.5)	# start, stop in seconds


def load_rgb(fileobj):
	rgb = numpy.loadtxt(fileobj)
	t = rgb[:,0]
	rgb = rgb[:,1:]
	logging.info("loaded %d samples spanning [%g s, %g s)" % (len(t), t[0], t[-1]))
	deltas = t[1:] - t[:-1]
	if deltas.ptp() > 1e-8:
		gapindex = deltas.argmax()
		logging.warn("sample rate is not uniform, suspect %g s gap in data at %.16g s" % (deltas[gapindex] - deltas.mean(), t[gapindex]))
	return t, rgb


def differentiate(rgb, t):
	rgb = (rgb[2:] - rgb[:-2]).T / (t[2:] - t[:-2])
	return rgb.T, t[1:-1]


# load rgb.txt
t, rgb = load_rgb(sys.stdin)
logging.info("mean sample rate is %g Hz" % (len(t) / (t[-1] - t[0])))

# clip start and stop transients
transient = bisect.bisect(t, t[0] + transient[0]), bisect.bisect(t, t[-1] - transient[1])
rgb = rgb[transient[0]:transient[1]]
t = t[transient[0]:transient[-1]]
logging.info("after transient removal, have %d samples spanning [%g s, %g s)" % (len(t), t[0], t[-1]))

# replace RGB time series with its 1st derivative to whiten the
# background noise spectrum
rgb, t = differentiate(rgb, t)


# how to compute unmix transform
def unmix_rgb(rgb, n_comp = 3):
	# relationship between the RGB streams, the unmixed streams, and
	# the two other matrices returned by this function is:
	#
	#	s = numpy.dot(rgb - rgb.mean(0), numpy.asmatrix(k) * w)
	#
	# where rgb and s are both (samples x 3) matrices and k and w are
	# 3x3 matrices
	k, w, s = ica.fastica(rgb, fun = "exp", n_comp = n_comp, w_init = None, maxit = 40000, tol = 1e-14)
	unmix = numpy.asarray(numpy.asmatrix(k) * w)

	#
	# the unmix matrix is not unique.  the component order is undefined
	# and the sign of the components is ambiguous.  the following 5
	# constrains fix the under and signs of the three components.
	#

	# move greenest component to position 0, keeping all matrices
	# consistent
	i = abs(unmix)[1].argmax()
	s = numpy.roll(s, -i, 1)
	w = numpy.roll(w, -i, 1)
	unmix = numpy.roll(unmix, -i, 1)

	# of the other two, move bluest component to position 1, keeping
	# all matrices consistent
	i = abs(unmix)[2][1:].argmax()
	s[:,1:] = numpy.roll(s[:,1:], -i, 1)
	w[:,1:] = numpy.roll(w[:,1:], -i, 1)
	unmix[:,1:] = numpy.roll(unmix[:,1:], -i, 1)

	# make the green coefficient of the greenest component negative,
	# keeping all matrices consistent
	if unmix[1,0] > 0:
		s[:,0] *= -1.0
		w[:,0] *= -1.0
		unmix[:,0] *= -1.0

	# make the blue coefficient of the bluest component positive,
	# keeping all matrices consistent
	if unmix[2,1] < 0:
		s[:,1] *= -1.0
		w[:,1] *= -1.0
		unmix[:,1] *= -1.0

	# make the red coefficient of the final component positive,
	# keeping all matrices consistent
	if unmix[0,2] < 0:
		s[:,2] *= -1.0
		w[:,2] *= -1.0
		unmix[:,2] *= -1.0

	logging.info("unmix = %s" % str(unmix))

	# make sure we've not screwed up the matrices
	assert abs(unmix - numpy.asarray(numpy.asmatrix(k) * w)).max() < 1e-14
	assert abs(s - numpy.dot(rgb - rgb.mean(0), unmix)).max() < 1e-14

	return unmix, s

# add the forehead and cheek streams together to increase the SNR for
# finding the unmix transform
unmix, s = unmix_rgb(rgb[:,:3] + rgb[:,3:])

# now unmix the forehead and cheek streams
forehead_s = numpy.dot(rgb[:,:3] - rgb[:,:3].mean(0), unmix)
cheek_s = numpy.dot(rgb[:,3:] - rgb[:,3:].mean(0), unmix)

# write output
output = sys.stdout
fmt = "%.16g" + " %.16g" * (forehead_s.shape[1] + cheek_s.shape[1])
for t, forehead_x, cheek_x in itertools.izip(t, forehead_s[:], cheek_s[:]):
	print >>output, fmt % ((t,) + tuple(forehead_x) + tuple(cheek_x))
