#!/usr/bin/env python
import sys

for file_title in ["spline36", "lanczos4"]:
	out = open(file_title + ".cpp", "w")
	out.write("const char* PIXEL_SHADER_%s =\n" % file_title.upper())
	for line in open(file_title + ".psh", "r"):
		out.write("\"" + line.strip() + "\\n\"\n")
	out.write(";\n")
