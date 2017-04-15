
from debian:jessie


RUN apt-get update && apt-get install -y build-essential
RUN apt-get install -y gdb

COPY syscall.c /opt/
RUN gcc -g /opt/syscall.c -o /usr/bin/runtest

ENTRYPOINT ["/bin/sh", "-c"]
