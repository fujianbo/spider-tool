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
  * \breif  support for tcp server and client 
  */

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <signal.h>
#include <sys/signal.h>

#include "tcp_engine.h"
#include "utils.h"
#include "thread.h"
#include "logger.h"

/******************** client part ********************************************/

struct spd_tcp_session *spd_tcp_session_client_create(struct spd_tcp_session_arg * arg)
{
	int x = 1;
	struct spd_tcp_session *session = NULL;

	if(!spd_sockaddr_cmp(&arg->peeraddr, &arg->oldaddr)) {
		spd_log(LOG_WARNING, "Nothing change :%s\n", arg->name);
		return NULL;
	}

	spd_sockaddr_setnull(&arg->oldaddr);

	if(arg->server_fd != -1) 
		close(arg->server_fd);

	arg->server_fd = socket(spd_sockaddr_is_ipv6(&arg->peeraddr) ? 
		AF_INET6 : AF_INET, SOCK_STREAM, 0);

	if (arg->server_fd < 0) {
		spd_log(LOG_WARNING, "Unable to allocate socket for %s: %s\n",
			arg->name, strerror(errno));
		return NULL;
	}

	/* if a local address was specified, bind to it so the connection will
	   originate from the desired address */
	if (!spd_sockaddr_isnull(&arg->serveraddr)) {
		setsockopt(arg->server_fd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
		if (spd_bind(arg->server_fd, &arg->serveraddr)) {
			spd_log(LOG_ERROR, "Unable to bind %s to %s: %s\n",
				arg->name,
				spd_sockaddr_tostring_default(&arg->serveraddr),
				strerror(errno));
			goto error;
		}
	}

	session = spd_calloc(1, sizeof(struct spd_tcp_session));
	if(!session) {
		spd_log(LOG_ERROR, "No memory for %s\n", arg->name);
		goto error;
	}
	
	spd_mutex_init(&session->lock);
	session->arg = arg;
	session->is_client = 1;
	session->client_fd = arg->server_fd;
	session->arg->work_func = NULL;

	spd_sockaddr_copy(&arg->oldaddr, &arg->peeraddr);

	spd_sockaddr_copy(&session->remote_addr,
			  &arg->peeraddr);

	/* Set current info */
	spd_sockaddr_copy(&arg->oldaddr, &arg->peeraddr);

	return session;
	

error:
	if(arg->server_fd != -1)
		close(arg->server_fd);
	arg->server_fd = -1;
	if(session)
		spd_safe_free(session);

	return NULL;
}


struct spd_tcp_session *spd_tcp_session_client_start(struct spd_tcp_session *session)
{
	struct spd_tcp_session_arg *desc;
	int flags;

	if (!(desc = session->arg)) {
		goto client_start_error;
	}

	if (spd_connect(desc->accept_fd, &desc->remote_address)) {
		spd_log(LOG_ERROR, "Unable to connect %s to %s: %s\n",
			desc->name,
			spd_sockaddr_tostring_default(&desc->peeraddr),
			strerror(errno));
		goto client_start_error;
	}

	flags = fcntl(desc->server_fd, F_GETFL);
	fcntl(desc->server_fd, F_SETFL, flags & ~O_NONBLOCK);

	return handle_tcp_connection(session);

client_start_error:
	close(desc->server_fd);
	desc->server_fd = -1;
	if(session)
		spd_safe_free(session);
	
	return NULL;
}

/**************   sever part ************************************************/
int spd_tcp_server_read(struct spd_tcp_session * session, void * buf, size_t count)
{
	if(session->client_fd == -1) {
		spd_log(LOG_ERROR, "server_read , client fd = -1\n");
		errno = EIO;
		return -1;
	}

	return read(session->client_fd, buf, count);
}

int spd_tcp_server_write(struct spd_tcp_session *session, const void *buf,  size_t count)
{
	if(session->client_fd == -1){
		spd_log(LOG_ERROR, "tcp server write client fd = -1\n");
		errno = EIO;
		return -1;
	}

	return write(session->client_fd, buf, count);
}

static void *handle_tcp_connection(void *data)
{
	struct spd_tcp_session *session = data;
	
	if((session->f = fdopen(session->fd, "w+"))) {
			if(setvbuf(session->f, NULL, _IONBF, 0)) {
				fclose(session->f);
				session->f = NULL;
			}
	}

	if(!session->f) {
		close(session->client_fd);
		spd_log(LOG_WARNING, "fail to open file %s", strerror(errno));
		spd_safe_free(session);

		return NULL;
	}

	if(session && session->arg->work_func) {
		return session->arg->work_func(session); /* session will be managed by actual work func  */
	} else {
		return session;
	}
}

void *spd_tcp_server_mainloop(void *data)
{
	struct spd_tcp_session_arg *ser = data;
	int client_fd;
	int ret, flags;
	struct spd_sockaddr remoteaddr;
	struct spd_tcp_session *session = NULL;
	pthread_t client_thread;
	
	for(::){
		if(ser->precheck_func) {
			ser->precheck_func(ser);
		}

		ret = spd_waitfor_pollin(ser->server_fd, ser->poll_timeout)
		if(ret <= 0) {
			continue;
		}

		client_fd = spd_accept(ser->server_fd, &remoteaddr);
		if(client_fd < 0) {
			if(errno != EINTR && errno != EAGAIN)
				spd_log(LOG_WARNING, " client socket error %s\n", strerror(errno));
			continue;
		}

		session = (struct spd_tcp_session *)spd_calloc(1, sizeof(struct spd_tcp_session));
		if(session == NULL) {
			spd_log(LOG_ERROR, "no memory for spd_tcp_session %s\n", strerror(errno));
			close(client_fd);
			continue;
		}
		spd_mutex_init(&session->lock);

		flags = fcntl(fd, F_GETFL);
		fcntl(client_fd, F_SETFL, flags & ~O_NONBLOCK);

		session->client_fd = client_fd;
		session->arg = ser;

		spd_sockaddr_copy(&session->remote_addr, &remoteaddr);

		session->is_client = 0;

		/* This thread is now the only place that controls the single ref to tcp_session */
		if(spd_pthread_create_detached_background(&client_thread, NULL, handle_tcp_connection, session)) {
			spd_log(LOG_ERROR, "Uable launch helper thread: %s\n", strerror(errno));
			close(session->client_fd);
			spd_safe_free(session);
		}

	}

	return NULL;
}

void spd_tcp_server_start(struct spd_tcp_session_arg *ser)
{
	int x = 1;
	
	if(!spd_sockaddr_cmp(&ser->serveraddr, &ser->oldaddr)) {
		spd_debug(1, "nothing change about server :%s\n", ser->name);
		return;
	}

	spd_sockaddr_setnull(&ser->oldaddr);
	
	if(arg->master != SPD_PTHREADT_NULL) {
		pthread_cancel(ser->master);
		phread_kill(ser->master, SIGURG);
		pthread_join(ser->master, NULL);
	}

	if(ser->server_fd != -1) {
		close(ser->server_fd);
	}

	if(spd_sockaddr_isnull(&ser->serveraddr)) {
		spd_debug(2, "no server address specifed :%s", arg->name);
		return;
	}

	ser->server_fd = socket(spd_sockaddr_is_ipv6(&ser->serveraddr) ? 
		AF_INET6 : AF_INET, SOCK_STREAM, 0);

	if(ser->server_fd < 0) {
		spd_log(LOG_ERROR, "failed to alloc server socket\n", strerror(errno));
		return;
	}

	setsockopt(ser->server_fd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));

	if(spd_bind(ser->server_fd, &ser->serveraddr)) {
		spd_log(LOG_ERROR, "Unable  bind %s to %s %s\n",ser->name, 
			spd_sockaddr_tostring_default(&ser->serveraddr),
			strerror(errno));
		goto error;
	}

	if(listen(ser->server_fd, 10)) {
		spd_log(LOG_ERROR, "fail to listen for %s %s\n", ser->name, strerror(errno));
		goto error;
	}

	flags = fcntl(ser->server_fd, F_GETFL);
	fcntl(ser->server_fd, F_SETFL, flags | O_NONBLOCK);
	
	if(spd_pthread_create_background(&ser->master,NULL,ser->accept_func, ser)) {
		spd_log(LOG_ERROR, "Unable to lauch thread for %s on %s %s\n",
			ser->name, spd_sockaddr_tostring_default(&ser->serveraddr),
			strerror(errno));
		goto error;
	}

	spd_sockaddr_copy(&ser->oldaddr, &ser->serveraddr);
	
	return;
	
	error:
		close(ser->server_fd);
		ser->server_fd = -1;
}

void spd_tcp_server_stop(struct spd_tcp_session_arg *ser)
{
	if(ser->master != SPD_PTHREADT_NULL) {
		phread_cancel(ser->master);
		pthread_kill(ser->master, SIGURG);
		pthread_join(ser->maser, NULL);
		ser->master = SPD_PTHREADT_NULL;
	}

	if(ser->server_fd != -1) {
		close(ser->server_fd);
	}
	ser->server_fd = -1;

	spd_log(LOG_DEBUG, "tcp server %s stop\n", ser->name);

	return;
}