/*
 *   iothconf_dhcpv6.c: auto donfiguration library for ioth
 *       dhcpv6: partial implementation of RFC 8415 and 4704 (fqdn)
 *
 *   Copyright 2021 Renzo Davoli - Virtual Square Team
 *   University of Bologna - Italy
 *
 *   This library is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or (at
 *   your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <poll.h>
#include <endian.h>
#include <sys/socket.h>
#include <sys/random.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <ioth.h>
#include <iothconf.h>
#include <iothconf_mod.h>
#include <iothconf_hash.h>
#include <iothconf_data.h>
#include <iothconf_dns.h>

#define   DHCP_CLIENTPORT   546
#define   DHCP_SERVERPORT   547
#define   DHCP_SOLICIT  1
#define   DHCP_ADVERTISE  2
#define   DHCP_REQUEST  3
#define   DHCP_CONFIRM  4
#define   DHCP_RENEW  5
#define   DHCP_REBIND   6
#define   DHCP_REPLY  7
#define   DHCP_RELEASE  8
#define   DHCP_DECLINE  9
#define   DHCP_RECONFIGURE  10

#define   OPTION_CLIENTID   1
#define   OPTION_SERVERID   2
#define   OPTION_IA_NA  3
#define   OPTION_IA_TA  4
#define   OPTION_IAADDR  5
#define   OPTION_ORO  6
#define   OPTION_PREFERENCE   7
#define   OPTION_ELAPSED_TIME   8
#define   OPTION_RELAY_MSG  9
#define   OPTION_AUTH   11
#define   OPTION_UNICAST  12
#define   OPTION_STATUS_CODE  13
#define   OPTION_RAPID_COMMIT   14
#define   OPTION_USER_CLASS   15
#define   OPTION_VENDOR_CLASS   16
#define   OPTION_VENDOR_OPTS  17
#define   OPTION_INTERFACE_ID   18
#define   OPTION_RECONF_MSG   19
#define   OPTION_RECONF_ACCEPT  20
#define   OPTION_DNS_SERVERS  23
#define   OPTION_DOMAIN_LIST  24
#define   OPTION_IAPREFIX   26
#define   OPTION_SNTP_SERVERS   31
#define   OPTION_CLIENT_FQDN  39

#define MCAST_ALL_ROUTERS \
{0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x02}

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define HTONS(x) ((__uint16_t) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)))
# define NTOHS(x) ((__uint16_t) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)))
#else
# define HTONS(x) (x)
# define NTOHS(x) (x)
#endif

static struct sockaddr_in6 mcastaddr = {
	.sin6_family       = AF_INET6,
	.sin6_port         = HTONS(DHCP_SERVERPORT),
	.sin6_addr.s6_addr = MCAST_ALL_ROUTERS
};

static inline void fput_int8(FILE *f, uint8_t data) {
	fputc(data, f);
}

static inline void fput_int16(FILE *f, uint16_t data) {
	fputc(data >> 8, f);
	fputc(data, f);
}

static inline void fput_int32(FILE *f, uint32_t data) {
	fputc(data >> 24, f);
	fputc(data >> 16, f);
	fputc(data >> 8, f);
	fputc(data, f);
}

static inline void fput_data(FILE *f, void *data, uint16_t len) {
	fwrite(data, len, 1, f);
}

/* fget_int8 and fget_int16 return EOF */
static inline int fget_int8(FILE *f) {
	return fgetc(f);
}

static inline int fget_int16(FILE *f) {
	return (fget_int8(f) << 8) | fget_int8(f);
}

static inline uint32_t fget_int32(FILE *f) {
	return (fget_int8(f) << 24) | (fget_int8(f) << 16) |
		(fget_int8(f) << 8) | fget_int8(f);
}

static inline void fget_data(FILE *f, void *data, uint16_t len) {
	size_t retval = fread(data, len, 1, f);
	(void) retval;
}

struct iaaddr {
	uint16_t type;
	uint16_t len;
	uint8_t addr[16];
	uint32_t preferred_lifetime;
	uint32_t valid_lifetime;
};

static void ia_lifetime_zero(uint8_t *iana_addr, uint16_t iana_addrlen) {
	uint8_t *limit = iana_addr + iana_addrlen;
	while (iana_addr < limit) {
		struct iaaddr *iaaddr = (void *) iana_addr;
		if (NTOHS(iaaddr->type) == OPTION_IAADDR)
			iaaddr->preferred_lifetime = iaaddr-> valid_lifetime = 0;
		iana_addr += sizeof(uint16_t) + sizeof(uint16_t) + NTOHS(iaaddr->len);
	}
}

static unsigned int lname2dns(const char *name, char *out) {
	unsigned int len = 0;
	while (*name) {
		char *itemlen = out++;
		while (*name != 0 && *name != '.')
			*out++ = *name++;
		if (*name == '.') name++;
		*itemlen = out - itemlen - 1;
		len += (*itemlen) + 1;
	}
	*out=0;
	return len + 1;
}

#define time2000101 946684800
static time_t idtime(void) {
	static time_t duidtime;
	if (duidtime == 0) duidtime = time(NULL) - time2000101;
	return duidtime;
}

static void dhcp_add_head(FILE *f, int type, uint8_t *tid) {
	fput_int8(f, type);
	fput_data(f, tid, 3);
}

static void dhcp_add_option(FILE *f, int type, int len) {
	fput_int16(f, type);
	fput_int16(f, len);
}

static void dhcp_add_opt_clientid(FILE *f, uint8_t *macaddr) {
	dhcp_add_option(f, OPTION_CLIENTID, 14);
	fput_int16(f, 1);
	fput_int16(f, 1);
	fput_int32(f, idtime());
	fput_data(f, macaddr, ETH_ALEN);
}

static void dhcp_add_opt_serverid(FILE *f, uint8_t *serverid, uint16_t serveridlen) {
	if (serverid) {
		dhcp_add_option(f, OPTION_SERVERID, serveridlen);
		fput_data(f, serverid, serveridlen);
	}
}

static void dhcp_add_opt_oro(FILE *f, ...) {
	int len;
	va_list ap;
	va_start(ap, f);
	for (len = 0; va_arg(ap, unsigned int) != 0; len++)
		;
	va_end(ap);
	dhcp_add_option(f, OPTION_ORO, sizeof(uint16_t) * len);
	va_start(ap, f);
	for (;;) {
		uint8_t opt = va_arg(ap, unsigned int);
		if (opt == 0) break;
		fput_int16(f, opt);
	}
	va_end(ap);
}

static void dhcp_add_opt_elapsed_time(FILE *f, int value) {
	dhcp_add_option(f, OPTION_ELAPSED_TIME, 2);
	fput_int16(f, value);
}

static void dhcp_add_opt_fqdn(FILE *f, const char *fqdn, uint8_t flags) {
	if (fqdn && *fqdn) {
		char out[strlen(fqdn) + 2];
		int len = lname2dns(fqdn, out);
		dhcp_add_option(f, OPTION_CLIENT_FQDN, 1 + len);
		fput_int8(f, flags);
		fput_data(f, out, len);
	}
}

static void dhcp_add_opt_iana(FILE *f, uint8_t *macaddr, uint8_t *iana_addr, uint16_t iana_addrlen) {
	dhcp_add_option(f, OPTION_IA_NA, 12);
	fput_data(f, macaddr+2, 4);
	fput_int32(f, 0); /* section 25 RFC 8415 */
	fput_int32(f, 0);
	if (iana_addr)
		fput_data(f, iana_addr, iana_addrlen);
}

struct dhcpdata {
	struct ioth *stack;
	unsigned int ifindex;
	time_t timestamp;
	uint8_t tid[3];
	uint8_t macaddr[ETH_ALEN];
	const char *fqdn;
	uint8_t *serverid;
	uint16_t serveridlen;
	uint8_t *iana_addr;
	uint16_t iana_addrlen;
};

static int dhcp_get(int sendtype, int fd, struct dhcpdata *data);
static int dhcp_send(int type, int fd, struct dhcpdata *data) {
	char *buf;
	size_t buflen = 0;
	FILE *f = open_memstream(&buf, &buflen);
	if (getrandom(data->tid, sizeof(data->tid), 0) < 0)
		return -1;
	ia_lifetime_zero(data->iana_addr, data->iana_addrlen);
	dhcp_add_head(f, type, data->tid);
	dhcp_add_opt_clientid(f, data->macaddr);
	dhcp_add_opt_serverid(f, data->serverid,  data->serveridlen);
	dhcp_add_opt_oro(f, OPTION_DNS_SERVERS, OPTION_DOMAIN_LIST, 0);
	dhcp_add_opt_elapsed_time(f, 0);
	dhcp_add_opt_fqdn(f, data->fqdn, 0);
	dhcp_add_opt_iana(f, data->macaddr, data->iana_addr, data->iana_addrlen);
	fclose(f);
	int times = 0;
	for (;;) {
		if (ioth_sendto(fd, buf, buflen, 0, (struct sockaddr *) &mcastaddr, sizeof(mcastaddr)) < 0)
			goto err;
		if (dhcp_get(type, fd, data) == 0)
			break;
		if (times >= 2 || errno != ETIME)
			goto err;
		times++;
	}
	free(buf);
	return 0;
err:
	free(buf);
	return -1;
}

static int check_consistency(int type, uint8_t *inbuf, size_t inbuflen, struct dhcpdata *data) {
	if (inbuflen < 4) return 0;
	if (type != inbuf[0]) return 0;
	if (memcmp(inbuf+1, data->tid, 3) != 0) return 0;
	return 1;
}

static int check_clientid(FILE *f, uint16_t len, uint8_t *macaddr) {
	uint32_t cmp_data;
	uint8_t cmp_macaddr[ETH_ALEN];
	if (len != 14) return 0;
	cmp_data = fget_int16(f);
	if (cmp_data != 1) return 0;
	cmp_data = fget_int16(f);
	if (cmp_data != 1) return 0;
	cmp_data = fget_int32(f);
	if (cmp_data != idtime()) return 0;
	fget_data(f, cmp_macaddr, ETH_ALEN);
	if (memcmp(cmp_macaddr, macaddr, ETH_ALEN) != 0) return 0;
	return 1;
}

static int check_iana(FILE *f, uint16_t len, uint8_t *macaddr) {
	uint8_t cmp_iaid[4];
	if (len < 12) return 0;
	fget_data(f, cmp_iaid, 4);
	if (memcmp(cmp_iaid, macaddr + 2, 4) != 0) return 0;
	fget_int32(f);
	fget_int32(f);
	return 1;
}

#define DHCP_TIMEOUT 2000
static int dhcp_get(int sendtype, int fd, struct dhcpdata *data) {
	struct pollfd pfd[] = {{fd, POLLIN, 0}};
	int timeout = DHCP_TIMEOUT;
	struct timeval start;
	struct timeval end;
	struct timeval timediff;
	int type;
	switch (sendtype) {
		case DHCP_SOLICIT: type = DHCP_ADVERTISE; break;
		case DHCP_REQUEST: type = DHCP_REPLY; break;
		case DHCP_CONFIRM: type = DHCP_REPLY; break;
		case DHCP_RENEW: type = DHCP_REPLY; break;
		default: return errno = EINVAL, -1;
	}
	for(;;) {
		gettimeofday(&start, NULL);
		int event = poll(pfd, 1, timeout);
		if (event == 0)
			return errno = ETIME, -1;
		size_t inbuflen = ioth_recv(fd, NULL, 0, MSG_PEEK|MSG_TRUNC);
		uint8_t inbuf[inbuflen];
		inbuflen = ioth_recv(fd, inbuf, inbuflen, 0);
		if (check_consistency(type, inbuf, inbuflen, data)) {
			uint8_t *dns_serv_addr = NULL;
			uint8_t *dns_search_addr = NULL;
			uint16_t dns_serv_len = 0;
			uint16_t dns_search_len = 0;
			uint8_t *optbuf = inbuf + 4;
			size_t optbuflen = inbuflen - 4;
			int ok = 1;
			FILE *optf = fmemopen(optbuf, optbuflen, "r");
			/* first scan */
			for(;;) {
				int opt_type = fget_int16(optf);
				if (opt_type == EOF)
					break;
				unsigned int opt_len = fget_int16(optf);
				long next_opt = ftell(optf) + opt_len;
				//printf("%d\n", opt_type);
				switch(opt_type) {
					case OPTION_CLIENTID:
						ok = check_clientid(optf, opt_len, data->macaddr);
						break;
					case OPTION_SERVERID:
						data->serverid = optbuf + ftell(optf);
						data->serveridlen = opt_len;
						break;
					case OPTION_IA_NA:
						ok = check_iana(optf, opt_len, data->macaddr);
						if (ok) {
							data->iana_addr = optbuf + ftell(optf);
							data->iana_addrlen = opt_len - 12;
						}
						break;
					case OPTION_DNS_SERVERS:
						dns_serv_addr = optbuf + ftell(optf);
						dns_serv_len = opt_len;
						break;
					case OPTION_DOMAIN_LIST:
						dns_search_addr = optbuf + ftell(optf);
						dns_search_len = opt_len;
						break;
				}
				fseek(optf, next_opt, SEEK_SET);
			}
			fclose(optf);
			if (ok) {
				if (type == DHCP_ADVERTISE)
					return dhcp_send(DHCP_REQUEST, fd, data);
				else {
					ioth_confdata_add(data->stack, data->ifindex, IOTH_CONFDATA_DHCP6_SERVERID, data->timestamp, 0,
							data->serverid, data->serveridlen);
					if (data->iana_addr != NULL) {
						FILE *ianaf = fmemopen(data->iana_addr, data->iana_addrlen, "r");
						for(;;) {
							int opt_type = fget_int16(ianaf);
							if (opt_type == EOF)
								break;
							unsigned int opt_len = fget_int16(ianaf);
							long next_opt = ftell(ianaf) + opt_len;
							//printf("iana %d\n", opt_type);
							switch(opt_type) {
								case OPTION_IAADDR:
									if (opt_len >= 24) {
										struct ioth_confdata_ip6addr iaaddr = {.prefixlen = 128};
										fget_data(ianaf, &iaaddr.addr, sizeof(iaaddr.addr));
										iaaddr.preferred_lifetime = fget_int32(ianaf);
										iaaddr.valid_lifetime = fget_int32(ianaf);
										ioth_confdata_add(data->stack, data->ifindex, IOTH_CONFDATA_DHCP6_ADDR, data->timestamp, 0,
												&iaaddr, sizeof(iaaddr));
									}
									break;
							}
							fseek(ianaf, next_opt, SEEK_SET);
						}
						fclose(ianaf);
					}
					/* dns server/dns search */
					if (dns_serv_addr != NULL) /* list of ip addrs RFC 3646 */
						ioth_confdata_add(data->stack, data->ifindex, IOTH_CONFDATA_DHCP6_DNS, data->timestamp, 0,
								dns_serv_addr, dns_serv_len);
					if (dns_search_addr != NULL) { /* list of domains in RFC1035 fmt */
						char dns_search_mstr[dns_search_len];
						int mstr_len = iothconf_domain2mstr(dns_search_addr, dns_search_mstr, dns_search_len);
						ioth_confdata_add(data->stack, data->ifindex, IOTH_CONFDATA_DHCP6_DOMAIN, data->timestamp, 0,
                dns_search_mstr, mstr_len);
					}
					ioth_confdata_write_timestamp(data->stack, data->ifindex, IOTH_CONFDATA_DHCP6_TIMESTAMP, data->timestamp);
					return 0;
				}
			}
		}
		gettimeofday(&end, NULL);
		timersub(&end, &start, &timediff);
		timeout -= timediff.tv_sec * 1000 + timediff.tv_usec / 1000;
		if (timeout < 0) timeout = 0;
	}
}

static int iothconf_dhcpv6_proto(struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags) {
	(void) config_flags;
	static struct sockaddr_in6 bindaddr = {
		.sin6_family      = AF_INET6,
		.sin6_port        = HTONS(DHCP_CLIENTPORT)
	};
	struct dhcpdata dhcpdata = {
		.stack = stack,
		.ifindex = ifindex,
		.timestamp = ioth_confdata_new_timestamp(stack, ifindex, IOTH_CONFDATA_DHCP6_TIMESTAMP),
		.fqdn = fqdn};
	int fd = ioth_msocket(stack, AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	ioth_bind(fd, (struct sockaddr *) &bindaddr, sizeof(bindaddr));
	ioth_linkgetaddr(stack, ifindex, dhcpdata.macaddr);
	int retval = dhcp_send(DHCP_SOLICIT, fd, &dhcpdata);
	ioth_close(fd);
	return retval;
}

int iothconf_dhcpv6(struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags) {
	int rv = iothconf_dhcpv6_proto(stack, ifindex, fqdn, config_flags);
	if (rv == 0)
		iothconf_ip_update(stack, ifindex, IOTH_CONFDATA_DHCP6_TIMESTAMP, config_flags);
	return rv;
}


