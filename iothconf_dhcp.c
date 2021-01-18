/*
 *   iothconf_dhcp.c: auto configuration for ioth.
 *       dhcp (v4) partial implementation do DHCP 2131 and 6843 (fqdn)
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/random.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <ioth.h>
#include <iothconf.h>
#include <iothconf_mod.h>
#include <iothconf_hash.h>
#include <iothconf_data.h>

struct dhcpdata {
	struct ioth *stack;
	unsigned int ifindex;
	uint8_t xid[4];
	uint8_t macaddr[ETH_ALEN];
	const char *fqdn;
	time_t timestamp;
	struct in_addr serveraddr;
	struct in_addr clientaddr;
};

#define   DHCP_CLIENTPORT   68
#define   DHCP_SERVERPORT   67

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7
#define DHCPINFORM   8

#define OPTION_PAD        0
#define OPTION_MASK       1
#define OPTION_ROUTER     3
#define OPTION_DNS        6
#define OPTION_HOSTNAME  12
#define OPTION_DOMNAME   15
#define OPTION_BROADCAST 28
#define OPTION_NTP       42
#define OPTION_REQIP     50
#define OPTION_LEASETIME 51
#define OPTION_TYPE      53
#define OPTION_SERVID    54
#define OPTION_PARLIST   55
#define OPTION_MAXSIZE   57
#define OPTION_CLIENTID  61
#define OPTION_FQDN      81
#define OPTION_DOMAIN_LIST      119
#define OPTION_END      255

#define DHCP_COOKIE {0x63,0x82,0x53,0x63}
#define BOOTP_VEND_LEN 64

__attribute__((__packed__)) struct bootp_head {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint8_t xid[4];
	uint8_t secs[2];
	uint8_t flags[2];
	uint8_t ciaddr[4];
	uint8_t yiaddr[4];
	uint8_t siaddr[4];
	uint8_t giaddr[4];
	uint8_t chaddr[16];
	uint8_t sname[64];
	uint8_t file[128];
};

__attribute__((__packed__)) struct dhcp_head {
	uint8_t dhcp_cookie[4];
};

__attribute__((__packed__)) struct dhcp_option {
	uint8_t opt_type;
	uint8_t opt_len;
	uint8_t opt_value[];
};

#define MAXDHCP 576
#define DHCPPKT \
	(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct bootp_head) + sizeof(struct dhcp_head))
#define MAXOPT MAXDHCP - DHCPPKT

struct dhcp_pkt {
	struct iphdr ip_h;
	struct udphdr udp_h;
	struct bootp_head bootp_h;
	struct dhcp_head dhcp_h;
	uint8_t options[MAXOPT];
};

/* IP checksum */
unsigned int chksum(unsigned int sum, void *vbuf, size_t len) {
	uint8_t *buf=vbuf;
	unsigned int i;
	for (i = 0; i < len; i++)
		sum += (i & 1)? buf[i]: buf[i]<<8;
	sum = (sum>>16) + (sum & 0xffff);
	sum = (sum>>16) + (sum & 0xffff);
	return sum;
}

/* packet composing helper functions */
static void add_dhcp_opt_type(FILE *f, int type) {
	fputc(OPTION_TYPE, f);
	fputc(1, f);
	fputc(type, f);
}

static void add_dhcp_opt_clientid(FILE *f, uint8_t *macaddr) {
	fputc(OPTION_CLIENTID, f);
	fputc(7, f);
	fputc(1, f); // ethernet
	fwrite(macaddr, ETH_ALEN, 1, f);
}

static void add_dhcp_opt_fqdn(FILE *f, const char *fqdn) {
	size_t len = strlen(fqdn);
	if (len > 251) return;
	fputc(OPTION_FQDN, f);
	fputc(4 + len, f);
	fputc(0x01, f); //flags
	fputc(0x00, f); //A-res
	fputc(0x00, f); //PTR-res
	fwrite(fqdn, len + 1, 1, f);
}

static void add_dhcp_opt_maxsize(FILE *f) {
	fputc(OPTION_MAXSIZE, f);
	fputc(2, f);
	fputc(MAXDHCP >> 8, f);
	fputc(MAXDHCP & 255, f);
}

static void add_dhcp_opt_end(FILE *f) {
	fputc(OPTION_END, f);
}

static void add_dhcp_opt(FILE *f, int opttype, uint8_t len,  const void *opt) {
	fputc(opttype, f);
	fputc(len, f);
	fwrite(opt, len, 1, f);
}

static void add_dhcp_opt_parlist(FILE *f, ...) {
	int len;
	va_list ap;
	va_start(ap, f);
	for (len = 0; va_arg(ap, unsigned int) != 0; len++)
		;
	va_end(ap);
	fputc(OPTION_PARLIST, f);
	fputc(len, f);
	va_start(ap, f);
	for (;;) {
		uint8_t opt = va_arg(ap, unsigned int);
		if (opt == 0) break;
		fputc(opt, f);
	}
	va_end(ap);
}

/* dialog functions. dhcp_send and dhcp_get use indirect recursion.
	 In this way temporary data can be stored on the stack */
static int dhcp_get(int sendtype, int fd, const struct sockaddr_ll *dest_addr, struct dhcpdata *data);
static int dhcp_send(int type, int fd, const struct sockaddr_ll *dest_addr, struct dhcpdata *data) {
	switch (type) {
		case DHCPDISCOVER:
		case DHCPREQUEST:
			break;
		default:
			return errno = EINVAL, -1;
	}
	if (getrandom(data->xid, sizeof(data->xid), 0) < 0)
		return -1;
	struct dhcp_pkt outbuf = {
		.ip_h.version = 4,
		.ip_h.ihl = 5,
		.ip_h.ttl = 64,
		.ip_h.protocol = SOL_UDP, //UDP
		.ip_h.daddr = 0xffffffff,
		.udp_h.uh_sport = htons(DHCP_CLIENTPORT),
		.udp_h.uh_dport = htons(DHCP_SERVERPORT),
		.bootp_h.op = 1, // boot req
		.bootp_h.htype = 1, // ethernet
		.bootp_h.hlen = ETH_ALEN,
		.dhcp_h.dhcp_cookie = DHCP_COOKIE};
	memcpy(outbuf.bootp_h.xid, data->xid, sizeof(data->xid));
	memcpy(outbuf.bootp_h.chaddr, data->macaddr, sizeof(data->macaddr));
	if (type != DHCPDISCOVER) {
		memcpy(outbuf.bootp_h.ciaddr, &data->clientaddr, sizeof(data->clientaddr));
		memcpy(outbuf.bootp_h.siaddr, &data->serveraddr, sizeof(data->serveraddr));
	}
	unsigned int sum=0;
	FILE *optf = fmemopen(&outbuf.options, MAXOPT, "w");
	add_dhcp_opt_type(optf, type);
	add_dhcp_opt_maxsize(optf);
	add_dhcp_opt_clientid(optf, data->macaddr);
	if (type != DHCPDISCOVER) {
		add_dhcp_opt(optf, OPTION_REQIP, sizeof(data->clientaddr), &data->clientaddr);
		add_dhcp_opt(optf, OPTION_SERVID, sizeof(data->serveraddr), &data->serveraddr);
	}
	add_dhcp_opt_parlist(optf,
			OPTION_MASK,
			OPTION_ROUTER,
			OPTION_DNS,
			OPTION_DOMNAME,
			0);
	if (data->fqdn)
		add_dhcp_opt_fqdn(optf, data->fqdn);
	add_dhcp_opt_end(optf);
	long optlen = ftell(optf);
	fclose(optf);
	outbuf.udp_h.uh_ulen = htons(DHCPPKT - sizeof(struct iphdr) + optlen);
	outbuf.ip_h.tot_len = htons(DHCPPKT + optlen);
	outbuf.ip_h.check = 0;
	sum = chksum(sum, &outbuf.ip_h, sizeof(outbuf.ip_h));
	outbuf.ip_h.check = htons(~sum);
	/* try 3 times */
	int times = 0;
	for (;;) {
		if (ioth_sendto(fd, &outbuf, DHCPPKT + optlen, 0, (struct sockaddr *) dest_addr, sizeof(*dest_addr)) < 0)
			return -1;
		if (dhcp_get(type, fd, dest_addr, data) == 0)
			return 0;
		if (times >= 2 || errno != ETIME)
			return -1;
		times++;
	}
}

/* check if the current packet is the expected one */
static int check_consistency(struct dhcp_pkt inbuf, size_t inbuflen, struct dhcpdata *data) {
	unsigned int sum=0;
	static uint8_t dhcp_cookie[] = DHCP_COOKIE;
	if (inbuflen < DHCPPKT)
		return 0;
	sum = chksum(sum, &inbuf.ip_h, sizeof(inbuf.ip_h));
	if (sum != 0xffff)
		return 0;
	if (inbuf.ip_h.protocol != SOL_UDP)
		return 0;
	if (inbuf.udp_h.uh_sport != htons(DHCP_SERVERPORT))
		return 0;
	if (inbuf.udp_h.uh_dport != htons(DHCP_CLIENTPORT))
		return 0;
	if (memcmp(inbuf.bootp_h.xid, data->xid, sizeof(data->xid)))
		return 0;
	if (memcmp(inbuf. dhcp_h.dhcp_cookie, dhcp_cookie, sizeof(dhcp_cookie)))
		return 0;
	if (inbuf.bootp_h.op != 2) //boot reply
		return 0;
	return 1;
}

/* mask to prefix conversion. e.g.: 255.255.255.0 -> 24 */
static uint8_t mask2prefix(uint32_t mask) {
	int i;
	for (i = 0;i < 32; i++, mask >>= 1)
		if (mask & 1)
			break;
	return 32 - i;
}

#define DHCP_TIMEOUT 2000
static int dhcp_get(int sendtype, int fd, const struct sockaddr_ll *dest_addr, struct dhcpdata *data) {
	int type;
	switch (sendtype) {
		case DHCPDISCOVER: type = DHCPOFFER; break;
		case DHCPREQUEST: type = DHCPACK; break;
		default:
											return errno = EINVAL, -1;
	}
	struct dhcp_pkt inbuf;
	struct pollfd pfd[] = {{fd, POLLIN, 0}};
	int timeout = DHCP_TIMEOUT;
	struct timeval start;
	struct timeval end;
	struct timeval timediff;
	/* this loop terminates:
		 when the expected reply msg has been received and processed OR
		 when the timeout expires */
	for(;;) {
		int event;
		gettimeofday(&start, NULL);
		event = poll(pfd, 1, timeout);
		//printf("event %d\n", event);
		if (event == 0)
			return errno = ETIME, -1;
		size_t inbuflen = ioth_recvfrom(fd, &inbuf, sizeof(inbuf), 0, NULL, NULL);
		//printf("%zd \n", inbuflen);
		if (check_consistency(inbuf, inbuflen, data)) {
			FILE *optf = fmemopen(&inbuf.options, MAXOPT, "r");
			uint8_t answ_type = 0;
			uint8_t *answ_server = NULL;
			uint8_t answ_prefix = 0;
			uint32_t answ_leasetime = 0;
			/* first scan */
			for(;;) {
				int opt_type = getc(optf);
				//printf("1 %d\n",opt_type);
				if (opt_type == OPTION_PAD)
					continue;
				if (opt_type == EOF || opt_type == OPTION_END)
					break;
				unsigned int opt_len = getc(optf);
				long next_opt = ftell(optf) + opt_len;
				switch(opt_type) {
					case OPTION_TYPE: answ_type = getc(optf); break;
					case OPTION_SERVID: answ_server = inbuf.options + ftell(optf); break;
					case OPTION_MASK:
															answ_prefix = mask2prefix(
																	(getc(optf) << 24) |
																	(getc(optf) << 16) |
																	(getc(optf) << 8) |
																	getc(optf));
															break;
					case OPTION_LEASETIME:
															answ_leasetime = (getc(optf) << 24) | (getc(optf) << 16) |
																(getc(optf) << 8) | getc(optf);
															break;


				}
				fseek(optf, next_opt, SEEK_SET);
			}
			fclose(optf);
			if (answ_type == DHCPNAK)
				return errno = ECANCELED, -1;
			if (answ_type == type && answ_server) {
				memcpy(&data->serveraddr, answ_server, sizeof(data->serveraddr));
				memcpy(&data->clientaddr, inbuf.bootp_h.yiaddr, sizeof(data->clientaddr));
				if (answ_type == DHCPOFFER)
					return dhcp_send(DHCPREQUEST, fd, dest_addr, data);
				else if (answ_type == DHCPACK) {
					ioth_confdata_add_data(data->stack, data->ifindex, IOTH_CONFDATA_DHCP4_SERVER, data->timestamp, 0,
							struct in_addr, data->serveraddr.s_addr);
					ioth_confdata_add_data(data->stack, data->ifindex, IOTH_CONFDATA_DHCP4_ADDR, data->timestamp, 0,
							struct ioth_confdata_ipaddr,
							.addr = data->clientaddr,
							.prefixlen = answ_prefix,
							.leasetime = answ_leasetime);
					optf = fmemopen(&inbuf.options, MAXOPT, "r");
					/* second scan */
					for(;;) {
						int opt_type = getc(optf);
						if (opt_type == OPTION_PAD)
							continue;
						if (opt_type == EOF || opt_type == OPTION_END)
							break;
						unsigned int opt_len = getc(optf);
						long next_opt = ftell(optf) + opt_len;
						switch(opt_type) {
							case OPTION_ROUTER:
								ioth_confdata_add(data->stack, data->ifindex, IOTH_CONFDATA_DHCP4_ROUTER, data->timestamp, 0,
										inbuf.options + ftell(optf), opt_len);
								break;
							case OPTION_DNS:
								ioth_confdata_add(data->stack, data->ifindex, IOTH_CONFDATA_DHCP4_DNS, data->timestamp, 0,
										inbuf.options + ftell(optf), opt_len);
								break;
							case OPTION_DOMNAME:
								{
									/* add strung termination */
									char domname[opt_len + 1];
									snprintf(domname, opt_len + 1,  "%.*s", opt_len, inbuf.options + ftell(optf));
									ioth_confdata_add(data->stack, data->ifindex, IOTH_CONFDATA_DHCP4_DOMAIN, data->timestamp, 0,
											domname, opt_len + 1);
								}
								break;
						}
						fseek(optf, next_opt, SEEK_SET);
					}
					fclose(optf);
					ioth_confdata_write_timestamp(data->stack, data->ifindex, IOTH_CONFDATA_DHCP4_TIMESTAMP, data->timestamp);
					return 0;
				} else
					return errno = EFAULT, -1;
			}
		}
		/* the code reaches this poinnt only if a spurious pakcet has beeen received.
			 it loops waiting for more packets using the remaining time to the timeout */
		gettimeofday(&end, NULL);
		timersub(&end, &start, &timediff);
		timeout -= timediff.tv_sec * 1000 + timediff.tv_usec / 1000;
		if (timeout < 0) timeout = 0;
	}
}

static int iothconf_dhcp_proto(struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags) {
	(void) config_flags;
	int packet_socket = ioth_msocket(stack, AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	struct sockaddr_ll sll = {
		.sll_family = AF_PACKET,
		.sll_protocol = htons(ETH_P_IP),
		.sll_ifindex = ifindex,
		.sll_halen = ETH_ALEN,
		.sll_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	};
	//ioth_bind(packet_socket, (struct sockaddr *) &sll, sizeof(sll));
	struct dhcpdata dhcpdata = {
		.stack = stack,
		.ifindex = ifindex,
		.xid = {0, 0, 0, 0},
		.fqdn = fqdn,
		.timestamp = ioth_confdata_new_timestamp(stack, ifindex, IOTH_CONFDATA_DHCP4_TIMESTAMP)
	};
	ioth_linkgetaddr(stack, ifindex, dhcpdata.macaddr);
	//loop
	int rv = dhcp_send(DHCPDISCOVER, packet_socket, &sll, &dhcpdata);
	ioth_close(packet_socket);
	return rv;
}

int iothconf_dhcp(struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags) {
	int rv = iothconf_dhcp_proto(stack, ifindex, fqdn, config_flags);
	if (rv == 0)
		iothconf_ip_update(stack, ifindex, IOTH_CONFDATA_DHCP4_TIMESTAMP, config_flags);
	return rv;
}
