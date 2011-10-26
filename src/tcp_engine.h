/*
 * Spider -- An open source C language toolkit.
 *
 * Copyright (C) 2011 , Inc.
 *
 * lidp <openser@yeah.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*
  * \breif  tcp relate sockt obstract ,support for tcp server and client 
  */

#ifndef _SPIDER_TCP_ENGINE_H
#define _SPIDER_TCP_ENGINE_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "lock.h"
#include "utils.h"
#include "threadprivdata.h"
#include "socket.h"
#include "strings.h"


/*!
 * The following code implements a generic mechanism for starting
 * services on a TCP socket.
 * The service is configured in the struct session_args, and
 * then started by calling server_start(desc) on the descriptor.
 * server_start() first verifies if an instance of the service is active,
 * and in case shuts it down. Then, if the service must be started, creates
 * a socket and a thread in charge of doing the accept().
 *
 * The body of the thread is desc->accept_fn(desc), which the user can define
 * freely. We supply a sample implementation, server_root(), structured as an
 * infinite loop. At the beginning of each iteration it runs periodic_fn()
 * if defined (e.g. to perform some cleanup etc.) then issues a poll()
 * or equivalent with a timeout of 'poll_timeout' milliseconds, and if the
 * following accept() is successful it creates a thread in charge of
 * running the session, whose body is desc->worker_fn(). The argument of
 * worker_fn() is a struct spd_tcptls_session_instance, which contains the address
 * of the other party, a pointer to desc, the file descriptors (fd) on which
 * we can do a select/poll (but NOT I/O), and a FILE *on which we can do I/O.
 * We have both because we want to support plain and SSL sockets, and
 * going through a FILE * lets us provide the encryption/decryption
 * on the stream without using an auxiliary thread.
 */

/*! \brief
 * arguments for the accepting thread
*/

struct spd_tcp_session_arg {
	struct spd_sockaddr serveraddr; /* us address */
	struct spd_sockaddr peeraddr;   /* peer address */
	struct spd_sockaddr oldaddr;    /* same with serveraddr */
	int poll_timeout;
	int server_fd;
	pthread_t master;                
	void *(*precheck_func)(void *); /* may make some pre check by use us */
	void *(*accept_func)(void *);   /* incharge of accept connect */
	void *(*work_func)(void *);     /* real work func for this connect */
	const char *name;
};

/*! \brief
* stand for a signle tcp session instance */
struct spd_tcp_session {
	FILE *f;
	int is_client; /* which mod we are  ? */
	int client_fd; /* returned by accept */
	struct spd_sockaddr remote_addr;
	struct spd_tcp_session_arg *arg;   /* args for accept thread */
	spd_mutex_t lock;
};

/*
 *\brief suport for client mode
 */

/*
  *\brief just create socket 
 */
struct spd_tcp_session *spd_tcp_session_client_create(struct spd_tcp_session_arg *arg);

/*
 *\brief start client side socket
 */
struct spd_tcp_session *spd_tcp_session_client_start(struct spd_tcp_session *arg);


/*
 * \breif  suport for server mode
 */

/*
  *\brief start a tcp server instance
  */
void spd_tcp_server_start(struct spd_tcp_session_arg *arg);

/*
 *\brief shutdown a runing tcp server instance
*/
void spd_tcp_server_stop(struct spd_tcp_session_arg *arg);

/* defualt hanle of accept , you may replace it by set work_func callback */
void *spd_tcp_server_mainloop(void *);

int spd_tcp_server_read(struct spd_tcp_session *session, void *buf, size_t count);
int spd_tcp_server_write(struct spd_tcp_session *session, const void *buf, size_t count);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
