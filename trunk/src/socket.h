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
  * \brief network socket utils, suport ipv4 and ipv6
  * refer to http://www.retran.com/beej/ip4to6.html
  */
  
#ifndef _SPIDER_SOCKET_H
#define _SPIDERSOCKET_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#define SPD_SOCKADDR_STR_ADDR		(1 << 0)
#define SPD_SOCKADDR_STR_PORT		(1 << 1)
#define SPD_SOCKADDR_STR_BRACKETS	(1 << 2)
#define SPD_SOCKADDR_STR_REMOTE		(1 << 3)
#define SPD_SOCKADDR_STR_HOST		(SPD_SOCKADDR_STR_ADDR | SPD_SOCKADDR_STR_BRACKETS)
#define SPD_SOCKADDR_STR_DEFAULT	(SPD_SOCKADDR_STR_ADDR | SPD_SOCKADDR_STR_PORT)
#define SPD_SOCKADDR_STR_ADDR_REMOTE     (SPD_SOCKADDR_STR_ADDR | SPD_SOCKADDR_STR_REMOTE)
#define SPD_SOCKADDR_STR_HOST_REMOTE     (SPD_SOCKADDR_STR_HOST | SPD_SOCKADDR_STR_REMOTE)
#define SPD_SOCKADDR_STR_DEFAULT_REMOTE  (SPD_SOCKADDR_STR_DEFAULT | SPD_SOCKADDR_STR_REMOTE)
#define SPD_SOCKADDR_STR_FORMAT_MASK     (SPD_SOCKADDR_STR_ADDR | SPD_SOCKADDR_STR_PORT | SPD_SOCKADDR_STR_BRACKETS)

enum {
	SPD_AF_UNSPEC = -2,
	SPD_AF_INET = 1,
	SPD_AF_INET6,
};

enum {
	PARSE_PORT_MASK =	0x0300, /* 0x000: accept port if present */
	PARSE_PORT_IGNORE =	0x0100, /* 0x100: ignore port if present */
	PARSE_PORT_REQUIRE = 0x0200, /* 0x200: require port number */
	PARSE_PORT_FORBID =	0x0300, /* 0x100: forbid port number */
};

struct spd_sockaddr {
	struct sockaddr_storage ss;
	socklen_t len;
};

/*!
 *
 * \brief
 * Checks if the spd_sockaddr is null. "null" in this sense essentially
 * means uninitialized, or having a 0 length.
 *
 * \param addr Pointer to the spd_sockaddr we wish to check
 * \retval 1 \a addr is null
 * \retval 0 \a addr is non-null.
 */
static inline int spd_sockaddr_isnull(const struct spd_sockaddr *ss)
{
	return !ss || ss->len == 0;
}

/*!
 *
 * \brief
 * Sets address \a ss to null.
 *
 * \retval void
 */
static inline void spd_sockaddr_setnull(struct spd_sockaddr *ss)
{
	ss->len = 0;
}

/*!
 *
 * \brief
 * Copies the data from one spd_sockaddr to another
 *
 * \param dst The destination spd_sockaddr
 * \param src The source spd_sockaddr
 * \retval void
 */
static inline void spd_sockaddr_copy(struct spd_sockaddr *dst, const struct spd_sockaddr *src)
{
	memcpy(dst, src, src->len);
	dst->len = src->len;
}

/*!
 *
 * \brief
 * Compares two spd_sockaddr structures
 *
 * \retval -1 \a a is lexicographically smaller than \a b
 * \retval 0 \a a is equal to \a b
 * \retval 1 \a b is lexicographically smaller than \a a
 */
int spd_sockaddr_cmp(const struct spd_sockaddr *a, const struct spd_sockaddr *b);

/*!
 *
 * \brief
 * Compares the addresses of two ast_sockaddr structures.
 *
 * \retval -1 \a a is lexicographically smaller than \a b
 * \retval 0 \a a is equal to \a b
 * \retval 1 \a b is lexicographically smaller than \a a
 */
int spd_sockaddr_cmp_addr(const struct spd_sockaddr *a, const struct spd_sockaddr *b);

/*!
 *
 * \brief
 * Convert a socket address to a string.
 *
 * \details
 * This will be of the form a.b.c.d:xyz
 * for IPv4 and [a:b:c:...:d]:xyz for IPv6.
 *
 * This function is thread-safe. The returned string is on static
 * thread-specific storage.
 *
 * \param addr The input to be stringified
 * \param format one of the following:
 * SPD_SOCKADDR_STR_DEFAULT:
 *    a.b.c.d:xyz for IPv4
 *    [a:b:c:...:d]:xyz for IPv6.
 * SPD_SOCKADDR_STR_ADDR: address only
 *    a.b.c.d for IPv4
 *    a:b:c:...:d for IPv6.
 * SPD_SOCKADDR_STR_HOST: address only, suitable for a URL
 *    a.b.c.d for IPv4
 *    [a:b:c:...:d] for IPv6.
 * SPD_SOCKADDR_STR_PORT: port only
 * \retval "(null)" \a addr is null
 * \retval "" An error occurred during processing
 * \retval string The stringified form of the address
 */
char *spd_sockaddr_tostring_fmt(const struct spd_sockaddr *ss, int fmt);

/*
  * wraper for spd_sockaddr_tostring_fmt, return default format string(addr:port)
  */
static inline char *spd_sockaddr_tostring_default(const struct spd_sockaddr *ss)
{
	return spd_sockaddr_tostring_fmt(ss, SPD_SOCKADDR_STR_DEFAULT);	
}

/*!
 * \since 1.8
 *
 * \brief
 * Wrapper around spd_sockaddr_stringify_fmt() with default format
 *
 * \return same as spd_sockaddr_stringify_fmt()
 */
static inline char *spd_sockaddr_tostring_remote(const struct spd_sockaddr *addr)
{
	return spd_sockaddr_tostring_fmt(addr, SPD_SOCKADDR_STR_DEFAULT_REMOTE);
}

/*!
 *
 * \brief
 * Wrapper around spd_sockaddr_stringify_fmt() with default format
 *
 * \note This address will be suitable for passing to a remote machine via the
 * application layer. For example, the scope-id on a link-local IPv6 address
 * will be stripped.
 *
 * \return same as spd_sockaddr_stringify_fmt()
 */
static inline char*spd_sockaddr_tostring_addr(const struct spd_sockaddr *ss)
{
	return spd_sockaddr_tostring_fmt(ss, SPD_SOCKADDR_STR_ADDR);
}

/*!
 * 
 * \brief
 * Wrapper around spd_sockaddr_stringify_fmt() to return an address only
 *
 * \note This address will be suitable for passing to a remote machine via the
 * application layer. For example, the scope-id on a link-local IPv6 address
 * will be stripped.
 *
 * \return same as spd_sockaddr_stringify_fmt()
 */
static inline char *spd_sockaddr_tostring_addr_remote(const struct spd_sockaddr *ss)
{
	return spd_sockaddr_tostring_fmt(ss, SPD_SOCKADDR_STR_ADDR_REMOTE);
}

/*!
 *
 * \brief
 * Wrapper around spd_sockaddr_stringify_fmt() to return an address only,
 * suitable for a URL (with brackets for IPv6).
 *
 * \return same spd_sockaddr_stringify_fmt()
 */
static inline char *spd_sockaddr_tostring_host(const struct spd_sockaddr *addr)
{
	return spd_sockaddr_tostring_fmt(addr, SPD_SOCKADDR_STR_HOST);
}

/*!
 * \since 1.8
 *
 * \brief
 * Wrapper around spd_sockaddr_tostring_fmt() to return an address only,
 * suitable for a URL (with brackets for IPv6).
 *
 * \note This address will be suitable for passing to a remote machine via the
 * application layer. For example, the scope-id on a link-local IPv6 address
 * will be stripped.
 *
 * \return same as spd_sockaddr_tostring_fmt()
 */
static inline char *spd_sockaddr_tostring_host_remote(const struct spd_sockaddr *addr)
{
	return spd_sockaddr_tostring_fmt(addr, SPD_SOCKADDR_STR_HOST_REMOTE);
}

/*!
 * \since 1.8
 *
 * \brief
 * Wrapper around spd_sockaddr_tostring_fmt() to return a port only
 *
 * \return same as spd_sockaddr_tostring_fmt()
 */
static inline char *spd_sockaddr_tostring_port(const struct spd_sockaddr *addr)
{
	return spd_sockaddr_tostring_fmt(addr, SPD_SOCKADDR_STR_PORT);
}

/*!
 *
 * \brief
 * Splits a string into its host and port components
 *
 * \param str[in]   The string to parse. May be modified by writing a NUL at the end of
 *                  the host part.
 * \param host[out] Pointer to the host component within \a str.
 * \param port[out] Pointer to the port component within \a str.
 * \param flags     If set to zero, a port MAY be present. If set to PARSE_PORT_IGNORE, a
 *                  port MAY be present but will be ignored. If set to PARSE_PORT_REQUIRE,
 *                  a port MUST be present. If set to PARSE_PORT_FORBID, a port MUST NOT
 *                  be present.
 *
 * \retval 1 Success
 * \retval 0 Failure
 */

int spd_sockaddr_stringto_hostport(char *str, char **host, char **port, int flags);

/*!
 *
 * \brief
 * Parse an IPv4 or IPv6 address string to spd_sockaddr.
 *
 * \details
 * Parses a string containing an IPv4 or IPv6 address followed by an optional
 * port (separated by a colon) into a struct spd_sockaddr. The allowed formats
 * are the following:
 *
 * a.b.c.d
 * a.b.c.d:port
 * a:b:c:...:d
 * [a:b:c:...:d]
 * [a:b:c:...:d]:port
 *
 * Host names are NOT allowed.
 *
 * \param[out] addr The resulting ast_sockaddr
 * \param str The string to parse
 * \param flags If set to zero, a port MAY be present. If set to
 * PARSE_PORT_IGNORE, a port MAY be present but will be ignored. If set to
 * PARSE_PORT_REQUIRE, a port MUST be present. If set to PARSE_PORT_FORBID, a
 * port MUST NOT be present.
 *
 * \retval 1 Success
 * \retval 0 Failure
 */
int spd_sockaddr_parse(struct spd_sockaddr *ss, const char *str, int flags);

/*!
 *
 * \brief
 * Parses a string with an IPv4 or IPv6 address and place results into an array
 *
 * \details
 * Parses a string containing a host name or an IPv4 or IPv6 address followed
 * by an optional port (separated by a colon).  The result is returned into a
 * array of struct spd_sockaddr. Allowed formats for str are the following:
 *
 * hostname:port
 * host.example.com:port
 * a.b.c.d
 * a.b.c.d:port
 * a:b:c:...:d
 * [a:b:c:...:d]
 * [a:b:c:...:d]:port
 *
 * \param[out] addrs The resulting array of ast_sockaddrs
 * \param str The string to parse
 * \param flags If set to zero, a port MAY be present. If set to
 * PARSE_PORT_IGNORE, a port MAY be present but will be ignored. If set to
 * PARSE_PORT_REQUIRE, a port MUST be present. If set to PARSE_PORT_FORBID, a
 * port MUST NOT be present.
 *
 * \param family Only addresses of the given family will be returned. Use 0 or
 * SPD_SOCKADDR_UNSPEC to get addresses of all families.
 *
 * \retval 0 Failure
 * \retval non-zero The number of elements in addrs array.
 */
int spd_sockaddr_resolve(struct spd_sockaddr **addrs, const char *str,
			 int flags, int family);

/*!
 *
 * \brief
 * Get the port number of a socket address.
 *
 * \warning Do not use this function unless you really know what you are doing.
 * And "I want the port number" is not knowing what you are doing.
 *
 * \retval 0 Address is null
 * \retval non-zero The port number of the spd_sockaddr
 */
#define spd_sockaddr_get_port(addr) _spd_sockaddr_get_port(addr,__FILE__, __LINE__, __PRETTY_FUNCTION__)

unsigned char  _spd_sockaddr_get_port(struct spd_sockaddr *ss, const char *file, int line, const char *function);


/*!
 *
 * \brief
 * Sets the port number of a socket address.
 *
 * \warning Do not use this function unless you really know what you are doing.
 * And "I want the port number" is not knowing what you are doing.
 *
 * \param addr Address on which to set the port
 * \param port The port you wish to set the address to use
 * \retval void
 */
#define spd_sockaddr_set_port(addr, port) _spd_sockaddr_set_port(addr, port, __FILE__, __LINE__, __PRETTY_FUNCTION__)

void _spd_sockaddr_set_port(struct spd_sockaddr *ss, uint16_t port, const char *file, int line, const char *function);

/*!
 *
 * \brief
 * Get an IPv4 address of an spd_sockaddr
 *
 * \warning You should rarely need this function. Only use if you know what
 * you're doing.
 * \return IPv4 address in network byte order
 */
uint32_t spd_sockaddr_ipv4(const struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Determine if the address is an IPv4 address
 *
 * \warning You should rarely need this function. Only use if you know what
 * you're doing.
 * \retval 1 This is an IPv4 address
 * \retval 0 This is an IPv6 or IPv4-mapped IPv6 address
 */
int spd_sockaddr_is_ipv4(const struct spd_sockaddr *addr);

/*!
 * \brief
 * Convert an IPv4-mapped IPv6 address into an IPv4 address.
 *
 * \warning You should rarely need this function. Only call this
 * if you know what you're doing.
 *
 * \param addr The IPv4-mapped address to convert
 * \param mapped_addr The resulting IPv4 address
 * \retval 0 Unable to make the conversion
 * \retval 1 Successful conversion
 */
int spd_sockaddr_ipv4_mapped(const struct spd_sockaddr *addr, struct spd_sockaddr *spd_mapped);

/*!
 *
 * \brief
 * Determine if this is an IPv4-mapped IPv6 address
 *
 * \warning You should rarely need this function. Only use if you know what
 * you're doing.
 *
 * \retval 1 This is an IPv4-mapped IPv6 address.
 * \retval 0 This is not an IPv4-mapped IPv6 address.
 */
int spd_sockaddr_is_ipv4_mapped(const struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Determine if an IPv4 address is a multicast address
 *
 * \parm addr the address to check
 *
 * This function checks if an address is in the 224.0.0.0/4 network block.
 *
 * \return non-zero if this is a multicast address
 */
int spd_sockaddr_is_ipv4_multicast(const struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Determine if this is a link-local IPv6 address
 *
 * \warning You should rarely need this function. Only use if you know what
 * you're doing.
 *
 * \retval 1 This is a link-local IPv6 address.
 * \retval 0 This is link-local IPv6 address.
 */
int spd_sockaddr_is_ipv6_link_local(const struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Determine if this is an IPv6 address
 *
 * \warning You should rarely need this function. Only use if you know what
 * you're doing.
 *
 * \retval 1 This is an IPv6 or IPv4-mapped IPv6 address.
 * \retval 0 This is an IPv4 address.
 */
int spd_sockaddr_is_ipv6(const struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Determine if the address type is unspecified, or "any" address.
 *
 * \details
 * For IPv4, this would be the address 0.0.0.0, and for IPv6,
 * this would be the address ::. The port number is ignored.
 *
 * \retval 1 This is an "any" address
 * \retval 0 This is not an "any" address
 */
int spd_sockaddr_is_any(const struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Computes a hash value from the address. The port is ignored.
 *
 * \retval 0 Unknown address family
 * \retval other A 32-bit hash derived from the address
 */
int spd_sockaddr_hash(const struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Wrapper around accept(2) that uses struct spd_sockaddr.
 *
 * \details
 * For parameter and return information, see the man page for
 * accept(2).
 */
int spd_accept(int fd, struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Wrapper around bind(2) that uses struct spd_sockaddr.
 *
 * \details
 * For parameter and return information, see the man page for
 * bind(2).
 */
int spd_bind(int fd, struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Wrapper around listen(2) that uses struct spd_sockaddr.
 *
 * \details
 * For parameter and return information, see the man page for
 * listen(2).
 */
int spd_listen(int fd, int backlog);

/*!
 *
 * \brief
 * Wrapper around connect(2) that uses struct spd_sockaddr.
 *
 * \details
 * For parameter and return information, see the man page for
 * connect(2).
 */
int spd_connect(int fd, struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Wrapper around getsockname(2) that uses struct ast_sockaddr.
 *
 * \details
 * For parameter and return information, see the man page for
 * getsockname(2).
 */
int spd_getsockname(int sockfd, struct spd_sockaddr *addr);

/*!
 *
 * \brief
 * Wrapper around recvfrom(2) that uses struct ast_sockaddr.
 *
 * \details
 * For parameter and return information, see the man page for
 * recvfrom(2).
 */
ssize_t spd_recvfrom(int sockfd, void *buf, size_t len, int flags,
		     struct spd_sockaddr *src_addr);

/*!
 *
 * \brief
 * Wrapper around sendto(2) that uses ast_sockaddr.
 *
 * \details
 * For parameter and
 * return information, see the man page for sendto(2)
 */
ssize_t spd_sendto(int sockfd, const void *buf, size_t len, int flags,
		   const struct spd_sockaddr *dest_addr);

/*!
 *
 * \brief
 * Set type of service
 *
 * \details
 * Set ToS ("Type of Service for IPv4 and "Traffic Class for IPv6) and
 * CoS (Linux's SO_PRIORITY)
 *
 * \param sockfd File descriptor for socket on which to set the parameters
 * \param tos The type of service for the socket
 * \param cos The cost of service for the socket
 * \param desc A text description of the socket in question.
 * \retval 0 Success
 * \retval -1 Error, with errno set to an appropriate value
 */
int spd_set_qos(int sockfd, int tos, int cos, const char *desc);


/*!
 * These are backward compatibility functions that may be used by subsystems
 * that have not yet been converted to IPv6. They will be removed when all
 * subsystems are IPv6-ready.
 */
/*@{*/

/*!
 *
 * \brief
 * Converts a struct spd_sockaddr to a struct sockaddr_in.
 *
 * \param addr The spd_sockaddr to convert
 * \param[out] sin The resulting sockaddr_in struct
 * \retval nonzero Success
 * \retval zero Failure
 */
#define spd_sockaddr_to_sin(addr,sin)	_spd_sockaddr_to_sin(addr,sin, __FILE__, __LINE__, __PRETTY_FUNCTION__)
int _spd_sockaddr_to_sin(const struct spd_sockaddr *addr,
			struct sockaddr_in *sin, const char *file, int line, const char *func);

/*!
 *
 * \brief
 * Converts a struct sockaddr_in to a struct ast_sockaddr.
 *
 * \param sin The sockaddr_in to convert
 * \return an spd_sockaddr structure
 */
#define spd_sockaddr_from_sin(addr,sin)	_spd_sockaddr_from_sin(addr,sin, __FILE__, __LINE__, __PRETTY_FUNCTION__)
void _spd_sockaddr_from_sin(struct spd_sockaddr *addr, const  struct sockaddr_in *sin,
		const char *file, int line, const char *func);

#if defined(__cplusplus) || defined(c_pluseplus)
}
#endif

#endif
