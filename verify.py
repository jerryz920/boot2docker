#!/usr/bin/env python

import os
import sys
import stat
import subprocess

if len(sys.argv) > 1:
    target=sys.argv[1]
else:
    target='.'



for d, sd, fs in os.walk(target):
    # just calculate the file checksum
    for f in fs:
        name=os.path.join(d, f)
        res = os.lstat(name)
        if stat.S_ISREG(res.st_mode):
            out=subprocess.check_output(["sha1sum", os.path.join(d,f)])
            data = out.split(' ')
            print("%s %s" % (data[0], f))
        elif stat.S_ISLNK(res.st_mode):
            real_name = os.readlink(name)
            print("%s %s" % (real_name, f))



