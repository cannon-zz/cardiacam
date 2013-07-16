import bisect
import numpy
from cardiacam import ica

# rgb
wguess = numpy.array(
	[[ 0.05648833,  0.97166715,  0.22950386],
	 [ 0.50415065,  0.17065149, -0.84658738],
	 [ 0.86176632, -0.16352682,  0.48022681]]
)
# default
#wguess = None


def load_rgb(filename):
	rgb = numpy.loadtxt(filename)
	t = rgb[:,0]
	rgb = rgb[:,1:]
	return rgb, t


def differentiate(rgb, t):
	rgb = (rgb[2:] - rgb[:-2]).T / (t[2:] - t[:-2])
	return rgb.T, t[1:-1]

if True:
	# load rgb1.txt
	rgb, t = load_rgb("rgb1.txt")

	# replace RGB time series with its 1st derivative to whiten the
	# background noise spectrum
	rgb, t = differentiate(rgb, t)

	k, w, s = ica.fastica(rgb, fun = "exp", n_comp = 3, w_init = wguess, maxit = 40000, tol = 1e-14)
	print w

	output = open("ica1.txt", "w")
	for t, x in zip(t, s[:]):
		print >>output, ("%.16g " * 4) % ((t,) + tuple(x))


if True:
	# load rgb2.txt
	rgb, t = load_rgb("rgb2.txt")

	# replace RGB time series with its 1st derivative to whiten the
	# background noise spectrum
	rgb, t = differentiate(rgb, t)

	k, w, s = ica.fastica(rgb, fun = "exp", n_comp = 3, w_init = wguess, maxit = 40000, tol = 1e-14)
	print w

	output = open("ica2.txt", "w")
	for t, x in zip(t, s[:]):
		print >>output, ("%.16g " * 4) % ((t,) + tuple(x))
