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
            subprocess.call(["sha1sum", os.path.join(d,f)])
        if stat.S_ISLNK(res.st_mode):
            real_name=name
            while stat.S_ISLNK(res.st_mode):
                real_name=os.readlink(real_name)
                res = os.lstat(real_name)
            print("%s %s" % (real_name, name))



