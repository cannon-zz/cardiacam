import itertools
import logging
import numpy
import sys
from cardiacam import ica

logging.basicConfig(level = logging.INFO)


# rgb
wguess = numpy.array(
	[[ 0.05648833,  0.97166715,  0.22950386],
	 [ 0.50415065,  0.17065149, -0.84658738],
	 [ 0.86176632, -0.16352682,  0.48022681]]
)
# default
#wguess = None

transient = (7.0, 1.0)	# start, stop in seconds


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
# for some reason need to skip a couple of samples before doing
# this
rate = int(round(1. / (t[3] - t[2])))
logging.info("sample rate seems to be %d Hz" % rate)

# clip start and stop transients
transient = int(round(transient[0] * rate)), int(round(transient[1] * rate))
rgb = rgb[transient[0]:-transient[1]]
t = t[transient[0]:-transient[-1]]
logging.info("after transient removal, have %d samples spanning [%g s, %g s)" % (len(t), t[0], t[-1]))

# replace RGB time series with its 1st derivative to whiten the
# background noise spectrum
rgb, t = differentiate(rgb, t)

# relationship is:  s = numpy.dot(rgb, w)  ?
forehead_k, forehead_w, forehead_s = ica.fastica(rgb[:,:3], fun = "exp", n_comp = 3, w_init = None, maxit = 40000, tol = 1e-14)
cheek_k, cheek_w, cheek_s = ica.fastica(rgb[:,3:], fun = "exp", n_comp = 3, w_init = forehead_w, maxit = 40000, tol = 1e-14)
logging.info("forehead w = %s" % str(forehead_w))
logging.info("cheek w = %s" % str(cheek_w))

output = sys.stdout
fmt = "%.16g" + " %.16g" * (forehead_s.shape[1] + cheek_s.shape[1])
for t, forehead_x, cheek_x in itertools.izip(t, forehead_s[:], cheek_s[:]):
	print >>output, fmt % ((t,) + tuple(forehead_x) + tuple(cheek_x))
