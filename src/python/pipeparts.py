#
# =============================================================================
#
#                                   Preamble
#
# =============================================================================
#


import sys


from gi.repository import GObject
GObject.threads_init()
from gi.repository import Gst
Gst.init(None)


__author__ = "Kipp Cannon <kcannon@cita.utoronto.ca>"
__version__ = "FIXME"
__date__ = "FIXME"


#
# =============================================================================
#
#                            Pipeline Construction
#
# =============================================================================
#


def mkelem(pipeline, src, elem_type_name, **properties):
	elem = Gst.ElementFactory.make(elem_type_name, properties.pop("name", None))
	if elem is None:
		raise ValueError("failed to create element '%s'" % elem_type_name)
	for name, value in properties.items():
		elem.set_property(name.replace("_", "-"), value)
	pipeline.add(elem)
	if isinstance(src, Gst.Pad):
		src.get_parent_element().link_pads(src, elem, None)
	elif src is not None:
		src.link(elem)
	return elem


def write_dump_dot(pipeline, filestem, verbose = False):
	"""
	This function needs the environment variable GST_DEBUG_DUMP_DOT_DIR
	to be set.   The filename will be

	os.path.join($GST_DEBUG_DUMP_DOT_DIR, filestem + ".dot")

	If verbose is True, a message will be written to stderr.
	"""
	if "GST_DEBUG_DUMP_DOT_DIR" not in os.environ:
		raise ValueError("cannot write pipeline, environment variable GST_DEBUG_DUMP_DOT_DIR is not set")
	Gst.debug_bin_to_dot_file(pipeline, Gst.DebugGraphDetails.ALL, filestem)
	if verbose:
		print >>sys.stderr, "Wrote pipeline to %s" % os.path.join(os.environ["GST_DEBUG_DUMP_DOT_DIR"], "%s.dot" % filestem)


class src_deferred_link(object):
	def __init__(self, src, srcpadname, sinkpad):
		no_more_pads_handler_id = src.connect("no-more-pads", self.no_more_pads, srcpadname)
		src.connect("pad-added", self.pad_added, (srcpadname, sinkpad, no_more_pads_handler_id))

	@staticmethod
	def pad_added(element, pad, (srcpadname, sinkpad, no_more_pads_handler_id)):
		if pad.get_name() == srcpadname:
			pad.get_parent().handler_disconnect(no_more_pads_handler_id)
			pad.link(sinkpad)

	@staticmethod
	def no_more_pads(element, srcpadname):
		#write_dump_dot(element.get_parent(), "pipeline")
		raise ValueError("%s: no pad named '%s'" % (element.get_name(), srcpadname))
