#!/usr/bin/env python3
"""Write an ld --dynamic-list naming every function and variable the static
extension library defines, so the exe exports that API and nothing else."""

import subprocess
import sys

nm, archive, out = sys.argv[1:4]
listing = subprocess.run([nm, "-g", "--defined-only", "-P", archive],
                         capture_output=True, text=True, check=True).stdout
names = set()
for line in listing.splitlines():
    fields = line.split()
    # Archive member headers end in ':' and carry no type field.
    if len(fields) >= 2 and fields[1] in "TDBR":
        names.add(fields[0])
if not names:
    sys.exit("dynamic-list.py: no symbols found in " + archive)

with open(out, "w") as f:
    f.write("{\n")
    for name in sorted(names):
        f.write("\t" + name + ";\n")
    f.write("};\n")
