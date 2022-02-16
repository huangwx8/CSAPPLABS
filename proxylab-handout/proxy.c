#include "csapp.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define CACHE_LIST_SIZE 12

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/*
 * Function prototypes
 */
void init_cachelist();
int get_fileindex(char* filename);
int lookup(char* filename, char* msg, int* size);
int insert(char* filename, char* msg, int size);

int parse_uri(char *uri, char *target_addr, char *path, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int read_headers(rio_t *, char*);

int accept_client(int listenfd);
int connect_server(char* hostname, char* port);
void send_client(int connfd, char* msg, int size);
void send_server(int clientfd, char* hostname, char* pathname);
int receive_client(int connfd, char* hostname, char* pathname, char* port);
int receive_server(int clientfd, char* msg);

void always_listen(char* port);
void do_work(int connfd);

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
	/* Check arguments */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>", argv[0]);
		exit(0);
	}

	printf("%s\n", user_agent_hdr);

	// alloc shared memory as cache
	init_cachelist();

	// infinite loop listening, dispatch a worker process when new client ask a GET
	always_listen(argv[1]);
	
	return 0;
}

// LISTEN LOOP BEGIN

void always_listen(char* port) {
	/* Internet connection file descriptor */
	int listenfd, connfd;
	/* Open a listen socket descriptor at given port */
	listenfd = Open_listenfd(port);
	while (1) {
		/* Build a connection with client end */
		connfd = accept_client(listenfd);
		/* Use multi-process serve several clients */
		if (Fork() == 0) { /* Child process serve client as an individual */
			/* Child should close its listening socket */
			Close(listenfd);
			/* Receive from client -> Wrap and send to server -> Receive from server -> Send it back to client */
			do_work(connfd);
		}
		Close(connfd); /* Parent closes its connected socket */
	}
}

// LISTEN LOOP END

// WORKER LOGIC BEGIN

void do_work(int connfd) {
	
	/* Local varaiables in stack */
	char hostname[MAXLINE >> 1], pathname[MAXLINE >> 1], port[MAXLINE >> 1], msg[MAX_OBJECT_SIZE];
	int clientfd;
	int size;

	/* Receive request */
	if (receive_client(connfd, hostname, pathname, port) == -1) {
		Close(connfd);
		exit(0);
	}

	/* Try to find pathname in cache */
	if (lookup(pathname, msg, &size) >= 0) {
		send_client(connfd, msg, size);
		Close(connfd);
		exit(0);
	}

	/* Connect server */
	clientfd = connect_server(hostname, port);

	/* Connecting server failed */
	if (clientfd < 0) {
		/* report an error message that server is closed */
		clienterror(connfd, "Server closed", "404", "Not Found", "Server refuses this connection request");
		Close(connfd);
		exit(0);
	}

	/* Start ask server for data */

	/* Forward request */
	send_server(clientfd, hostname, pathname);
	/* Receive response */
	size = receive_server(clientfd, msg);
	/* Forward response */
	send_client(connfd, msg, size);
	/* Save message into cache */
	insert(pathname, msg, size);

	/* Child closes all connections */
	Close(clientfd);
	Close(connfd);
	/* Child exits */
	exit(0);
}

// WORKER LOGIC END


// CACHE MECHANISM BEGIN

typedef struct
{
	char filename[128];
	char content[MAX_OBJECT_SIZE];
	int size;
} CacheObj;


typedef struct
{
	CacheObj list[CACHE_LIST_SIZE];
	int cindex;
	sem_t mutex;

} CacheList;


CacheList* cachelist;
int shmid;
key_t key;

void init_cachelist() {
	/* Make a key */
	key = ftok("../", 2017);
	/* Create shared memory */
	shmid = shmget(key, sizeof(CacheList), IPC_CREAT | 0666);
	cachelist = shmat(shmid, NULL, 0);
	/* Now each process will share this cachelist */

	/* Initialize cachelist data structure */
	for (int i = 0;i < CACHE_LIST_SIZE;i++) {
		cachelist->list[i].filename[0] = '\0';
	}
	cachelist->cindex = 0;
	Sem_init(&(cachelist->mutex), 0, 1);
}

/* Return index of filename in cachelist, if not exist, return -1 */
int get_fileindex(char* filename) {
	for (int i = 0;i < CACHE_LIST_SIZE;i++) {
		if (!strcmp(cachelist->list[i].filename, filename)) {
			return i;
		}
	}
	return -1;
}

/* Cache lookup */
int lookup(char* filename, char* msg, int* size) {
	P(&(cachelist->mutex));
	int i = get_fileindex(filename);
	if (i >= 0) {
		*size = cachelist->list[i].size;
		strncpy(msg, cachelist->list[i].content, *size);
	}
	V(&(cachelist->mutex));
	return i;
}

/* Cache write */
int insert(char* filename, char* msg, int size) {
	if (size > MAX_OBJECT_SIZE)
		return -1;
	P(&(cachelist->mutex));
	int i = get_fileindex(filename);
	if (i < 0) i = cachelist->cindex;
	cachelist->cindex = (cachelist->cindex + 1) % CACHE_LIST_SIZE;
	cachelist->list[i].size = size;
	strncpy(cachelist->list[i].content, msg, size);
	strncpy(cachelist->list[i].filename, filename, strlen(filename));
	V(&(cachelist->mutex));
	return i;
}

// CACHE MECHANISM END


// NET UTILS BEGIN

/*
 * Call accept, waiting for any connections from clients,
 * return connected file descriptor.
 */
int accept_client(int listenfd) {
	struct sockaddr_storage clientaddr;
	socklen_t clientlen = sizeof(clientaddr);
	return Accept(listenfd, (SA *)&clientaddr, &clientlen);
}

/*
 * Call connect, build tcp connection with target server {hostname:port},
 * return connected file descriptor.
 */
int connect_server(char* hostname, char* port) {
	/* Build a connection to target server */
	return Open_clientfd(hostname, port);
}

/*
 * Forward the response message back to client.
 * Especially, call Rio_writen(connfd, msg, strlen(msg)) will cause implict error,
 * because some '\0' can exist in response body of non-text http content type.
 */
void send_client(int connfd, char* msg, int size) {
	printf("Forward response message, length: %d bytes\n", size);
	Rio_writen(connfd, msg, size);
}

/*
 * Prepare message and send them to server
 *
 * the proxy should open a connection to ${hostname}:${server_port} and
 * send an HTTP request of its own starting with a line of the following form:
 * GET /${file} HTTP/1.0
 */
void send_server(int clientfd, char* hostname, char* pathname) {
	/* Data temporary buffer */
	char buf[MAXLINE];
	sprintf(buf, "GET %s HTTP/1.0\r\n", pathname);
	Rio_writen(clientfd, buf, strlen(buf));
	sprintf(buf, "Host: %s\r\n", hostname);
	Rio_writen(clientfd, buf, strlen(buf));
	strcpy(buf, user_agent_hdr);
	Rio_writen(clientfd, buf, strlen(buf));
	strcpy(buf, "Connection: close\r\n");
	Rio_writen(clientfd, buf, strlen(buf));
	strcpy(buf, "Proxy-Connection: close\r\n\r\n");
	Rio_writen(clientfd, buf, strlen(buf));
}

/*
 * Receive request from client, parse it.
 * return -1 if any bad things happened, else 0.
 */
int receive_client(int connfd, char* hostname, char* pathname, char* port) {
	/* Internet buffer */
	rio_t rio;
	rio_t* rp = &rio;
	/* Data temporary buffer */
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], request_header[MAXBUF];
	/* Initialize */
	Rio_readinitb(rp, connfd);
	/* read a line from socket buffer as request line */
	Rio_readlineb(rp, buf, MAXLINE);
	/* Varaible buf is like: GET http://${hostname}:${server_port}/${file} HTTP/1.1 */
	printf("Request line:\n%s", buf);
	sscanf(buf, "%s %s %s", method, uri, version);

	/* read serveral lines from socket buffer as request headers */
	if (read_headers(rp, request_header) < 0) {
		clienterror(connfd, request_header, "400", "Bad request", "Read Headers error");
		return -1;
	}
	printf("Request headers:\n%s", request_header);

	/* Protocals except HTTP are not implemented */
	if (parse_uri(uri, hostname, pathname, port) < 0) {
		clienterror(connfd, uri, "400", "Bad request", "Proxy dont support this Protocol");
		return -1;
	}

	/* Methods except GET are not implemented */
	if (strcmp(method, "GET")) {
		clienterror(connfd, method, "501", "Not implemented", "Proxy dont support this Method");
		return -1;
	}
	return 0;
}


/*
 * Receive response from server, write all the things into a outer memory "rsp_msg"
 * return size of message.
 */
int receive_server(int clientfd, char* msg) {
	/* Internet buffer */
	rio_t rio;
	rio_t* rp = &rio;
	/* Data temporary buffer */
	char buf[MAXLINE];
	/* Initialize */
	Rio_readinitb(rp, clientfd);
	/* Receive response line */
	Rio_readlineb(rp, buf, MAXLINE);
	printf("Response line:\n%s", buf);
	strcpy(msg, buf);
	/* Receive response headers */
	read_headers(rp, buf);
	printf("Response headers:\n%s", buf);
	strcpy(msg + strlen(msg), buf);
	/* Get "Content-length:" item */
	char* p = strstr(buf, "Content-length: ");
	int content_len = p ? atoi(p + 16) : 0;
	/* Receive response body */
	int size = strlen(msg) + content_len;
	Rio_readnb(rp, msg + strlen(msg), content_len);
	*(msg + size) = '\0';
	return size;
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;

	/* Check if "http://" is in uri, or this uri is not a http request. */
	if (strncasecmp(uri, "http://", 7)) {
		hostname[0] = '\0';
		return -1;
	}

	/* Extract the host name */
	hostbegin = uri + 7;
	hostend = strpbrk(hostbegin, " :/\r\n\0");
	len = hostend - hostbegin;
	strncpy(hostname, hostbegin, len);
	hostname[len] = '\0';

	/* Extract the port number */
	strncpy(port, "80\0", 3); /* default */
	if (*hostend == ':') {
		int n_port = atoi(hostend + 1);
		sprintf(port, "%d", n_port);
	}
	/* Extract the path */
	pathbegin = strchr(hostbegin, '/');
	if (pathbegin == NULL) {
		pathname[0] = '\0';
	}
	else {
		strcpy(pathname, pathbegin);
	}

	return 0;
}

void clienterror(int fd, char *cause, char *errnum,
	char *shortmsg, char *longmsg)
{
	char buf[MAXLINE], body[MAXBUF];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body + strlen(body), "<body bgcolor=""ffffff"">\r\n");
	sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);
	sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);
	sprintf(body + strlen(body), "<hr><em>The Tiny Proxy</em>\r\n");

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}

/*
 * Except header lines wont be more than MAXLINE chars,
 * therefore readin iteratively, until meet "\r\n" or EOF
 */
int read_headers(rio_t *rp, char* headers) {
	ssize_t rc;
	char buf[MAXBUF];
	buf[0] = '\0';
	headers[0] = '\0';

	/* Stop reading until meet "\r\n" or EOF */
	while (strcmp(buf, "\r\n")) {
		rc = Rio_readlineb(rp, buf, MAXBUF);
		if (rc == 0) break;
		else if (rc < 0) return -1;
		strcpy(headers + strlen(headers), buf);
	}
	return 0;
}

// NET UTILS END
