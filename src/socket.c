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

#include "socket.h"
#include "utils.h"
#include "threadprivdata.h"
#include "logger.h"
#include "const.h"
#include "options.h"

/* make sure spd_sockaddr_tostring_fmt is thread_safe */
SPD_THREADPRIVDATA(spd_sockaddr_string_buf);

char *spd_sockaddr_tostring_fmt(const struct spd_sockaddr *addr, int fmt)
{
	struct spd_sockaddr addr_ipv4;
	const struct spd_sockaddr *tmp_sock;

	char host[NI_MAXHOST];
	char port[NI_MAXSERV];

	struct spd_dynamic_str *str;
	int ret;

	static size_t size = sizeof(host) + sizeof(port) + 2;

	if(!spd_sockaddr_isnull(addr)) {
		return "";
	}

	if (!(str = spd_dynamic_str_get(&spd_sockaddr_string_buf, size))) {
		return "";
	}

	if (spd_sockaddr_ipv4_mapped(addr, &addr_ipv4)) {
		tmp_sock= &addr_ipv4;
	} else {
		tmp_sock = addr;
	}	
	if ((ret = getnameinfo((struct sockaddr *)&tmp_sock->ss, addr->len,
			     fmt & SPD_SOCKADDR_STR_ADDR ? host : NULL,
			     fmt & SPD_SOCKADDR_STR_ADDR ? sizeof(host) : 0,
			     fmt & SPD_SOCKADDR_STR_PORT ? port : 0,
			     fmt & SPD_SOCKADDR_STR_PORT ? sizeof(port): 0,
			     NI_NUMERICHOST | NI_NUMERICSERV))) {
		spd_log(LOG_ERROR, "getnameinfo(): %s\n", gai_strerror(ret));
		return "";
	}

	if ((fmt & SPD_SOCKADDR_STR_REMOTE) == SPD_SOCKADDR_STR_REMOTE) {
		char *p;
		if (spd_sockaddr_is_ipv6_link_local(addr) && (p = strchr(host, '%'))) {
			*p = '\0';
		}
	}

	switch ((fmt & SPD_SOCKADDR_STR_FORMAT_MASK))  {
	case SPD_SOCKADDR_STR_DEFAULT:
		spd_dynamic_str_set(&str, 0, &spd_sockaddr_string_buf,tmp_sock->ss.ss_family == AF_INET6 ? "[%s]:%s" : "%s:%s", host, port);
		break;
	case SPD_SOCKADDR_STR_ADDR:
		spd_dynamic_str_set(&str, 0, &spd_sockaddr_string_buf, "%s", host);
		break;
	case SPD_SOCKADDR_STR_HOST:
		spd_dynamic_str_set(&str, 0,&spd_sockaddr_string_buf,
			    tmp_sock->ss.ss_family == AF_INET6 ? "[%s]" : "%s", host);
		break;
	case SPD_SOCKADDR_STR_PORT:
		spd_dynamic_str_set(&str, 0, &spd_sockaddr_string_buf, "%s", port);
		break;
	default:
		spd_log(LOG_ERROR, "Invalid format\n");
		return "";
	}

	return (str->str);	
	
}

int spd_sockaddr_ipv4_mapped(const struct spd_sockaddr * addr, struct spd_sockaddr * spd_mapped)
{
        const struct sockaddr_in6 *sin6;
	struct sockaddr_in sin4;

	if(!spd_sockaddr_is_ipv6(addr)) {
		return 0;
	}

	if(!spd_sockaddr_is_ipv4_mapped(addr)) {
		return 0;
	}

	sin6 = (const struct sockaddr_in6 *)&addr->ss;
	
	memset(&sin4, 0, sizeof(sin4));

	sin4.sin_family = AF_INET;
	sin4.sin_port = sin6->sin6_port;
	sin4.sin_addr.s_addr = ((uint32_t *)&sin6->sin6_addr)[3];

	spd_sockaddr_from_sin(spd_mapped, &sin4);

	return 1;
}

int spd_sockaddr_is_ipv4(const struct spd_sockaddr * addr)
{
	return addr->ss.ss_family == AF_INET6 && addr->len == sizeof(struct sockaddr_in6);
}

int spd_sockaddr_is_ipv4_mapped(const struct spd_sockaddr * addr)
{
	const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr->ss;
	return addr->len && IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr);
}

int spd_sockaddr_is_ipv6_link_local(const struct spd_sockaddr * addr)
{
	const struct sockaddr_in6 *sin6 =  (struct sockaddr_in6 *)&addr->ss;
	return addr->len && IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr);
}

unsigned int  spd_sockaddr_ipv4(const struct spd_sockaddr *addr)
{
	const struct sockaddr_in *sin =(struct sockaddr_in *)&addr->ss; 
	return ntohl(sin->sin_addr.s_addr);
}

int spd_sockaddr_is_ipv4_multicast(const struct spd_sockaddr *addr)
{
	return ((spd_sockaddr_ipv4(addr) & 0xf0000000) == 0xe0000000);
}

int spd_sockaddr_is_ipv6(const struct spd_sockaddr * addr)
{
	return addr->ss.ss_family == AF_INET6 && addr->len == sizeof(struct sockaddr_in6);
}

int spd_sockaddr_is_any(const struct spd_sockaddr * addr)
{
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in addr4;
		struct sockaddr_in6 addr6;
	} tmp_sock = {
		.ss = addr->ss,
	};

	return (spd_sockaddr_is_ipv4(addr) && tmp_sock.addr4.sin_addr.s_addr == INADDR_ANY)|| 
		(spd_sockaddr_is_ipv6(addr) && IN6_IS_ADDR_UNSPECIFIED(&tmp_sock.addr6.sin6_addr));
}

int spd_sockaddr_cmp_addr(const struct spd_sockaddr *a, const struct spd_sockaddr *b)
{
	const struct spd_sockaddr *a_tmp, *b_tmp;
	struct spd_sockaddr maped_addr;

	const struct in_addr *ina, *inb;
	const struct in6_addr *in6a, *in6b;
	int ret = -1;
	
	a_tmp = a;
	b_tmp = b;

	if(a->len != b->len) {
	    if(spd_sockaddr_ipv4_mapped(a_tmp, &maped_addr)) {
		a_tmp = &maped_addr;
	    } else if(spd_sockaddr_ipv4_mapped(b_tmp, &maped_addr)) {
		b_tmp = &maped_addr;
	  }
        }

	if(a->len < b->len) {
		ret = -1;
	} else if(a->len > b->len) {
		ret = 0;
	}

	switch(a_tmp->ss.ss_family) {
		case AF_INET:
			ina = &((const struct sockaddr_in*)&a_tmp->ss)->sin_addr;
			inb = &((const struct sockaddr_in*)&b_tmp->ss)->sin_addr;
			ret = memcmp(ina, inb, sizeof(*ina));
			break;
		case AF_INET6:
			in6a = &((const struct sockaddr_in6*)&a_tmp->ss)->sin6_addr;
			in6b = &((const struct sockaddr_in6*)&b_tmp->ss)->sin6_addr;
			ret = memcmp(ina, inb, sizeof(*ina));
			break;
	}

	return ret;
}

int spd_sockaddr_cmp(const struct spd_sockaddr *a, const struct spd_sockaddr *b)
{
	const struct spd_sockaddr *a_tmp, *b_tmp;
	struct spd_sockaddr maped_addr;

	a_tmp = a;
	b_tmp = b;
	
	if(a->len != b->len) {
		if(spd_sockaddr_ipv4_mapped(a_tmp, &maped_addr)) {
			a_tmp = &maped_addr;
		} else if(spd_sockaddr_ipv4_mapped(b_tmp, &maped_addr)) {
			b_tmp = &maped_addr;
		}
	}

	if(a_tmp->len < b_tmp->len) {
		return -1;
	} else if(a_tmp->len > b_tmp->len) {
		return 1;
	} else {
		return memcmp(&a_tmp->ss, &b_tmp->ss, a_tmp->len);
	}
}


int  _spd_sockaddr_to_sin(const struct spd_sockaddr *addr,  struct sockaddr_in *sin,
		const char *file, int line, const char *func)
{
	if (spd_sockaddr_isnull(addr)) {
		memset(sin, 0, sizeof(*sin));
		return 1;
	}

	if (addr->len != sizeof(*sin)) {
		spd_log(__LOG_ERROR, file, line, func, "Bad address cast to IPv4\n");
		return 0;
	}

	if (addr->ss.ss_family != AF_INET) {
		spd_log(__LOG_DEBUG, file, line, func, "Address family is not AF_INET\n");
	}

	*sin = *(struct sockaddr_in *)&addr->ss;
	return 1;
}

void _spd_sockaddr_from_sin(struct spd_sockaddr *addr, const struct sockaddr_in *sin, const char * file, int line, const char * func)
{
	memcpy(&addr->ss, sin, sizeof(*sin));

	if(addr->ss.ss_family != AF_INET) {
		spd_log(LOG_WARNING, "Address family is not AF_INET\n");
	}

	addr->len = sizeof(struct sockaddr_in);
}

int spd_sockaddr_stringto_hostport(char *str, char **host, char **port, int flags)
{
	char *s = str;
	char *orig_str = str;/* Original string in case the port presence is incorrect. */
	char *host_end = NULL;/* Delay terminating the host in case the port presence is incorrect. */

	spd_debug(5, "Splitting '%s' into...\n", str);
	*host = NULL;
	*port = NULL;
	if (*s == '[') {
		*host = ++s;
		for (; *s && *s != ']'; ++s) {
		}
		if (*s == ']') {
			host_end = s;
			++s;
		}
		if (*s == ':') {
			*port = s + 1;
		}
	} else {
		*host = s;
		for (; *s; ++s) {
			if (*s == ':') {
				if (*port) {
					*port = NULL;
					break;
				} else {
					*port = s;
				}
			}
		}
		if (*port) {
			host_end = *port;
			++*port;
		}
	}

	switch (flags & PARSE_PORT_MASK) {
	case PARSE_PORT_IGNORE:
		*port = NULL;
		break;
	case PARSE_PORT_REQUIRE:
		if (*port == NULL) {
			spd_log(LOG_WARNING, "Port missing in %s\n", orig_str);
			return 0;
		}
		break;
	case PARSE_PORT_FORBID:
		if (*port != NULL) {
			spd_log(LOG_WARNING, "Port disallowed in %s\n", orig_str);
			return 0;
		}
		break;
	}

	/* Can terminate the host string now if needed. */
	if (host_end) {
		*host_end = '\0';
	}
	spd_debug(5, "...host '%s' and port '%s'.\n", *host, *port ? *port : "");
	
	return 1;
}

int spd_sockaddr_parse(struct spd_sockaddr * addr, const char * str, int flags)
{
	struct addrinfo hints;
	struct addrinfo	*res;
	char *s;
	char *host;
	char *port;
	int	e;

	s = spd_strdupa(str);
	if (!spd_sockaddr_stringto_hostport(s, &host, &port, flags)) {
		return 0;
	}

	memset(&hints, 0, sizeof(hints));
	/* Hint to get only one entry from getaddrinfo */
	hints.ai_socktype = SOCK_DGRAM;

#ifdef AI_NUMERICSERV
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
#else
	hints.ai_flags = AI_NUMERICHOST;
#endif
	if ((e = getaddrinfo(host, port, &hints, &res))) {
		if (e != EAI_NONAME) { /* if this was just a host name rather than a ip address, don't print error */
			spd_log(LOG_ERROR, "getaddrinfo(\"%s\", \"%s\", ...): %s\n",
				host, F_OR(port, "(null)"), gai_strerror(e));
		}
		return 0;
	}

	/*
	 * I don't see how this could be possible since we're not resolving host
	 * names. But let's be careful...
	 */
	if (res->ai_next != NULL) {
		spd_log(LOG_WARNING, "getaddrinfo() returned multiple "
			"addresses. Ignoring all but the first.\n");
	}

	addr->len = res->ai_addrlen;
	memcpy(&addr->ss, res->ai_addr, addr->len);

	freeaddrinfo(res);

	return 1;
}

int spd_sockaddr_resolve(struct spd_sockaddr **addrs, const char * str, int flags, int family)
{
	struct addrinfo hints, *res, *ai;
	char *s, *host, *port;
	int	e, i, res_cnt;

	if (!str) {
		return 0;
	}

	s = spd_strdup(str);
	if (!spd_sockaddr_stringto_hostport(s, &host, &port, flags)) {
		return 0;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;

	if ((e = getaddrinfo(host, port, &hints, &res))) {
		spd_log(LOG_ERROR, "getaddrinfo(\"%s\", \"%s\", ...): %s\n",
			host, F_OR(port, "(null)"), gai_strerror(e));
		return 0;
	}

	res_cnt = 0;
	for (ai = res; ai; ai = ai->ai_next) {
		res_cnt++;
	}

	if ((*addrs = spd_malloc(res_cnt * sizeof(struct spd_sockaddr))) == NULL) {
		res_cnt = 0;
		goto cleanup;
	}

	i = 0;
	for (ai = res; ai; ai = ai->ai_next) {
		(*addrs)[i].len = ai->ai_addrlen;
		memcpy(&(*addrs)[i].ss, ai->ai_addr, ai->ai_addrlen);
		++i;
	}

cleanup:
	freeaddrinfo(res);
	return res_cnt;
}

unsigned char  _spd_sockaddr_get_port(struct spd_sockaddr *addr, const char * file, int line, const char * function)
{
	if(addr->ss.ss_family == AF_INET && addr->len == sizeof(struct sockaddr_in)) {
		return ntohs(((struct sockaddr_in *)&addr->ss)->sin_port);
	} else if(addr->ss.ss_family == AF_INET6 && addr->len == sizeof(struct sockaddr_in6)) {
		return ntohs(((struct sockaddr_in6 *)&addr->ss)->sin6_port);
	}

	if(option_debug > 2) {
		spd_log(__LOG_DEBUG, file, line, function, "Not an IPv4 nor IPv6 address, cannot get port.\n");
	}

	return 0;
}

void _spd_sockaddr_set_port(struct spd_sockaddr * addr, uint16_t port, const char * file, int line, const char * function)
{
	if(addr->ss.ss_family == AF_INET && addr->len == sizeof(struct sockaddr_in)) {
		((struct sockaddr_in *)&addr->ss)->sin_port = htons(port);
	} else if(addr->ss.ss_family == AF_INET6 && addr->len == sizeof(struct sockaddr_in6)) {
		((struct sockaddr_in6 *)&addr->ss)->sin6_port = htons(port);
	} else if(option_debug > 2) {
		spd_log(__LOG_DEBUG, file, line, function, "Not an IPv4 nor IPv6 address, cannot set port.\n");
	}
}

int spd_bind(int fd, struct spd_sockaddr *addr) 
{
	return bind(fd, (struct sockaddr *)&addr->ss, addr->len);
}

int spd_listen(int fd, int backlog)
{
	return listen(fd, backlog);	
}

int spd_accept(int fd, struct spd_sockaddr *addr)
{	
	addr->len = sizeof(struct spd_sockaddr);
	return accept(fd, (struct sockaddr *)&addr->ss, &addr->len);
}

int spd_connect(int fd, struct spd_sockaddr * addr)
{
	return connect(fd,(struct sockaddr *)&addr->ss, addr->len);
}

ssize_t spd_recvfrom(int sockfd, void * buf, size_t len, int flags, struct spd_sockaddr *src_addr)
{
	src_addr->len = sizeof(src_addr->ss);
	return recvfrom(sockfd, buf, len, flags, (struct sockaddr *)&src_addr->ss, &src_addr->len);
}

int spd_getsockname(int sockfd, struct spd_sockaddr * addr)
{
	addr->len = sizeof(addr->ss);
	return getsockname(sockfd, (struct sockaddr *)&addr->ss, &addr->len);
}

ssize_t spd_sendto(int sockfd, const void * buf, size_t len, int flags, const struct spd_sockaddr * dest_addr)
{
	return sendto(sockfd, buf, len, flags,(struct sockaddr *)&dest_addr->ss, dest_addr->len);
}

