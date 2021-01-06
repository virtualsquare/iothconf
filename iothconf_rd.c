/*
 *   iothconf_rd.c: auto configuration library for ioth
 *       router discovery protocol (partial implementation of RFC 4861
 *
 *   Copyright 1 Renzo Davoli - Virtual Square Team
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
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/icmp6.h>

#include <ioth.h>
#include <iothconf.h>
#include <iothconf_mod.h>
#include <iothconf_data.h>
#include <iothconf_hash.h>

struct icmp6_LLA_attr {
	uint8_t type;
	uint8_t len;
	uint8_t addr[6];
};

struct in6_addr ll_allrouters = {.s6_addr = {0xff,0x02, [15]=0x02}};

#define RD_TIMEOUT 1000

static int iothconf_rd_proto(struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags) {
	struct {
		struct icmp6_hdr h;
		struct icmp6_LLA_attr l;
	} msg = {
		.h.icmp6_type = ND_ROUTER_SOLICIT,
		.l.type = ND_OPT_SOURCE_LINKADDR,
		.l.len = sizeof(struct icmp6_LLA_attr) / 8,
	};
	time_t ioth_timestamp = ioth_confdata_new_timestamp(stack, ifindex, IOTH_CONFDATA_RD6_TIMESTAMP);
	nlinline_linkgetaddr(ifindex, msg.l.addr);
	struct sockaddr_in6 dst = {
		.sin6_family = AF_INET6,
		.sin6_addr = ll_allrouters,
	};
	struct sockaddr_in6 router;
	socklen_t routerlen = sizeof(router);
	int hoplimit = 255;
	int sd = ioth_msocket(stack, AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	ioth_setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hoplimit, sizeof(hoplimit));
	int rv = ioth_sendto(sd, &msg, sizeof(msg), 0, (void *) &dst, sizeof(dst));

	struct pollfd pfd[] = {{sd, POLLIN, 0}};
	int timeout = RD_TIMEOUT;
	struct timeval start;
	struct timeval end;
	struct timeval timediff;
	for(;;) {
		int event;
		gettimeofday(&start, NULL);
		event = poll(pfd, 1, timeout);
		if (event == 0) {
			ioth_close(sd);
			return errno = ETIME, -1;
		}
		rv = ioth_recvfrom(sd, NULL, 0, MSG_PEEK|MSG_TRUNC, (void *) &router, &routerlen);
		// printf("%d\n", rv);
		uint8_t inbuf[rv];
		rv = ioth_recvfrom(sd, inbuf, rv, 0, (void *) &router, &routerlen);

		uint8_t *limit = inbuf + rv;
		struct nd_router_advert *inh = (void *) inbuf;

		if (inh->nd_ra_type == ND_ROUTER_ADVERT) {
			unsigned char *opt = (void *) (inh + 1);
			ioth_confdata_add_data(stack, ifindex, IOTH_CONFDATA_RD6_ROUTER, ioth_timestamp, 0, struct ioth_confdata_ip6addr,
					.addr = router.sin6_addr,
					.flags = inh->nd_ra_flags_reserved,
					.valid_lifetime = ntohs(inh->nd_ra_router_lifetime));
			while (opt < limit) {
				struct nd_opt_hdr *opth = (void *) opt;
				switch (opth->nd_opt_type)  {
					case ND_OPT_PREFIX_INFORMATION:
						{
							struct nd_opt_prefix_info *this = (void *) opt;
							ioth_confdata_add_data(stack, ifindex, IOTH_CONFDATA_RD6_PREFIX, ioth_timestamp, 0, struct ioth_confdata_ip6addr,
									.addr = this->nd_opt_pi_prefix,
									.flags = this->nd_opt_pi_flags_reserved,
									.prefixlen = this->nd_opt_pi_prefix_len,
									.preferred_lifetime = ntohl(this->nd_opt_pi_preferred_time),
									.valid_lifetime = ntohl(this->nd_opt_pi_valid_time));
							if (config_flags & IOTHCONF_RD_SLAAC && this->nd_opt_pi_prefix_len == 64 &&
									((this->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO) || fqdn != NULL)) {
								struct in6_addr addr = this->nd_opt_pi_prefix;
								if (fqdn != NULL)
									iothconf_hashaddr6(&addr, fqdn);
								else
									iothconf_eui64(&addr, msg.l.addr);
								ioth_confdata_add_data(stack, ifindex, IOTH_CONFDATA_RD6_ADDR, ioth_timestamp, 0, struct ioth_confdata_ip6addr,
										.addr = addr,
										.flags = 0,
										.prefixlen = this->nd_opt_pi_prefix_len,
										.preferred_lifetime = ntohl(this->nd_opt_pi_preferred_time),
										.valid_lifetime = ntohl(this->nd_opt_pi_valid_time));

							}
						}
						break;
					case ND_OPT_MTU:
						{
							struct nd_opt_mtu *this = (void *) opt;
							ioth_confdata_add_data(stack, ifindex, IOTH_CONFDATA_RD6_MTU, ioth_timestamp, 0, uint32_t, this->nd_opt_mtu_mtu);
						}
						break;
				}
				opt += 8 * opth->nd_opt_len;
			}
			ioth_close(sd);
			ioth_confdata_write_timestamp(stack, ifindex, IOTH_CONFDATA_RD6_TIMESTAMP, ioth_timestamp);
			return 0;
		}
		gettimeofday(&end, NULL);
		timersub(&end, &start, &timediff);
		timeout -= timediff.tv_sec * 1000 + timediff.tv_usec / 1000;
		if (timeout < 0) timeout = 0;
	}
}

int iothconf_rd(struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags) {
	int rv = iothconf_rd_proto(stack, ifindex, fqdn, config_flags);
	if (rv == 0)
		iothconf_ip_update(stack, ifindex, IOTH_CONFDATA_RD6_TIMESTAMP, config_flags);
	return rv;
}
