#!/usr/bin/env python

import os
import sys
import subprocess

if len(sys.argv) > 1:
    target=sys.argv[1]
else:
    target='.'



for d, sd, fs in os.walk(target):
    # just calculate the file checksum
    for f in fs:
        subprocess.call(["sha1sum", os.path.join(d,f)])



