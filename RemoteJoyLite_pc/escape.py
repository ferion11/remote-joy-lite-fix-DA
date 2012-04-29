#!/usr/bin/env python
import sys

out = open("Spline36.cpp", "w")
out.write("const char* PIXEL_SHADER =\n")
for line in open("spline36.psh", "r"):
    out.write("\"" + line.strip() + "\\n\"\n")
out.write(";\n")
