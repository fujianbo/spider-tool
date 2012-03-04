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
 
#include "stun.h"
#include "poll.h"
#include "socket.h"
#include "logger.h"
 
 
/*!
 * \brief STUN support code
 *
 * http://www.ietf.org/rfc/rfc3489.txt
 *
 * This code provides some support for doing STUN transactions.
 * STUN is described in RFC3489 and it is based on the exchange
 * of UDP packets between a client and one or more servers to
 * determine the externally visible address (and port) of the client
 * once it has gone through the NAT boxes that connect it to the
 * outside.
 * The simplest request packet is just the header defined in
 * struct stun_header, and from the response we may just look at
 * one attribute, STUN_MAPPED_ADDRESS, that we find in the response.
 * By doing more transactions with different server addresses we
 * may determine more about the behaviour of the NAT boxes, of
 * course - the details are in the RFC.
 *
 * All STUN packets start with a simple header made of a type,
 * length (excluding the header) and a 16-byte random transaction id.
 * Following the header we may have zero or more attributes, each
 * structured as a type, length and a value (whose format depends
 * on the type, but often contains addresses).
 * Of course all fields are in network format.
 */
 
/*! \brief STUN message types
 * 'BIND' refers to transactions used to determine the externally
 * visible addresses. 'SEC' refers to transactions used to establish
 * a session key for subsequent requests.
 * 'SEC' functionality is not supported here.
 */
 
#define STUN_BINDREQ    0x0001
#define STUN_BINDRESP   0x0101
#define STUN_BINDERROR  0x0111
#define STUN_SECREQ     0x0002
#define STUN_SECRESP    0x0102
#define STUN_SECERROR   0x0112
 
 
 
/*! \brief Basic attribute types in stun messages.
 * Messages can also contain custom attributes (codes above 0x7fff)
 */
 
#define STUN_MSG_MAPPED_ADDR   		0x0001
#define STUN_MSG_RESPONCE_ADDR 		0x0002
#define STUN_MSG_RESPONSE_ADDRESS	0x0002
#define STUN_MSG_CHANGE_REQUEST		0x0003
#define STUN_MSG_SOURCE_ADDRESS		0x0004
#define STUN_MSG_CHANGED_ADDRESS	0x0005
#define STUN_MSG_USERNAME			0x0006
#define STUN_MSG_PASSWORD			0x0007
#define STUN_MSG_INTEGRITY		0x0008
#define STUN_MSG_ERROR_CODE			0x0009
#define STUN_MSG_UNKNOWN_ATTRIBUTES	0x000a
#define STUN_MSG_REFLECTED_FROM		0x000b
 
#define stun_debug 0;
 
typedef struct { unsigned int id[4]; } __attribute__((packed)) stun_transac_id;
 
 
   /*
	   0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |0 0|     STUN Message Type     |         Message Length        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                         Magic Cookie                          |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                     Transaction ID (96 bits)                  |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
 
struct stun_header {
	unsigned short msgtype;
	unsigned short msglen;
	stun_transac_id id; 
	unsigned char attr[0];
};
 
struct stun_attr {
	unsigned short attr;
	unsigned short len;
	unsigned char value[0];
} __attribute__((packed));
 
struct stun_addr {
	unsigned char unused;
	unsigned char family;
	unsigned short port;
	unsigned int addr;
} __attribute__((packed));
 
struct stun_state {
	const char *username;
	const char *password;
};
 
static const char* stun_msg2str(int msg)
{
	switch(msg) {
		case STUN_BINDREQ:
			return "Binding Request";
		case STUN_BINDRESP:
			return "Binding Responce";
		case STUN_BINDERROR:
			return "Binding Error";
		case STUN_SECREQ:
			return "SecRequest";
		case STUN_SECRESP:
			return "SecResponce"
		case STUN_SECERROR:
			return "SecError";
		default:
		return "Not RFC 3489 MSG";
	}
}
 
static const char *stun_attr2str(int msg)
{
	switch (msg) {
	case STUN_MSG_MAPPED_ADDR:
		return "Mapped Address";
	case STUN_MSG_RESPONCE_ADDR:
		return "Responce Address";
	case STUN_MSG_CHANGE_REQUEST:
		return "Change Request";
	case STUN_MSG_SOURCE_ADDRESS:
		return "Source Address";
	case STUN_MSG_CHANGED_ADDRESS:
		return "Changed Address";
	case STUN_MSG_USERNAME:
		return "Username";
	case STUN_MSG_PASSWORD:
		return "Password";
	case STUN_MSG_INTEGRITY:
		return "Message Integrity";
	case STUN_MSG_UNKNOWN_ATTRIBUTES:
		return "Unknown Attributes";
	case STUN_MSG_REFLECTED_FROM:
		return "Reflected From";
	case STUN_MSG_ERROR_CODE:
		return "Error Code";	
	}
	return "Non-RFC3489 Attribute";
}
 
 
static void stun_transaction_id(struct stun_header *req)
{	
	if(req) {
		int x;
		for(x = 0; x < 4; x++)
			req->id.id[x] = spd_random(void);
	}
}
 
static void apppend_stunattr(struct stun_attr **attr, int attrvalue, const char *s, int *len, int *left)
{
	int size = sizeof(**attr) + strlen(s);
	if(*left > size) {
		(*attr)->attr = htons(attrvalue);
		(*attr)->len = htons(strlen(s));
		memcpy((*attr)->value, s, strlen(s));
		(*attr) = (struct stun_attr *) ((*attr)->value + strlen(s));
		*len += size;
		*left -=size;
	}
}
 
static int stun_transaction_match(stun_transac_id *left, stun_transac_id *right)
{	
	return memcmp(left, right, sizeof(*left));
}
 
static int stun_send(int s, struct sockaddr_in *dst, struct stun_header *resp)
{
	return sendto(s, resp, ntohs(resp->msglen) + sizeof(*resp), 0,
			(struct sockaddr *)dst, sizeof(*dst));
}
 
static void append_attr_address(struct stun_attr **attr, int attrval, struct sockaddr_in *sin, int *len, int *left)
{
	int size = sizeof(**attr) + 8;
	struct stun_addr *addr;
	if(*left > size) {
		(*attr)->attr = htons(attrval);
		(*attr)->len = htons(8);
		addr = (struct stun_addr *)((*attr)->value);
		addr->unused = 0;
		addr->family = 0x01;
		addr->port = sin->sin_port;
		addr->addr = sin->sin_addr.s_addr;
		(*attr) = (struct stun_attr *) ((*attr)->value + 8);
		*len += size;	
	}
}
 
static stun_process_attr(struct stun_state *state, struct stun_attr *attr)
{
	if(stun_debug)
		spd_log(LOG_DEBUG, "Found stun Attribute %s (%04x), length %d\n",
			stun_attr2str(ntohs(attr->attr)), ntohs(attr->attr), ntohs(attr->len));
	switch(ntohs(attr->attr)) {
	case STUN_MSG_USERNAME:
		state->username = (const char *)(attr->value);
		break;
	case STUN_MSG_PASSWORD:
		state->password = (const char*) (attr->value);
		break;
	default:
		if (stun_debug)
			spd_log(LOG_DEBUG, "Ignoring STUN attribute %s (%04x), length %d\n",
				    stun_attr2str(ntohs(attr->attr)), ntohs(attr->attr), ntohs(attr->len));
	}
	return 0;
}
 
int spd_stun_response_handle(int fd, struct sockaddr_in *src, unsigned char *data, size_t len, stun_cb_f *stun_cb, void *arg)
{
	struct stun_header *hdr = (struct stun_header *)data;
	struct stun_attr *attr;
	struct stun_state st;
	int ret = SPD_STUN_IGNORE;
	int x;
 
	if(len < sizeof(struct stun_header)) {
		spd_log(LOG_DEBUG, "runt stun packet (only %d, wanting at least %d)\n", (int)len, (int)sizeof(struct stun_header));
		return -1;
	}
 
	len -= sizeof(struct stun_header);
	data += sizeof(struct stun_header);
 
	x = ntohs(hdr->msglen);
 
	if(stun_debug) {
		spd_log(LOG_DEBUG, "stun packet, msg %s (%04x), length: %d\n", stun_msg2str(ntohs(hdr->msgtype)), ntohs(hdr->msgtype), x);
	} 
 
	if(x > len) {
		spd_log(LOG_DEBUG, "Scrambled STUN packet length (got %d, expecting %d)\n", x, (int)len);
	} else
		len = x;
 
	memset(&st, 0, sizeof(st));
 
	while(len) {
		if(len < sizeof(struct stun_attr)) {
			spd_log(LOG_WARNING, "stun attribute got (%d)  expect (%d) \n", len, (int)sizeof(struct stun_attr));
			 break;
		}
 
		if(stun_cb)
			stun_cb(attr, arg);
 
		if(stun_process_attr(&st, attr)) {
			spd_log(LOG_WARNING, "Failed to handle attribute %s (%04x)\n", stun_attr2str(attr->attr), ntohs(attr->attr));
			break;
		}
 
		attr->attr = 0;
		data += x;
		len -= x;
	}
	*data = '\0';
 
	if(len == 0) {
		unsigned char respdata[1024];
		struct stun_header *resp = (struct stun_header)respdata;
		int resplen = 0;
		int respleft = sizeof(respdata) - sizeof(struct stun_header);
 
		resp->id = hdr->id;
		resp->msgtype = 0;
		resp->msglen = 0;
		attr = (struct stun_attr *)resp->attr;
		switch(ntohs(hdr->msgtype)) {
			case STUN_BINDREQ:
				if(stun_debug)
					spd_log(LOG_DEBUG, "STUN Bind Request, username: %s\n",
					    st.username ? st.username : "<none>");
				if(st.username)
					apppend_stunattr(&attr, STUN_MSG_USERNAME, st.username, &resplen, &respleft);
				append_attr_address(&attr, STUN_MSG_MAPPED_ADDR, src, &resplen, &respleft);
				resp->msglen = htons(resplen);
				resp->msgtype = htons(STUN_BINDRESP);
				stun_send(fd, src, resp);
				ret = SPD_STUN_ACCEPT;
				break;
			default:
				if(stun_debug)
					spd_log(LOG_DEBUG, "Dunnon what to do with stun message %04x (%s)\n",
							ntohs(hdr->msgtype), stun_msg2str(ntohs(hdr->msgtype)));
		}
	}
 
	return ret;
	
}
 
/*! \brief Extract the STUN_MAPPED_ADDRESS from the stun response.
 * This is used as a callback for stun_handle_response
 * when called from ast_stun_request.
 */
static int stun_get_mapped(struct stun_attr *attr, void *arg)
{
	struct stun_addr *addr = (struct stun_addr *)(attr+ 1);
	struct sockaddr_in *sa = (struct sockaddr_in *)arg;
 
	if(ntohs(attr->attr) != STUN_MSG_MAPPED_ADDR || ntohs(attr->len) != 8)
		return 1;
	sa->sin_port = addr->port;
	sa->sin_addr.s_addr = addr->addr;
	return 0;
}
 
int spd_stun_bind(int fd, struct sockaddr_in *dest, const char *username, struct sockaddr_in *resp)
{
	struct stun_header *req;
	struct stun_header *rsp;
	unsigned char req_buf[1024];
	unsigned char rsp_buf[1024];
	int reqlen, reqleft;
	struct stun_attr *attr;
	int res = -1;
	int retry;
 
	if(resp) {
		memset(resp, 0, sizeof(struct sockaddr_in));
	}
 
	req = (struct stun_header *) req_buf;
	/* construct stun header */
	stun_transaction_id(req);
	reqlen = 0;
	reqleft = sizeof(req_buf) - sizeof(struct stun_header);
	req->msgtype = 0;
	req->msglen = 0;
	/* stun body */
	attr = (struct stun_attr *) req->attr;
	if(username) {
		apppend_stunattr(&attr, STUN_MSG_USERNAME, username, &reqlen, &reqleft);
	}
 
	req->msglen = htons(reqlen);
	req->msgtype = htons(STUN_BINDREQ);
 
	for(retry = 0; retry< 3; retry++;) {
		struct sockaddr_in src;
		socklen_t srclen;
 
		res = stun_send(fd, dest, req);
		if(res < 0) {
			spd_log(LOG_DEBUG, " stun_send try %d failed: %s\n", retry, strerror(errno));
			break;
		}
 
		if(!resp) {
			res = 0;
			break;
		}
	try_again:
			{
				struct pollfd pfds = { .fd = fd, .events = POLLIN };
				res = spd_poll(&pfds, 1, 3000);
				if(res < 0) {
					/* Error */
					continue;
				}
				if(!res) {
					/* timeout */
					res = 1;
					continue;
				}
			}
		/* read stun response  */
		memset(&src, 0, sizeof(src));
		srclen = sizeof(src);
 
		res = recvfrom(fd, rsp_buf, sizeof(rsp_buf) -1,
			0, (struct sockaddr *)&src, &srclen);
		if(res < 0) {
			spd_log(LOG_DEBUG, "recvfrom try %d failed: %s\n", retry, strerror(errno));
			break;
		}
 
		rsp = (struct stun_header *) rsp_buf;
		if(spd_stun_response_handle(fd, &src, rsp_buf, res, stun_get_mapped, resp)
			|| (rsp->msgtype != htons(STUN_BINDRESP))
			|| stun_transaction_match(&req->id, &rsp->id)) {
			memset(resp, 0, sizeof(struct sockaddr_in));
 
			goto try_again;
		}
		res = 0;
		break;
	}
	return res;
}