import bisect
import numpy
import ica

if True:
	rgb = numpy.loadtxt("rgb1.txt")
	t = rgb[:,0]
	rgb = rgb[:,1:]
	dt = (t[-1] - t[0]) / (len(t) - 1)

	# replace RGB time series with its 1st derivative to whiten the
	# background noise spectrum
	t = t[1:-1]
	rgb = (rgb[2:] - rgb[:-2]) / (2 * dt)

	# excise glitch
	kmin = bisect.bisect(t, 85.0)
	kmax = bisect.bisect(t, 86.0)
	rgb = numpy.delete(rgb, numpy.s_[kmin:kmax], 0)
	t = numpy.delete(t, numpy.s_[kmin:kmax], 0)

	k, w, s = ica.fastica(rgb, n_comp = 3, maxit = 40000, tol = 1e-14)
	print w

	output = open("ica1.txt", "w")
	for t, x in zip(t, s[:]):
		print >>output, ("%.16g " * 4) % ((t,) + tuple(x))


if True:
	rgb = numpy.loadtxt("rgb2.txt")
	t = rgb[:,0]
	rgb = rgb[:,1:]
	dt = (t[-1] - t[0]) / (len(t) - 1)

	# replace RGB time series with its 1st derivative to whiten the
	# background noise spectrum
	t = t[1:-1]
	rgb = (rgb[2:] - rgb[:-2]) / (2 * dt)

	k, w, s = ica.fastica(rgb, n_comp = 3, maxit = 40000, tol = 1e-14)
	print w

	output = open("ica2.txt", "w")
	for t, x in zip(t, s[:]):
		print >>output, ("%.16g " * 4) % ((t,) + tuple(x))
