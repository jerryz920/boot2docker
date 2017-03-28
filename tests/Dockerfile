
from debian:jessie


RUN apt-get update && apt-get install -y build-essential

COPY syscall.c /opt/
RUN gcc -g /opt/syscall.c -o /usr/bin/runtest

ENTRYPOINT ["/usr/bin/runtest"]
