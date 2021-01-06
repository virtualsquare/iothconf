/*
 *   iothconf_debug.c: debug: print current db.
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
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <arpa/inet.h>

#include <iothconf.h>
#include <iothconf_data.h>
#include <iothconf_mod.h>

#define STRTIMESTAMPLEN 128
static char *strtimestamp(time_t timestamp,  char *buf) {
	struct tm tm;
	gmtime_r(&timestamp, &tm);
	strftime(buf, STRTIMESTAMPLEN, "%Y%m%d %H%M%S", &tm);
	return buf;
}

static const char *strtype(uint8_t type) {
	switch(type) {
		case IOTH_CONFDATA_STATIC_TIMESTAMP : return "sxt";
		case IOTH_CONFDATA_STATIC4_ADDR     : return "s4a";
		case IOTH_CONFDATA_STATIC4_ROUTE    : return "s4r";
		case IOTH_CONFDATA_STATIC6_ADDR     : return "s6a";
		case IOTH_CONFDATA_STATIC6_ROUTE    : return "s6r";
		case IOTH_CONFDATA_STATIC4_DNS      : return "s4d";
		case IOTH_CONFDATA_STATIC6_DNS      : return "s6d";
		case IOTH_CONFDATA_STATIC_DOMAIN    : return "sxs";

		case IOTH_CONFDATA_DHCP4_TIMESTAMP  : return "d4t";
		case IOTH_CONFDATA_DHCP4_SERVER     : return "d4S";
		case IOTH_CONFDATA_DHCP4_ADDR       : return "d4a";
		case IOTH_CONFDATA_DHCP4_ROUTER     : return "d4r";
		case IOTH_CONFDATA_DHCP4_DNS        : return "d4d";
		case IOTH_CONFDATA_DHCP4_DOMAIN     : return "d4s";

		case IOTH_CONFDATA_DHCP6_TIMESTAMP  : return "d6t";
		case IOTH_CONFDATA_DHCP6_SERVERID   : return "d6S";
		case IOTH_CONFDATA_DHCP6_ADDR       : return "d6a";
		case IOTH_CONFDATA_DHCP6_DNS        : return "d6d";
		case IOTH_CONFDATA_DHCP6_DOMAIN     : return "d6s";

		case IOTH_CONFDATA_RD6_TIMESTAMP    : return "r6t";
		case IOTH_CONFDATA_RD6_PREFIX       : return "r6p";
		case IOTH_CONFDATA_RD6_ADDR         : return "r6a";
		case IOTH_CONFDATA_RD6_ROUTER       : return "r6r";
		case IOTH_CONFDATA_RD6_MTU          : return "r6m";

		default                             : return "---";
	}
}

static void print_lifetime(uint32_t lifetime) {
	if (lifetime == TIME_INFINITY)
		fprintf(stderr, "forever");
	else
		fprintf(stderr, "%" PRIu32, lifetime);
}

static void debug_type(uint8_t type, uint16_t len, void *data) {
	char abuf[INET6_ADDRSTRLEN];
	switch(type) {
		case IOTH_CONFDATA_STATIC4_ADDR     : // struct ioth_confdata_ipaddr
		case IOTH_CONFDATA_DHCP4_ADDR       :
			if (len >= sizeof(struct ioth_confdata_ipaddr)) {
				struct ioth_confdata_ipaddr *ipaddr = data;
				fprintf(stderr, " %s/%d !", inet_ntop(AF_INET, &ipaddr->addr, abuf, INET6_ADDRSTRLEN),
						ipaddr->prefixlen);
				print_lifetime(ipaddr->leasetime);
			}
			break;

		case IOTH_CONFDATA_DHCP4_SERVER     : // struct in_addr
		case IOTH_CONFDATA_DHCP4_ROUTER     :
		case IOTH_CONFDATA_DHCP4_DNS        :
		case IOTH_CONFDATA_STATIC4_DNS      :
		case IOTH_CONFDATA_STATIC4_ROUTE    :
			if (len >= sizeof(struct in_addr)) {
				struct in_addr *addr = data;
				fprintf(stderr, " %s", inet_ntop(AF_INET, addr, abuf, INET6_ADDRSTRLEN));
				debug_type(type, len - sizeof(struct in_addr), addr + 1);
			}
			break;

		case IOTH_CONFDATA_STATIC6_ADDR     : // struct ioth_confdata_ip6addr
		case IOTH_CONFDATA_STATIC6_ROUTE    :
		case IOTH_CONFDATA_STATIC6_DNS      :
		case IOTH_CONFDATA_DHCP6_ADDR       :
		case IOTH_CONFDATA_RD6_PREFIX       :
		case IOTH_CONFDATA_RD6_ADDR         :
		case IOTH_CONFDATA_RD6_ROUTER       :
			if (len >= sizeof(struct ioth_confdata_ip6addr)) {
				struct ioth_confdata_ip6addr *ip6addr = data;
				fprintf(stderr, " %s/%d %02x ?",
						inet_ntop(AF_INET6, &ip6addr->addr, abuf, INET6_ADDRSTRLEN),
						ip6addr->prefixlen, ip6addr->flags);
				print_lifetime(ip6addr->preferred_lifetime);
				fprintf(stderr, " !");
				print_lifetime(ip6addr->valid_lifetime);
			}
			break;

		case IOTH_CONFDATA_DHCP6_DNS        : // struct in6_addr
			if (len >= sizeof(struct in6_addr)) {
				struct in6_addr *addr = data;
				fprintf(stderr, " %s", inet_ntop(AF_INET6, addr, abuf, INET6_ADDRSTRLEN));
				debug_type(type, len - sizeof(struct in_addr), addr + 1);
			}
			break;

		case IOTH_CONFDATA_DHCP4_DOMAIN     : // string/multistring
		case IOTH_CONFDATA_DHCP6_DOMAIN     :
		case IOTH_CONFDATA_STATIC_DOMAIN    :
			if (len > 0) {
				char *s = data;
				size_t ssize = strlen(s) + 1;
				fprintf(stderr, " %s", s);
				debug_type(type, len - ssize, s + ssize);
			}
			break;

		case IOTH_CONFDATA_DHCP6_SERVERID   : //hex
			{
				int i;
				uint8_t *hd = data;
				for (i = 0; i < len; i++)
					fprintf(stderr, " %02x", hd[i]);
			}
			break;

		case IOTH_CONFDATA_RD6_MTU          :
			{
				uint32_t *mtu = data;
				fprintf(stderr, " %" PRIu32, *mtu);
			}
			break;
	}
}

static int iothconf_debug_cb(void *data, void *arg) {
	(void) arg;
	uint8_t type = ioth_confdata_gettype(data);
	time_t timestamp = ioth_confdata_gettimestamp(data);
	char timebuf[STRTIMESTAMPLEN];
	fprintf(stderr, "%02x %3s %s %02x%5d:", type, strtype(type),
			strtimestamp(timestamp, timebuf), ioth_confdata_setflags(data, 0),
			ioth_confdata_getdatalen(data));
	debug_type(type, ioth_confdata_getdatalen(data), data);
	fprintf(stderr, "\n");
	return 0;
}

void iothconf_data_debug(struct ioth *stack, unsigned int ifindex) {
	uint8_t type = 0;
	fprintf(stderr, " k typ   date    time flag len  data\n");
	for (type = 1; type != 0; type++)
		ioth_confdata_forall(stack, ifindex, type, iothconf_debug_cb, NULL);
}
