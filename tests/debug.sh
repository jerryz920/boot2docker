CFLAGS="-g -O0" make syscall
docker-machine scp syscall v1:
docker-machine ssh v1
