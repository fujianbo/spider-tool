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

#ifndef _TCPSERVER_H
#define _TCPSERVER_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* pending request queue size */
#define LISTENQ  10

int init_tcp_server();

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif 

