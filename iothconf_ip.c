/*
 *   iothconf_ip.c: update the stack configuration
 *   (set/unset ip address(es) and route(s)
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

#include <iothconf.h>
#include <iothconf_hash.h>
#include <iothconf_data.h>
#include <iothconf_mod.h>

static int ioth_ip_setaddr6(void *data, void *arg) {
	time_t *latest_timestamp = arg;
	struct ioth_confdata_ip6addr *ipaddr =  data;
	struct ioth *stack = ioth_confdata_getstack(data);
	time_t timestamp = ioth_confdata_gettimestamp(data);
	if (timestamp < *latest_timestamp) {
		if (ioth_confdata_clrflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE)
			ioth_ipaddr_del(stack, AF_INET6, &ipaddr->addr, ipaddr->prefixlen, ioth_confdata_getifindex(data));
		return IOTH_CONFDATA_FORALL_DELETE;
	} else {
		if (!(ioth_confdata_setflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE))
			ioth_ipaddr_add(stack, AF_INET6, &ipaddr->addr, ipaddr->prefixlen, ioth_confdata_getifindex(data));
		return 0;
	}
}

static int ioth_ip_setroute6(void *data, void *arg) {
	time_t *latest_timestamp = arg;
	struct ioth_confdata_ip6addr *ipaddr =  data;
	struct ioth *stack = ioth_confdata_getstack(data);
	time_t timestamp = ioth_confdata_gettimestamp(data);
	if (timestamp < *latest_timestamp) {
		if (ioth_confdata_clrflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE)
			ioth_iproute_del(stack, AF_INET6, NULL, 0, &ipaddr->addr, ioth_confdata_getifindex(data));
		return IOTH_CONFDATA_FORALL_DELETE;
	} else {
		if (!(ioth_confdata_setflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE))
			ioth_iproute_add(stack, AF_INET6, NULL, 0, &ipaddr->addr, ioth_confdata_getifindex(data));
		return 0;
	}
}

static int ioth_ip_setaddr4(void *data, void *arg) {
	time_t *latest_timestamp = arg;
	struct ioth_confdata_ipaddr *ipaddr =  data;
	struct ioth *stack = ioth_confdata_getstack(data);
	time_t timestamp = ioth_confdata_gettimestamp(data);
	if (timestamp < *latest_timestamp) {
		if (ioth_confdata_clrflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE)
			ioth_ipaddr_del(stack, AF_INET, &ipaddr->addr, ipaddr->prefixlen, ioth_confdata_getifindex(data));
		return IOTH_CONFDATA_FORALL_DELETE;
	} else {
		if (!(ioth_confdata_setflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE))
			ioth_ipaddr_add(stack, AF_INET, &ipaddr->addr, ipaddr->prefixlen, ioth_confdata_getifindex(data));
		return 0;
	}
}

static int ioth_ip_setroute4(void *data, void *arg) {
	time_t *latest_timestamp = arg;
	struct in_addr *ipaddr =  data;
	struct ioth *stack = ioth_confdata_getstack(data);
	time_t timestamp = ioth_confdata_gettimestamp(data);
	if (timestamp < *latest_timestamp) {
		if (ioth_confdata_clrflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE)
			ioth_iproute_del(stack, AF_INET, NULL, 0, ipaddr, 0);
		return IOTH_CONFDATA_FORALL_DELETE;
	} else {
		if (!(ioth_confdata_setflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE))
			ioth_iproute_add(stack, AF_INET, NULL, 0, ipaddr, 0);
		return 0;
	}
}

static int ioth_ip_cleanold(void *data, void *arg) {
	time_t *latest_timestamp = arg;
	time_t timestamp = ioth_confdata_gettimestamp(data);
	if (timestamp < *latest_timestamp)
		return IOTH_CONFDATA_FORALL_DELETE;
	else
		return 0;
}

void iothconf_ip_update(struct ioth *stack, unsigned int ifindex, uint8_t type, uint32_t config_flags) {
	(void) config_flags;
	if (type != TIMESTAMP(type)) return;
	time_t timestamp = ioth_confdata_read_timestamp(stack, ifindex, type);
	switch (type) {
		case IOTH_CONFDATA_STATIC_TIMESTAMP:
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_STATIC6_ADDR, ioth_ip_setaddr6, &timestamp);
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_STATIC6_ROUTE, ioth_ip_setroute6, &timestamp);
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_STATIC4_ADDR, ioth_ip_setaddr4, &timestamp);
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_STATIC4_ROUTE, ioth_ip_setroute4, &timestamp);
			break;
		case IOTH_CONFDATA_DHCP4_TIMESTAMP:
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_DHCP4_ADDR, ioth_ip_setaddr4, &timestamp);
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_DHCP4_ROUTER, ioth_ip_setroute4, &timestamp);
			break;
		case IOTH_CONFDATA_DHCP6_TIMESTAMP:
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_DHCP6_ADDR, ioth_ip_setaddr6, &timestamp);
			break;
		case IOTH_CONFDATA_RD6_TIMESTAMP:
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_RD6_ADDR, ioth_ip_setaddr6, &timestamp);
			ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_RD6_ROUTER, ioth_ip_setroute6, &timestamp);
			break;
	}
	ioth_confdata_forall_mask(stack, ifindex, type, IOTH_CONFDATA_MASK_TYPE, ioth_ip_cleanold, &timestamp);
}

void iothconf_ip_clean(struct ioth *stack, unsigned int ifindex, uint8_t type, uint32_t config_flags) {
	(void) config_flags;
	if (type != TIMESTAMP(type)) return;
	time_t timestamp = ioth_confdata_new_timestamp(stack, ifindex, type);
	ioth_confdata_write_timestamp(stack, ifindex, type, timestamp);
	iothconf_ip_update(stack, ifindex, type, config_flags);
	ioth_confdata_del_timestamp(stack, ifindex, type);
}
