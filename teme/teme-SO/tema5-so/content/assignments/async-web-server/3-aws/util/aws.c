#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "aws.h"
#include "debug.h"
#include "util.h"
#include "sock_util.h"
#include "w_epoll.h"
#include "http-parser/http_parser.h"

static int listenfd;
static int epollfd;

static http_parser parser;

static char *request_path;

int request_path_cb (http_parser *p, const char *buf, size_t len)
{
    assert(p == &parser);
    memcpy(request_path, buf, len);
    dlog(LOG_DEBUG, "Path read in header is %s\n", buf);

    return 0;
}

static http_parser_settings parser_settings =
    {.on_message_begin = 0
    ,.on_header_field = 0
    ,.on_header_value = 0
    ,.on_path = request_path_cb
    ,.on_url = 0
    ,.on_fragment = 0
    ,.on_query_string = 0
    ,.on_body = 0
    ,.on_headers_complete = 0
    ,.on_message_complete = 0
};

static void accept_connection(int listenfd)
{
	int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;

	/* accept new connection */
	sockfd = accept(listenfd, (SSA *) &addr, &addrlen);
	DIE(sockfd < 0, "accept");

	dlog(LOG_INFO, "Accepted connection from %s:%d\n",
		inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    connection_t *con = calloc(1, sizeof(connection_t));
    DIE(!con, "con calloc");

    con->sockfd = sockfd;
    con->buffer = malloc(BUFSIZ);
    DIE(!con->buffer, "buffer malloc");
    con->path = malloc(BUFSIZ);
    DIE(!con->path, "path malloc");
    con->message = malloc(BUFSIZ);
    DIE(!con->message, "message malloc");

    w_epoll_add_ptr_in(epollfd, sockfd, con);
}

static void receive_request(connection_t *con)
{
	char abuffer[64];
	int rc;

	rc = get_peer_address(con->sockfd, abuffer, 64);
	if (rc < 0) {
		ERR("get_peer_address");
		goto remove_connection;
	}

    memset(con->buffer, 0, BUFSIZ);
    con->buffer_len = recv(con->sockfd, con->buffer, BUFSIZ, 0);
	if (con->buffer_len < 0) {		/* error in communication */
		dlog(LOG_ERR, "Error in communication from %s\n", abuffer);
		goto remove_connection;
	}
	if (con->buffer_len == 0) {		/* connection closed */
		dlog(LOG_INFO, "Connection closed from %s\n", abuffer);
		goto remove_connection;
	}

    dlog(LOG_INFO, "Received message from %s\n", abuffer);
    dlog(LOG_INFO, "Received %s\n", con->buffer);

	printf("--\n%s--\n", con->buffer);

    memset(request_path, 0, BUFSIZ);
    http_parser_init(&parser, HTTP_REQUEST);
    http_parser_execute(&parser, &parser_settings, con->buffer, con->buffer_len);

    memset(con->path, 0, BUFSIZ);
    sprintf(con->path, AWS_DOCUMENT_ROOT "%s", request_path + 1);
    dlog(LOG_INFO, "Connection path is %s\n", con->path);

    con->filefd = open(con->path, O_RDWR);

    memset(con->message, 0, BUFSIZ);
    if (con->filefd == -1) {
        strcat(con->message, "HTTP/1.0 404 Not Found\r\n\r\n");
        con->message_len = strlen(con->message);
        dlog(LOG_ERR, "Bad file descriptor\n");
    } else {
        strcat(con->message, "HTTP/1.0 200 OK\r\n\r\n");
        con->message_len = strlen(con->message);
    }

    dlog(LOG_INFO, "Read from path %s the message %s\n", request_path, con->buffer);

    rc = w_epoll_update_ptr_inout(epollfd, con->sockfd, con);
    DIE(rc < 0, "w_epoll_update_ptr_inout");

	return;

remove_connection:
	/* close local socket */
	close(con->sockfd);


// GET /static/small00.dat HTTP/1.0
}

void send_status(connection_t *con)
{
    ssize_t bytes_sent;
	char abuffer[64];
	int rc;

    rc = get_peer_address(con->sockfd, abuffer, 64);
	if (rc < 0) {
		ERR("get_peer_address");
		goto remove_connection;
	}

	bytes_sent = send(con->sockfd, con->message, strlen(con->message), 0);
	if (bytes_sent < 0) {		/* error in communication */
		dlog(LOG_ERR, "Error in communication to %s\n", abuffer);
		goto remove_connection;
	}
	if (bytes_sent == 0) {		/* connection closed */
		dlog(LOG_INFO, "Connection closed to %s\n", abuffer);
		goto remove_connection;
	}

	dlog(LOG_INFO, "Sent message to %s (bytes: %ld)\n", abuffer,
		bytes_sent);

	printf("--\n%s\n--\n", con->message);
    sleep(1);

    if (con->filefd != -1) {
        struct stat file_stats;
        fstat(con->filefd, &file_stats);

        if (true) {
            int total_sent = 0;
            
            while (total_sent < file_stats.st_size) {
                rc = sendfile(con->sockfd, con->filefd, NULL, file_stats.st_size);
				DIE(rc == -1, "sendfile");
				
                total_sent += rc;
                bytes_sent += rc;
				
            }
            
        }
    }

    dlog(LOG_INFO, "Sent file to %s (total bytes: %ld)\n", abuffer,
		bytes_sent);

    rc = w_epoll_update_ptr_in(epollfd, con->sockfd, con);
	DIE(rc < 0, "w_epoll_update_ptr_in");

	/* Close HTTP connection. */
	close(con->sockfd);
	return;

remove_connection:
    rc = w_epoll_remove_ptr(epollfd, con->sockfd, con);
	DIE(rc < 0, "w_epoll_remove_ptr");
	/* close local socket */
	close(con->sockfd);
}

int main() {
    request_path = malloc(BUFSIZ);
    DIE(!request_path, "path malloc");

    epollfd = w_epoll_create();
    DIE(epollfd < 0, "create epoll");

    listenfd = tcp_create_listener(AWS_LISTEN_PORT, DEFAULT_LISTEN_BACKLOG);
    DIE(listenfd < 0, "create listener");

    DIE(w_epoll_add_fd_in(epollfd, listenfd) < 0, "add epoll fd");

    dlog(LOG_INFO, "Started listening on port %d\n", AWS_LISTEN_PORT);

    dlog(LOG_INFO, "Current fd of listener is %d\n", listenfd);

    while (true) {
        struct epoll_event event;

        w_epoll_wait_infinite(epollfd, &event);
        
        dlog(LOG_DEBUG, "Found something on fd %d\n", event.data.fd);

        if (event.data.fd == listenfd) {
            accept_connection(listenfd);
        } else {
            if (event.events & EPOLLOUT) {
                dlog(LOG_DEBUG, "Ready to send message\n");
                send_status(event.data.ptr);
            }

            if (event.events & EPOLLIN) {
                dlog(LOG_DEBUG, "New message\n");
                receive_request(event.data.ptr);
            }
        }
    }

    return 0;
}