#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <errno.h>


/*
 *	These syscalls do not have wrappers in libc so far, we directly
 *	invoke from syscall.
 */
#define SYS_GET_LOCAL_PORT 326
#define SYS_SET_LOCAL_PORT 327
#define SYS_GET_RESERVED_PORT 328
#define SYS_ADD_RESERVED_PORT 329
#define SYS_DEL_RESERVED_PORT 330
#define SYS_CLR_RESERVED_PORT 331


static int get_local_range(pid_t target, int *lo, int *hi)
{
	return syscall(SYS_GET_LOCAL_PORT, target, lo, hi);
}

static void set_local_range(pid_t target, int lo, int hi)
{
	int ret = syscall(SYS_SET_LOCAL_PORT, target, lo, hi);
	printf("set local port: %d %d %d %d\n", target, lo, hi, ret);
	if (ret < 0){
		exit(-1);
	}
}

static int* get_reserved_port(pid_t target, int *n)
{
	int size = *n;
	int *buffer = (int*) malloc(size * 2 * sizeof(int));
	int ret = syscall(SYS_GET_RESERVED_PORT, target, buffer, size);
	if (ret < 0) {
		return NULL;
	}
	*n = ret;
	return buffer;
}

static int add_reserved_port(pid_t target, int lo, int hi)
{
	int ret = syscall(SYS_ADD_RESERVED_PORT, target, lo, hi);
	printf("add rport: %d %d %d %d\n", target, lo, hi, ret);
	if (ret < 0){
		exit(-1);
	}
	return 0;
}

static int del_reserved_port(pid_t target, int lo, int hi)
{
	int ret = syscall(SYS_DEL_RESERVED_PORT, target, lo, hi);
	printf("del rport: %d %d %d %d\n", target, lo, hi, ret);
	if (ret < 0){
		exit(-1);
	}
	return 0;
}

static int clear_reserved_port(pid_t target)
{
	int ret = syscall(SYS_CLR_RESERVED_PORT, target);
	printf("del rport: %d %d\n", target, ret);
	if (ret < 0){
		exit(-1);
	}
	return 0;
}

static struct addrinfo *get_dst(const char* service, const char *port)
{
	struct addrinfo hint, *res = NULL;
	int ret = 0;
	hint.ai_family = AF_INET;
	hint.ai_protocol = 0;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = 0;
	
	ret = getaddrinfo(service, port, &hint, &res);
	if (ret != 0) {
		const char *err = gai_strerror(ret);
		fprintf(stderr, "error in getting addr of %s:%s, %s\n",
				service, port, err);
		exit(1);
	}

	return res;
}

static int portnum = 0;

static int do_connection()
{
	static char* allports[] = {"80", "5000", "35357", "8080"};
	int s = socket(AF_INET, SOCK_STREAM, 0);
	assert(s > 0);
	int set = 1;
	int ret = setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &set, sizeof(set));
	if (errno)
		perror("setsocketopt");
	assert(ret >= 0);
	struct addrinfo *res = get_dst("10.10.1.1", allports[portnum%4]);
	portnum ++;
	assert(res);
	ret = connect(s, res->ai_addr, res->ai_addrlen);
	if (errno)
		perror("connect");
	assert(ret >= 0);
	freeaddrinfo(res);

	struct sockaddr_in addr;
	socklen_t len=sizeof(addr);
	
	ret = getsockname(s, (struct sockaddr*) &addr, &len);
	if (errno)
		perror("getsockname");
	close(s);
	return ntohs(addr.sin_port);
}


static void expect(int actual, int ref)
{

}

static void waitchild(pid_t p, const char* cname)
{
	int status;
	do {
		int w = waitpid(p, &status, WUNTRACED | WCONTINUED);
		if (w == -1) {
			perror("waitpid");
			exit(EXIT_FAILURE);
		}

		if (WIFEXITED(status)) {
			int ret = WEXITSTATUS(status);
			if (ret != 0) {
				printf("child %s exit: %d\n", cname, ret);
				exit(1);
			}
		} else if (WIFSIGNALED(status)) {
			printf("child %s signaled\n", cname);
			exit(1);
		}
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
}


void test_local_ports()
{
	printf("test local ports\n");
	pid_t child;

	////test can get local ports
	int lo, hi;
	int ret = get_local_range(getpid(), &lo, &hi);
	printf("ret is %d %d %d\n", ret, lo, hi);
	assert(ret >= 0);
	assert(lo == 0);
	assert(hi == 65535);

	child = fork();
	if (child == 0) {
		/// child, check connect using local ports
		int p = do_connection();
		ret = get_local_range(getpid(), &lo, &hi);
		printf("client used port %d, getlocal: %d, range %d %d\n", p, ret, lo, hi);
		if (ret < 0 || p < 0)
			exit(1);
		exit(0);
	} else {
		///parent waitpid
		printf("child %d %d\n", child, __LINE__);
		waitchild(child, "default");
	}
	////test can set local ports
	lo = 42000;
	hi = 45000;
	set_local_range(getpid(), lo, hi);
	ret = get_local_range(getpid(), &lo, &hi);
	assert(ret >= 0);
	assert(lo == 42000);
	assert(hi == 45000);

	child = fork();
	if (child == 0) {
		/// child, check connect using local ports
		int p = do_connection();
		ret = get_local_range(getpid(), &lo, &hi);
		printf("client used port %d, getlocal: %d, range %d %d\n", p, ret, lo, hi);
		if (ret < 0 || p < 0)
			exit(2);
		lo = 42500;
		hi = 43000;
		set_local_range(getpid(), lo, hi);
		ret = get_local_range(getpid(), &lo, &hi);
		p = do_connection();
		printf("client used port %d, range %d %d %d\n", p, lo, hi, ret);
		if (p < lo || p > hi)
			exit(3);
		exit(0);
	} else {
		///parent waitpid
		printf("child %d %d\n", child, __LINE__);
		waitchild(child, "set child");
	}
	
	child = fork();
	if (child == 0) {
		/// child, check connect using local ports
		sleep(2);
		int p = do_connection();
		ret = get_local_range(getpid(), &lo, &hi);
		printf("client used port %d, getlocal: %d, range %d %d\n", p, ret, lo, hi);
		if (ret < 0 || p < 43000 || p > 44000)
			exit(4);
		exit(0);
	} else {
		///parent waitpid
		printf("child %d %d\n", child, __LINE__);
		set_local_range(child, 43000, 44000);
		waitchild(child, "sleep child");
	}
}

static int inlow(int *port, int n, int expect)
{
	int i;
	for (i = 0;i < n; i+=2) {
		if (expect == port[i])
			return 1;
	}
	printf("%d not found\n", expect);
	assert(0);
	return 0;
}

static int inhigh(int *port, int n, int expect)
{
	int i;
	for (i = 1;i < n; i+=2) {
		if (expect == port[i])
			return 1;
	}
	printf("%d not found\n", expect);
	assert(0);
	return 0;
}

void test_basic_reserved_ports()
{
	int n = 10;
	int *res = get_reserved_port(getpid(), &n);
	assert(n == 0);
	assert(res != NULL);
	free(res);

	add_reserved_port(getpid(), 40100, 41000);
	n=10;
	res = get_reserved_port(getpid(), &n);
	assert(n == 2);
	assert(res != NULL);
	assert(res[0] == 40100);
	assert(res[1] == 41000);
	free(res);

	add_reserved_port(getpid(), 40100, 41000);
	n=10;
	res = get_reserved_port(getpid(), &n);
	assert(n == 2);
	assert(res != NULL);
	assert(res[0] == 40100);
	assert(res[1] == 41000);
	free(res);

	add_reserved_port(getpid(), 41100, 41200);

	n=10;
	res = get_reserved_port(getpid(), &n);
	printf("result count %d\n", n);
	assert(n == 4);
	assert(res != NULL);
	inlow(res, n, 40100);
	inlow(res, n, 41100);
	inhigh(res, n, 41000);
	inhigh(res, n, 41200);
	free(res);
	add_reserved_port(getpid(), 41300, 41400);
	n=10;
	res = get_reserved_port(getpid(), &n);
	assert(n == 6);
	assert(res != NULL);
	inlow(res, n, 40100);
	inlow(res, n, 41100);
	inhigh(res, n, 41000);
	inhigh(res, n, 41200);
	inlow(res, n, 41300);
	inhigh(res, n, 41400);
	free(res);


	del_reserved_port(getpid(), 41100, 41200);
	n=10;
	res = get_reserved_port(getpid(), &n);
	assert(n == 4);
	assert(res != NULL);
	inlow(res, n, 40100);
	inhigh(res, n, 41000);
	inlow(res, n, 41300);
	inhigh(res, n, 41400);
	free(res);

	del_reserved_port(getpid(), 40100, 41000);
	n=10;
	res = get_reserved_port(getpid(), &n);
	assert(n == 2);
	assert(res != NULL);
	inlow(res, n, 41300);
	inhigh(res, n, 41400);
	free(res);

	del_reserved_port(getpid(), 41300, 41400);
}

void test_reserved_ports()
{
	printf("test reserve ports\n");
	pid_t child;

	test_basic_reserved_ports();
	printf("finish basic reserve test\n");

	set_local_range(getpid(), 41000, 42000);
	add_reserved_port(getpid(), 41000, 41100);
	add_reserved_port(getpid(), 41200, 41300);
	add_reserved_port(getpid(), 41400, 41500);

	int i;
	for (i = 0; i < 500; i++) {
		int p = do_connection();
		assert(!(p >= 41000 && p <= 41100) || !( p >=41200 && p <=41300) || !(p>=41400 && p <=41500));
	}

	// set reserved ports to 1000-1100, 1200-1300, 1400-1500
	// check connect not using these port range, with 500 random connections to my local apache server
	child = fork();
	if (child == 0) {
		// child, check get reserved ports is 1000-1100,1200-1300,1400-1500
		// check connect not using these port range, with 500 random connections to my local apache server
		sleep(2);
		int n = 10;
		int *res = get_reserved_port(getpid(), &n);
		assert(n == 2);
		assert(res != NULL);
		inlow(res, n, 41000);
		inhigh(res, n, 41020);
		free(res);
		for (i = 0; i < 80; i++) {
			int p = do_connection();
			if ((p >= 41000 && p <= 41020)) {
				printf("reserve child port %d\n", p);
				exit(6);
			}
		}
		exit(0);
	} else {
		///parent waitpid
		printf("child %d %d\n", child, __LINE__);
		clear_reserved_port(child);
		add_reserved_port(child, 41000, 41020);
		waitchild(child, "reserve copy child");
	}
	
	child = fork();
	if (child == 0) {
		// set local ports 1000-1100
		// child, check not able to connect
		// clear reserved ports and set to 1010-1020 1030-1040 1050-1060
		// check connect not using these port ranges with 100 random connections to my local apache server
		set_local_range(getpid(), 41000, 41100);
		clear_reserved_port(getpid());
		add_reserved_port(getpid(), 41000, 41020);
		for (i = 0; i < 80; i++) {
			int p = do_connection();
			if ((p >= 41000 && p <= 41020)) {
				printf("reserve child port1 %d\n", p);
				exit(7);
			}
			if ( p < 41000 || p > 41100) {
				printf("reserve child port2 %d\n", p);
				exit(8);
			}
		}
		exit(0);
	} else {
		///parent waitpid
		waitchild(child, "reserve self child");
	}

}

int main()
{
	printf("parent %d\n", getpid());
	test_local_ports();
	test_reserved_ports();
	return 0;
}


