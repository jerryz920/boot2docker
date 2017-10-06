#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>


#define FAIL(msg) fprintf(stderr, "error at line %d: %s\n", __LINE__, (msg))
#define SUSPEND(msg) {fprintf(stderr, "error at line %d: %s\n", __LINE__, (msg)); exit(1);}
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
#define SYS_ALLOC_LOCAL_PORT 332

const char* local_addr = "127.0.0.1";
int server_port = 10014;

int wait_principal(pid_t child) {
  int status;
  int ret = 0;
  if (waitpid(child, &status, 0) < 0) {
    perror("waitpid");
    ret = -1;
  }
  if (WIFEXITED(status)) {
    printf("%d exit!\n", child);
    ret = WEXITSTATUS(status);
  } else {
    ret = -2;
  }
  //::delete_principal(child);
  return ret;
}

int create_server_socket(int local_port) {

  int s = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(local_port),
    .sin_addr = {INADDR_ANY},
  };

  int val = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
    perror("setsockopt");
  }
  int flag = fcntl(s, F_GETFL, 0);
  if (fcntl(s, F_SETFL,  flag | O_NONBLOCK) < 0) {
    perror("fcntl");
  }

  if (bind(s, (struct sockaddr*) (&addr), sizeof(addr)) < 0) {
    perror("bind");
    SUSPEND("fail to allocate socket");
  }

  if (listen(s, 100) < 0) {
    perror("listen");
    SUSPEND("fail to listen on port");
  }

  return s;
}

int client_try_access(int local_port) {

  int c = socket(AF_INET, SOCK_STREAM, 0);
  const char* myip = "127.0.0.1";
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(local_port),
    .sin_addr = {inet_addr(myip)},
  };

  struct sockaddr_in my_addr;
  socklen_t my_addr_len = sizeof(my_addr);
  if (connect(c, (struct sockaddr*) (&addr), sizeof(addr)) < 0) {
    /// This way we only quit the client process, server needs
    // to capture the status
    perror("connect");
    SUSPEND("fail to connect to server ");
  }
  getsockname(c, (struct sockaddr*) &my_addr, &my_addr_len);
  fprintf(stderr, "client socket seen port: %d\n", ntohs(my_addr.sin_port));
  close(c);
  return 0;
}

static int wait_for_port_range(int lo, int hi)
{
  int s = create_server_socket(server_port);

  int c;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
again:
  c = accept(s, (struct sockaddr*)&client_addr, &client_addr_len);
  if (c < 0 && errno == EAGAIN) {
    usleep(100000);
    goto again;
  }

  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  int client_port = ntohs(client_addr.sin_port);
  close(s);
  close(c);
  if (client_port >= lo && client_port <= hi) {
    return 1;
  }
  return 0;
}


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
        fprintf(stderr, "getting %d reserved ports\n", ret);
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

static int alloc_local_ports(pid_t p, int n) {
        return syscall(SYS_ALLOC_LOCAL_PORT, 0, p, n);
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
	static const char* allports[] = {"80", "5000", "35357", "8080"};
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
	
	getsockname(s, (struct sockaddr*) &addr, &len);
	if (errno)
		perror("getsockname");
	close(s);
	return ntohs(addr.sin_port);
}

static int pending_socket()
{
	static const char* port = "80";
	int s = socket(AF_INET, SOCK_STREAM, 0);
	assert(s > 0);
	int set = 1;
	int ret = setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &set, sizeof(set));
	if (errno)
		perror("setsocketopt");
	assert(ret >= 0);
	struct addrinfo *res = get_dst("10.10.1.1", port);
	portnum ++;
	assert(res);
	ret = connect(s, res->ai_addr, res->ai_addrlen);
	if (errno)
		perror("connect");
	assert(ret >= 0);
	freeaddrinfo(res);
        return s;
}

static int get_port(int s) 
{
	struct sockaddr_in addr;
	socklen_t len=sizeof(addr);
	
	getsockname(s, (struct sockaddr*) &addr, &len);
	if (errno)
		perror("getsockname");
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
	int ret = add_reserved_port(getpid(), 41000, 41100);
        fprintf(stderr, "add result %d\n", ret);
	ret = add_reserved_port(getpid(), 41200, 41300);
        fprintf(stderr, "add result %d\n", ret);
	ret = add_reserved_port(getpid(), 41400, 41500);
        fprintf(stderr, "add result %d\n", ret);

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

void test_allocate_ports()
{
	printf("test allocate ports\n");
	pid_t child;
        clear_reserved_port(0);
	set_local_range(getpid(), 41000, 42000);
        add_reserved_port(0, 41000, 41040);
            int m = 10;
            int *res1 = get_reserved_port(0, &m);
            fprintf(stderr, "reserve port count %d\n", m);
            free(res1);

        int s1 = pending_socket();
        int s2 = pending_socket();
        int s3 = pending_socket();
        int s4 = pending_socket();
        int p1 = get_port(s1);
        int p2 = get_port(s2);
        int p3 = get_port(s3);
        int p4 = get_port(s4);
        fprintf(stderr, "!!!!!!!!!!!!!!pending ports %d %d %d %d\n", p1, p2, p3, p4);

        child = fork();
        if (child == 0) {
            sleep(1);
            client_try_access(server_port);
            exit(0);
        } else {
            int base = alloc_local_ports(child, 20);
            int n = 10;
            int *res = get_reserved_port(0, &n);
            fprintf(stderr, "reserve port count %d\n", n);
            free(res);
            printf("get port range from %d to %d for child %d\n",  base, base  + 20, child);
            wait_for_port_range(base, base+19);
            if (base <= 41040) {
                fprintf(stderr, "the range has been reserved, should not use it!\n");
            }
            if ((p1 >= base && p1 < base + 20) ||
                (p2 >= base && p2 < base + 20) ||
                (p3 >= base && p3 < base + 20) ||
                (p4 >= base && p4 < base + 20)) {
                fprintf(stderr, "used ports, should not occur!\n");
            }
            if (wait_principal(child) != 0) {
                fprintf(stderr, "something wrong with port allocating\n");
            }
            n = 10;
            res = get_reserved_port(0, &n);
            int i;
            for (i = 0; i < n; i += 2) {
              fprintf(stderr, "!!!reserved: %d %d\n", res[i], res[i+1]);
            }
            inlow(res, n, base);
            inhigh(res, n, base + 20 - 1);
            del_reserved_port(0, base, base + 19);
        }
        close(s1);
        close(s2);
        close(s3);
        close(s4);

}

int main()
{
        time_t s = time(0);
	printf("parent %d %lu\n", getpid(), s);
	test_local_ports();
        sleep(1);
	test_reserved_ports();
        sleep(1);
        test_allocate_ports();
	return 0;
}


