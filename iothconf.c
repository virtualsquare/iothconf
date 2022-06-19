/*
 *   iothconf.c: auto configuration library for ioth
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stropt.h>
#include <strcase.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iothconf.h>
#include <iothconf_hash.h>
#include <iothconf_data.h>
#include <iothconf_mod.h>

/* configuration for ethernet:
	 if fqdn, create a hash based MAC address (so that the node always gets the same MAC);
	 turn on the interface */
int iothconf_eth(struct ioth *stack, unsigned int ifindex,
		const char *fqdn, const char *mac, uint32_t config_flags) {
	(void) config_flags;
	uint8_t macaddr[ETH_ALEN];
	if (mac) {
		ioth_macton(mac, macaddr);
		ioth_linksetaddr(stack,ifindex, macaddr);
	} else if (fqdn) {
		iothconf_hashmac(macaddr, fqdn);
		ioth_linksetaddr(stack,ifindex, macaddr);
	}
	ioth_linksetupdown(stack, ifindex, 1);
	usleep(1000000);
	return 0;
}

void iothconf_cleaneth(struct ioth *stack, unsigned int ifindex, uint32_t config_flags) {
	(void) config_flags;
	ioth_linksetupdown(stack, ifindex, 0);
}

static int iothconf_static(struct ioth *stack, unsigned int ifindex, char **tags, char **args, uint32_t config_flags) {
	time_t ioth_timestamp = 1; // static! all records dated back to 1970 Jan 01 0:00:01
	int prefix;
	char *prefixstr;
	uint8_t addr[sizeof(struct in6_addr)];
	for (; *tags != NULL; tags++, args++) {
		switch(strcase(*tags)) {
			case STRCASE(i,p):
				if (*args == NULL) break;
				prefixstr = strchr(*args, '/');
				if (prefixstr == NULL)
					prefix = 0;
				else {
					*prefixstr++ = 0;
					prefix = strtol(prefixstr, NULL, 10);
				}
				if (inet_pton(AF_INET6, *args, addr))
					ioth_confdata_add_data(stack, ifindex, IOTH_CONFDATA_STATIC6_ADDR, ioth_timestamp, 0,
							struct ioth_confdata_ip6addr,
							.addr = *((struct in6_addr *)(&addr)),
							.prefixlen = (prefix == 0) ? 64 : prefix,
							.preferred_lifetime = TIME_INFINITY,
							.valid_lifetime = TIME_INFINITY);
				else if (inet_pton(AF_INET, *args, addr))
					ioth_confdata_add_data(stack, ifindex, IOTH_CONFDATA_STATIC4_ADDR, ioth_timestamp, 0,
							struct ioth_confdata_ipaddr,
							.addr = *((struct in_addr *)(&addr)),
							.prefixlen = (prefix == 0) ? 24 : prefix,
							.leasetime = TIME_INFINITY);
				break;
			case STRCASE(minus,i,p):
				if (*args == NULL) break;
				prefixstr = strchr(*args, '/');
				if (prefixstr == NULL)
					prefix = 0;
				else {
					*prefixstr++ = 0;
					prefix = strtol(prefixstr, NULL, 10);
				}
				if (inet_pton(AF_INET6, *args, addr))
					ioth_confdata_del_data(stack, ifindex, IOTH_CONFDATA_STATIC6_ADDR,
							struct ioth_confdata_ip6addr,
							.addr = *((struct in6_addr *)(&addr)),
							.prefixlen = (prefix == 0) ? 64 : prefix,
							.preferred_lifetime = TIME_INFINITY,
							.valid_lifetime = TIME_INFINITY);
				else if (inet_pton(AF_INET, *args, addr))
					ioth_confdata_del_data(stack, ifindex, IOTH_CONFDATA_STATIC4_ADDR,
							struct ioth_confdata_ipaddr,
							.addr = *((struct in_addr *)(&addr)),
							.prefixlen = (prefix == 0) ? 24 : prefix,
							.leasetime = TIME_INFINITY);
				break;
			case STRCASE(g,w):
				if (*args == NULL) break;
				if (inet_pton(AF_INET6, *args, addr))
					ioth_confdata_add_data(stack, ifindex, IOTH_CONFDATA_STATIC6_ROUTE, ioth_timestamp, 0,
							struct ioth_confdata_ip6addr,
							.addr = *((struct in6_addr *)(&addr)),
							.valid_lifetime = TIME_INFINITY);
				else if (inet_pton(AF_INET, *args, addr))
					ioth_confdata_add(stack, ifindex, IOTH_CONFDATA_STATIC4_ROUTE, ioth_timestamp, 0,
							addr, sizeof(struct in_addr));
				break;
			case STRCASE(minus,g,w):
				if (*args == NULL) break;
				if (inet_pton(AF_INET6, *args, addr))
					ioth_confdata_del_data(stack, ifindex, IOTH_CONFDATA_STATIC6_ROUTE,
							struct ioth_confdata_ip6addr,
							.addr = *((struct in6_addr *)(&addr)),
							.valid_lifetime = TIME_INFINITY);
				else if (inet_pton(AF_INET, *args, addr))
					ioth_confdata_del(stack, ifindex, IOTH_CONFDATA_STATIC4_ROUTE,
							addr, sizeof(struct in_addr));
				break;
			case STRCASE(d,n,s):
				if (inet_pton(AF_INET6, *args, addr))
					ioth_confdata_add(stack, ifindex, IOTH_CONFDATA_STATIC6_DNS, ioth_timestamp, 0,
							addr, sizeof(struct in6_addr));
				else if (inet_pton(AF_INET, *args, addr))
					ioth_confdata_add(stack, ifindex, IOTH_CONFDATA_STATIC4_DNS, ioth_timestamp, 0,
							addr, sizeof(struct in_addr));
				break;
			case STRCASE(minus,d,n,s):
				if (inet_pton(AF_INET6, *args, addr))
					ioth_confdata_del(stack, ifindex, IOTH_CONFDATA_STATIC6_DNS,
							addr, sizeof(struct in6_addr));
				else if (inet_pton(AF_INET, *args, addr))
					ioth_confdata_del(stack, ifindex, IOTH_CONFDATA_STATIC4_DNS,
							addr, sizeof(struct in_addr));
				break;
			case STRCASE(d,o,m,a,i,n):
				if (*args == NULL) break;
				ioth_confdata_add(stack, ifindex, IOTH_CONFDATA_STATIC_DOMAIN, ioth_timestamp, 0,
						*args, strlen(*args) + 1);
				break;
			case STRCASE(minus,d,o,m,a,i,n):
				if (*args == NULL) break;
				ioth_confdata_del(stack, ifindex, IOTH_CONFDATA_STATIC_DOMAIN,
						*args, strlen(*args) + 1);
				break;
		}
	}
	ioth_confdata_write_timestamp(stack, ifindex, IOTH_CONFDATA_STATIC_TIMESTAMP, ioth_timestamp);
	iothconf_ip_update(stack, ifindex, IOTH_CONFDATA_STATIC_TIMESTAMP, config_flags);
	return 0;
}

static int _ioth_config(struct ioth *stack, const char *config, int from_ioth_newstackc) {
	uint32_t config_flags = 0;
	uint32_t clean_flags = 0;
	char *fqdn = NULL;
	char *iface = NULL;
	char *mac = NULL;
	int ifindex = 0;
	int debug = 0;
	if (config == NULL) config = "";
	int tagc = stropt(config, NULL, NULL, NULL);
	char buf[strlen(config) + 1];
	char *tags[tagc];
	char *args[tagc];
	stropt(config, tags, args, buf);
	for (int i = 0; i < tagc - 1; i++) {
		switch(strcase(tags[i])) {
			case STRCASE(e,t,h): config_flags |= IOTHCONF_ETH; break;
			case STRCASE(d,h,c,p):
			case STRCASE(d,h,c,p,4):
			case STRCASE(d,h,c,p,v,4):
													 config_flags |= IOTHCONF_DHCP; break;
			case STRCASE(d,h,c,p,6):
			case STRCASE(d,h,c,p,v,6):
													 config_flags |= IOTHCONF_DHCPV6; break;
			case STRCASE(r,d):
			case STRCASE(r,d,6):
													 config_flags |= IOTHCONF_RD; break;
			case STRCASE(s,l,a,a,c):
													 config_flags |= IOTHCONF_RD_SLAAC; break;
			case STRCASE(a,u,t,o):
													 config_flags |=
														 IOTHCONF_ETH | IOTHCONF_DHCP | IOTHCONF_DHCPV6 | IOTHCONF_RD;
													 break;
			case STRCASE(a,u,t,o,4):
			case STRCASE(a,u,t,o,v,4):
													 config_flags |=
														 IOTHCONF_ETH | IOTHCONF_DHCP;
													 break;
			case STRCASE(a,u,t,o,6):
			case STRCASE(a,u,t,o,v,6):
													 config_flags |=
														 IOTHCONF_ETH | IOTHCONF_DHCPV6 | IOTHCONF_RD;
													 break;

			case STRCASE(minus,s,t,a,t,i,c):
													 clean_flags |= IOTHCONF_STATIC; break;
			case STRCASE(minus,e,t,h):
													 clean_flags |= IOTHCONF_ETH; break;
			case STRCASE(minus,d,h,c,p):
			case STRCASE(minus,d,h,c,p,4):
			case STRCASE(minus,d,h,c,p,v,4):
													 clean_flags |= IOTHCONF_DHCP; break;
			case STRCASE(minus,d,h,c,p,6):
			case STRCASE(minus,d,h,c,p,v,6):
													 clean_flags |= IOTHCONF_DHCPV6; break;
			case STRCASE(minus,r,d):
			case STRCASE(minus,r,d,6):
													 clean_flags |= IOTHCONF_RD; break;
			case STRCASE(minus,a,u,t,o):
			case STRCASE(minus,a,l,l):
													 clean_flags |=
														 IOTHCONF_ETH | IOTHCONF_DHCP | IOTHCONF_DHCPV6 | IOTHCONF_RD;
													 break;
			case STRCASE(minus,a,u,t,o,4):
			case STRCASE(minus,a,u,t,o,v,4):
													 clean_flags |=
														 IOTHCONF_ETH | IOTHCONF_DHCP;
													 break;
			case STRCASE(minus,a,u,t,o,6):
			case STRCASE(minus,a,u,t,o,v,6):
													 clean_flags |=
														 IOTHCONF_ETH | IOTHCONF_DHCPV6 | IOTHCONF_RD;
													 break;

			case STRCASE(f,q,d,n): fqdn = args[i]; break;
			case STRCASE(i,f,a,c,e): iface = args[i]; break;
			case STRCASE(i,f,i,n,d,e,x):
															 if (args[i] != NULL)
																 ifindex = strtoul(args[i], NULL, 10);
															 break;
			case STRCASE(m,a,c):
			case STRCASE(m,a,c,a,d,d,r): mac = args[i]; break;
			case STRCASE(i,p):
			case STRCASE(g,w):
			case STRCASE(d,n,s):
			case STRCASE(d,o,m,a,i,n):
			case STRCASE(minus,i,p):
			case STRCASE(minus,g,w):
			case STRCASE(minus,d,n,s):
			case STRCASE(minus,d,o,m,a,i,n):
																	 config_flags |= IOTHCONF_STATIC; break;
			case STRCASE(d,e,b,u,g):
																	 debug = 1; break;
			case STRCASE(s,t,a,c,k):
			case STRCASE(v,n,l):
																	 if (from_ioth_newstackc == 0)
																		 return errno = EINVAL, -1;
																	 break;
			default:
																	 return errno = EINVAL, -1;
		}
	}
	int retvalue = 0;
	if (config_flags || clean_flags || debug) {
		if (iface == NULL) iface = DEFAULT_INTERFACE;
		if (ifindex == 0) ifindex = ioth_if_nametoindex(stack, iface);
		if (ifindex <= 0)
			return errno = ENODEV, -1;
		if (clean_flags & IOTHCONF_STATIC)
			iothconf_ip_clean(stack, ifindex, IOTH_CONFDATA_STATIC_TIMESTAMP, 0);
		if (clean_flags & IOTHCONF_RD)
			iothconf_ip_clean(stack, ifindex, IOTH_CONFDATA_RD6_TIMESTAMP, 0);
		if (clean_flags & IOTHCONF_DHCPV6)
			iothconf_ip_clean(stack, ifindex, IOTH_CONFDATA_DHCP6_TIMESTAMP, 0);
		if (clean_flags & IOTHCONF_DHCP)
			iothconf_ip_clean(stack, ifindex, IOTH_CONFDATA_DHCP4_TIMESTAMP, 0);
		if (clean_flags & IOTHCONF_ETH)
			iothconf_cleaneth(stack, ifindex, 0);
		if (config_flags & IOTHCONF_ETH)
			if (iothconf_eth(stack, ifindex, fqdn, mac, config_flags) == 0)
				retvalue |= IOTHCONF_ETH;
		if (config_flags & IOTHCONF_RD)
			if (iothconf_rd(stack, ifindex, fqdn, config_flags) == 0)
				retvalue |= IOTHCONF_RD;
		if (config_flags & IOTHCONF_DHCPV6)
			if (iothconf_dhcpv6(stack, ifindex, fqdn, config_flags) == 0)
				retvalue |= IOTHCONF_DHCPV6;
		if (config_flags & IOTHCONF_DHCP)
			if (iothconf_dhcp(stack, ifindex, fqdn, config_flags) == 0)
				retvalue |= IOTHCONF_DHCP;
		if (config_flags & IOTHCONF_STATIC)
			if (iothconf_static(stack, ifindex, tags, args, config_flags) == 0)
				retvalue |= IOTHCONF_STATIC;
		if(debug)
			iothconf_data_debug(stack, ifindex);
	}
	return retvalue;
}

int ioth_config(struct ioth *stack, const char *config) {
	return _ioth_config(stack, config, 0);
}

char *ioth_resolvconf(struct ioth *stack, const char *config) {
	char *iface = NULL;
	int ifindex = 0;
	if (config == NULL) config = "";
	int tagc = stropt(config, NULL, NULL, NULL);
	char buf[strlen(config) + 1];
	char *tags[tagc];
	char *args[tagc];
	stropt(config, tags, args, buf);
	for (int i = 0; i < tagc - 1; i++) {
		switch(strcase(tags[i])) {
			case STRCASE(i,f,a,c,e): iface = args[i]; break;
			case STRCASE(i,f,i,n,d,e,x):
															 if (args[i] != NULL)
																 ifindex = strtoul(args[i], NULL, 10);
															 break;
			default:
															 return errno = EINVAL, NULL;
		}
	}
	if (iface == NULL) iface = DEFAULT_INTERFACE;
	if (ifindex == 0) ifindex = ioth_if_nametoindex(stack, DEFAULT_INTERFACE);
	if (ifindex <= 0)
		return errno = ENODEV, NULL;
	return iothconf_resolvconf(stack, ifindex);
}

struct ioth *ioth_newstackc(const char *stack_config) {
	struct ioth *ioth_stack;
	char *stack = NULL;
	char *vnl = NULL;
	if (stack_config == NULL) stack_config = "";
	int tagc = stropt(stack_config, NULL, NULL, NULL);
	char buf[strlen(stack_config) + 1];
	char *tags[tagc];
	char *args[tagc];
	stropt(stack_config, tags, args, buf);
	for (int i = 0; i < tagc - 1; i++) {
		switch(strcase(tags[i])) {
			case STRCASE(s,t,a,c,k):
				stack = args[i];
				break;
			case STRCASE(v,n,l):
				vnl = args[i];
				break;
		}
	}
	if ((ioth_stack = ioth_newstack(stack, vnl)) == NULL)
		return NULL;
	if (_ioth_config(ioth_stack, stack_config, 1) == -1) {
		ioth_delstack(ioth_stack);
		return NULL;
	}
	return ioth_stack;
}
