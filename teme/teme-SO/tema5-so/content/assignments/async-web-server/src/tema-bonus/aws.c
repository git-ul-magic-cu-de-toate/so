// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>
#include <libaio.h>
#include <errno.h>

#include "aws.h"
#include "utils/util.h"
#include "utils/debug.h"
#include "utils/sock_util.h"
#include "utils/w_epoll.h"
#include "http-parser/http_parser.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor */
static int epollfd;

static io_context_t ctx;

static int aws_on_path_cb(http_parser *p, const char *buf, size_t len)
{
	struct connection *conn = (struct connection *)p->data;

	memcpy(conn->request_path, buf, len);
	conn->request_path[len] = '\0';
	conn->have_path = 1;

	return 0;
}

static void prepare_connection_send_reply_header(struct connection *conn)
{
	/* TODO: Prepare the connection buffer to send the reply header. */
	struct stat file_stat;
    int rc = fstat(conn->fd, &file_stat);
    off_t file_size = file_stat.st_size;

	snprintf(conn->send_buffer, sizeof(conn->send_buffer),
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n", file_size);

	conn->send_len = strlen(conn->send_buffer);
}

static void prepare_connection_send_404(struct connection *conn)
{
	/* TODO: Prepare the connection buffer to send the 404 header. */ 
    snprintf(conn->send_buffer, sizeof(conn->send_buffer),
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n"
             "\r\n");
	
	conn->send_len = strlen(conn->send_buffer);
}

static enum resource_type connection_get_resource_type(struct connection *conn)
{
	/*
	 * TODO: Get resource type depending on request path/filename. Filename should
	 * point to the static or dynamic folder.
	 */
	strcpy(conn->filename, conn->request_path);
	
	char *check_static = strstr(conn->filename, AWS_REL_STATIC_FOLDER);
	if (check_static != NULL)
		return RESOURCE_TYPE_STATIC;

	char *check_dynamic = strstr(conn->filename, AWS_REL_DYNAMIC_FOLDER);
	if (check_dynamic != NULL)
		return RESOURCE_TYPE_DYNAMIC;

	return RESOURCE_TYPE_NONE;
}


struct connection *connection_create(int sockfd)
{
	/* TODO: Initialize connection structure on given socket. */
	struct connection *conn = malloc(sizeof(*conn));
	DIE(conn == NULL, "malloc");

	conn->sockfd = sockfd;
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);
	memset(conn->filename, 0, BUFSIZ);
	memset(conn->request_path, 0, BUFSIZ);

	conn->fd = -1;
	conn->state = STATE_INITIAL;

	return conn;
}

void connection_start_async_io(struct connection *conn)
{
	/* TODO: Start asynchronous operation (read from file).
	 * Use io_submit(2) & friends for reading data asynchronously.
	 */
}


void connection_remove(struct connection *conn)
{
	int rc;
	rc = w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
	
	close(conn->sockfd);
	free(conn);
}


void handle_new_connection(void)
{
	/* TODO: Handle a new connection request on the server socket. */
	static int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct connection *conn;
	int rc;

	/* TODO a: Accept new connection. */
	sockfd = accept(listenfd, (SSA *) &addr, &addrlen);
	// DIE(sockfd < 0, "accept");

	/* TODO b: Set socket to be non-blocking. */
    int flags = fcntl(sockfd, F_GETFL, 0);
    // DIE(flags == -1, "Failed to get socket flags");

	flags |= O_NONBLOCK;
	rc = fcntl(sockfd, F_SETFL, flags);
    // DIE(rc == -1, "Failed to set socket to non-blocking");

	/* TODO c: Instantiate new connection handler. */
	conn = connection_create(sockfd);

	// /* TODO d: Add socket to epoll. */
	rc = w_epoll_add_ptr_in(epollfd, sockfd, conn);
	// DIE(rc < 0, "w_epoll_add_in");

	/* TODO e: Initialize HTTP_REQUEST parser. */
	http_parser_init(&conn->request_parser, HTTP_REQUEST);
	conn->request_parser.data = conn;
}


void receive_data(struct connection *conn)
{
	// printf("salutare\n");
	ssize_t bytes_recv;
	int rc;
	conn->state = STATE_RECEIVING_DATA;


	bytes_recv = recv(conn->sockfd, conn->recv_buffer, BUFSIZ, 0);
	if (bytes_recv <= 0) {
		conn->recv_len = bytes_recv;
		conn->state = STATE_CONNECTION_CLOSED;
		return;
	}

	printf("recvbuffer: %s\n", conn->recv_buffer);
	// ssize_t total_bytes = 0;
    // ssize_t bytes = 0;

    // while (conn->send_len > 0) {
    //     bytes = recv(conn->sockfd, buffer, conn->send_len, 0);
    //     if (bytes == -1)
    //         break;
    //     total_bytes += bytes;
    //     conn->send_len -= bytes;
    // }

	conn->recv_len = bytes_recv;
	conn->state = STATE_REQUEST_RECEIVED;
}

int connection_open_file(struct connection *conn)
{
	/* TODO: Open file and update connection fields. */
	int rc = open(conn->filename, O_RDONLY);

	conn->fd = rc;
	return rc;
}

void connection_complete_async_io(struct connection *conn)
{
	/* TODO: Complete asynchronous operation; operation returns successfully.
	 * Prepare socket for sending.
	 */
}

int parse_header(struct connection *conn)
{
	/* TODO: Parse the HTTP header and extract the file path. */

	/* Use mostly null settings except for on_path callback. */
	http_parser_settings settings_on_path = {
		.on_message_begin = 0,
		.on_header_field = 0,
		.on_header_value = 0,
		.on_path = aws_on_path_cb,
		.on_url = 0,
		.on_fragment = 0,
		.on_query_string = 0,
		.on_body = 0,
		.on_headers_complete = 0,
		.on_message_complete = 0
	};

	int size = http_parser_execute(&conn->request_parser, &settings_on_path, 
									conn->recv_buffer, conn->recv_len);

	strcpy(conn->filename, AWS_DOCUMENT_ROOT);
	strcat(conn->filename, conn->request_path + 1);

	return size;
}

int connection_send_data(struct connection *conn)
{
	/* May be used as a helper function.
	 * TODO: Send as much data as possible from the connection send buffer.
	 * Returns the number of bytes sent or -1 if an error occurred
	 */
	ssize_t total_bytes = 0;
    ssize_t bytes = 0;

    while (conn->send_len > 0) {
        bytes = send(conn->sockfd, conn->send_buffer + total_bytes, conn->send_len, 0);
        if (bytes == -1)
            break;
        total_bytes += bytes;
        conn->send_len -= bytes;
    }

    return total_bytes;
}

enum connection_state connection_send_static(struct connection *conn)
{
	conn->state = STATE_SENDING_HEADER;

	printf("%s\n", conn->filename);
	prepare_connection_send_reply_header(conn);

	connection_send_data(conn);

	conn->state = STATE_HEADER_SENT;

	struct stat file_stat;
    fstat(conn->fd, &file_stat);
    
	off_t file_size = file_stat.st_size;
	
	off_t offset = 0;
	ssize_t bytes_sent;

	conn->state = STATE_SENDING_DATA;

    while (offset < file_size) {
		printf("bytes are sending: %d\n", offset);
        bytes_sent = sendfile(conn->sockfd, conn->fd, &offset, file_size);
		if (bytes_sent == -1) {
			close(conn->fd);
			return STATE_CONNECTION_CLOSED;
		}
    }

	return STATE_DATA_SENT;
}


int connection_send_dynamic(struct connection *conn)
{
	/* TODO: Read data asynchronously.
	 * Returns 0 on success and -1 on error.
	 */

	return 0;
}


void handle_input(struct connection *conn)
{
	/* TODO: Handle input information: may be a new message or notification of
	 * completion of an asynchronous I/O operation.
	 */
	
	if (conn->state == STATE_INITIAL) {
		receive_data(conn);
		printf("a iesit din recv data\n");
	}

	if (conn->state == STATE_CONNECTION_CLOSED) {
		connection_remove(conn);
		return;
	}
	
	if (conn->state == STATE_REQUEST_RECEIVED) {
		printf("a intrat aici\n");
		parse_header(conn);
		
		printf("parsed header: %s\n", conn->request_path);
		// open connection
		int rc = connection_open_file(conn);
		rc = w_epoll_update_ptr_out(epollfd, conn->sockfd, conn);
		return;
	}
}

void handle_output(struct connection *conn)
{
	/* TODO: Handle output information: may be a new valid requests or notification of
	 * completion of an asynchronous I/O operation or invalid requests.
	 */
	if (conn->state == STATE_REQUEST_RECEIVED) {
		if (conn->fd < 0) {
			conn->state = STATE_SENDING_404;
			return;
		}
		printf("salutare\n");
		enum resource_type res = connection_get_resource_type(conn);
		if (res == RESOURCE_TYPE_STATIC) {
			conn->state = connection_send_static(conn);
			return;
		}
		if (res == RESOURCE_TYPE_DYNAMIC) {
			connection_send_dynamic(conn);
			connection_remove(conn);
			return;
		}
	}

	if (conn->state == STATE_SENDING_404) {
		prepare_connection_send_404(conn);
		connection_send_data(conn);
		conn->state = STATE_CONNECTION_CLOSED;
		return;
	}

	if (conn->state == STATE_CONNECTION_CLOSED) {
		connection_remove(conn);
		return;
	}

	if (conn->state == STATE_DATA_SENT) {
		close(conn->fd);
		connection_remove(conn);
		return;
	}
}

void handle_client(uint32_t event, struct connection *conn)
{
	/* TODO: Handle new client. There can be input and output connections.
	 * Take care of what happened at the end of a connection.
	 */
	if (event == EPOLLIN)
		handle_input(conn);
	else if (event == EPOLLOUT)
		handle_output(conn);
}

int main(void)
{
	int rc;

	/* TODO: Initialize asynchronous operations. */

	/* TODO: Initialize multiplexing. */
	epollfd = w_epoll_create();
	DIE(epollfd < 0, "w_epoll_create");

	/* TODO: Create server socket. */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT,
		10);
	DIE(listenfd < 0, "tcp_create_listener");

	/* TODO: Add server socket to epoll object*/
	rc = w_epoll_add_fd_in(epollfd, listenfd);
	DIE(rc < 0, "w_epoll_add_fd_in");

	/* Uncomment the following line for debugging. */
	// dlog(LOG_INFO, "Server waiting for connections on port %d\n", AWS_LISTEN_PORT);

	/* server main loop */
	while (1) {
		struct epoll_event rev;

		/* TODO: Wait for events. */
		rc = w_epoll_wait_infinite(epollfd, &rev);
		// DIE(rc < 0, "w_epoll_wait_infinite");

		/*
		 * TODO: Switch event types; consider
		 *   - new connection requests (on server socket)
		 *   - socket communication (on connection sockets)
		 */

		if (rev.data.fd == listenfd) {
			handle_new_connection();
			printf("new connection\n");
		} else {
			if (rev.events & EPOLLIN) {
				handle_client(EPOLLIN, rev.data.ptr);
			}
			if (rev.events & EPOLLOUT) {
				handle_client(EPOLLOUT, rev.data.ptr);
			}
		}
	}

	return 0;
}
