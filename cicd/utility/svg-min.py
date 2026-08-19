#!/usr/bin/env python3
"""Shrink an SVG for bundling: drop editor bookkeeping, keep the drawing.

Themes are drawn in Inkscape, and what ships is mostly not the drawing - it is
sodipodi/inkscape namespaces, RDF metadata blocks, and the indentation between
elements. Removing only those is safe for every renderer, and typically halves
the file. Nothing that affects rendering is touched: styles, classes, gradient
ids and xlink references all survive verbatim, which is what keeps GTK's
symbolic recolouring working.

Syntax: svg-min.py <in.svg> <out.svg>
        svg-min.py --tree <in-dir> <out-dir>
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

SVG_NS = "http://www.w3.org/2000/svg"
XLINK_NS = "http://www.w3.org/1999/xlink"

# Editor bookkeeping. Everything under these namespaces is inert at render time.
#
# Matched as substrings, not exact URIs: themes carry several spellings of the
# same namespace, including a "sodipodi-0.dtd" that is missing a digit. An exact
# list let that one through, which left <ns1:namedview> in the output - harmless
# where the file keeps its own xmlns declaration, but it survives as dead weight
# and breaks anything that inlines the body into another document.
DROP_NS = (
	"sodipodi.sourceforge.net",
	"inkscape.org/namespaces",
	"ns.adobe.com/AdobeIllustrator",
	"22-rdf-syntax-ns#",
	"purl.org/dc/elements",
	"creativecommons.org/ns#",
	"openswatchbook.org/uri",
)

DROP_TAGS = ("metadata", "title", "desc", "foreignObject")

ET.register_namespace("", SVG_NS)
ET.register_namespace("xlink", XLINK_NS)


def _in_dropped_ns(name):
	if not name.startswith("{"):
		return False
	uri = name[1:name.index("}")] if "}" in name else ""
	return any(fragment in uri for fragment in DROP_NS)


def _clean(element):
	for child in list(element):
		tag = child.tag
		if not isinstance(tag, str):		# comment or processing instruction
			element.remove(child)
			continue
		if _in_dropped_ns(tag) or tag.split("}")[-1] in DROP_TAGS:
			element.remove(child)
			continue
		_clean(child)

	for key in list(element.attrib):
		if _in_dropped_ns(key):
			del element.attrib[key]

	# Indentation only - a text node that is all whitespace draws nothing.
	if element.text is not None and element.text.strip() == "":
		element.text = None
	if element.tail is not None and element.tail.strip() == "":
		element.tail = None


def minify(src, dst):
	tree = ET.parse(src)
	root = tree.getroot()
	_clean(root)

	out = ET.tostring(root, encoding="unicode")
	# ET emits the default namespace on every child it cannot attribute; collapse
	# the redundant redeclarations it leaves behind.
	out = out.replace(' xmlns:ns0="%s"' % SVG_NS, "")
	out = re.sub(r"\s+", " ", out)
	out = out.replace("> <", "><").strip()

	with open(dst, "w", encoding="utf-8", newline="\n") as handle:
		handle.write(out)


def main(argv):
	if len(argv) == 4 and argv[1] == "--tree":
		src_root, dst_root = argv[2], argv[3]
		kept = 0
		for base, _dirs, files in os.walk(src_root):
			for name in files:
				src = os.path.join(base, name)
				dst = os.path.join(dst_root, os.path.relpath(src, src_root))
				os.makedirs(os.path.dirname(dst), exist_ok=True)
				# A few names exist only as bitmaps upstream (Adwaita's
				# emblems). Nothing to minify - pass them through.
				if not name.endswith(".svg"):
					with open(src, "rb") as fh:
						data = fh.read()
					with open(dst, "wb") as fh:
						fh.write(data)
					kept += 1
					continue
				try:
					minify(src, dst)
				except Exception:
					# An SVG we cannot parse still renders; ship it untouched.
					with open(src, "rb") as fh:
						data = fh.read()
					with open(dst, "wb") as fh:
						fh.write(data)
				kept += 1
		print(kept)
		return 0

	if len(argv) != 3:
		sys.stderr.write(__doc__)
		return 2

	minify(argv[1], argv[2])
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
