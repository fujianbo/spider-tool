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
  * \brief stun client implimentation
  */
 
#ifndef _STUN_H_
#define _STUN_H_
 
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif
 
#define DEFUALT_STUN_PORT    3478
 
 
enum spd_stun_result {
	SPD_STUN_ACCEPT,
	SPD_STUN_IGNORE,
};
 
struct stun_attr;
 
/*
  *\ brief generic stun request.
  */
int spd_stun_bind(int fd, struct sockaddr_in *dest, const char* username, struct sockaddr_in *resp);
 
 
/*
  * \brief callback to be call when responce received.
  */
 
typedef int (stun_resp_cb)(struct stun_attr *attr, void *data);
 
/*
  * \brief handle an incoming stun message.
  */
int spd_stun_response_handle(int fd, struct sockaddr_in* from, unsigned char* data, size_t len, stun_resp_cb *call_back, void* arg);
 
 
#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif
