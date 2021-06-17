#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include <iothconf.h>
#include <iothconf_dns.h>
#include <iothconf_mod.h>
#include <iothconf_data.h>

static inline uint8_t domain_nx(uint8_t len) {
	if ((len & 0xc0) == 0xc0) return 0;
	return len;
}

int iothconf_domain2mstr(uint8_t *domain, char *mstr, uint16_t len) {
	uint8_t *limit = domain + len;
	char *scan = mstr;
	uint8_t count, oldcount;
	if (len == 0)
		return 0;
	for (count = oldcount = domain_nx(*domain++); domain < limit; domain++) {
		if (count == 0) {
			count = domain_nx(*domain);
			if (oldcount > 0 && count > 0)
				*scan++ = '.';
			else if (oldcount == 0 && count > 0)
				*scan++ = 0;
			oldcount = count;
		} else {
			*scan++ = *domain;
			count --;
		}
	}
	*scan++ = 0;
	return scan - mstr;
}

struct iothconf_resolvconf_cb_arg {
	FILE *rc;
	int countdomains;
	int countupdated;
	int scandomains;
	char **domains;
};

/* count how many domains are included in data (maybe there are dup entries from
	 different sources (dhcpv4 dhcpv6) */
static int iothconf_resolvconf_count_cb(void *data, void *arg) {
	struct iothconf_resolvconf_cb_arg *cbarg = arg;
	uint8_t type = ioth_confdata_gettype(data);
	switch (type) {
		case IOTH_CONFDATA_DHCP4_DOMAIN:
		case IOTH_CONFDATA_DHCP6_DOMAIN:
		case IOTH_CONFDATA_STATIC_DOMAIN:
			if (!(ioth_confdata_setflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE))
				cbarg->countupdated++;
			cbarg->countdomains += count_mstr(data, ioth_confdata_getdatalen(data));
			break;
		case IOTH_CONFDATA_DHCP4_DNS:
		case IOTH_CONFDATA_DHCP6_DNS:
		case IOTH_CONFDATA_STATIC4_DNS:
		case IOTH_CONFDATA_STATIC6_DNS:
			if (!(ioth_confdata_setflags(data, IOTH_CONFDATA_ACTIVE) & IOTH_CONFDATA_ACTIVE))
				cbarg->countupdated++;
			break;
	}
	return 0;
}

/* return 0 if this domain has been seen already, 1 if it is new */
static int iothconf_resolvconf_newdom(char *domain, struct iothconf_resolvconf_cb_arg *cbarg) {
	int i;
	for (i = 0; i < cbarg->scandomains; i++) {
		if (strcmp(domain, cbarg->domains[i]) == 0)
			return 0;
	}
	cbarg->domains[cbarg->scandomains++] = domain;
	return 1;
}

/* add domains to the line "search" of resolv.conf:
	 one loop: domains in cbarg are pointer to data elements
	 the entire operation must be done in mutual exclusion */
static int iothconf_resolvconf_domain_cb(void *data, void *arg) {
	struct iothconf_resolvconf_cb_arg *cbarg = arg;
	uint8_t type = ioth_confdata_gettype(data);
	switch (type) {
		case IOTH_CONFDATA_DHCP4_DOMAIN:
		case IOTH_CONFDATA_DHCP6_DOMAIN:
		case IOTH_CONFDATA_STATIC_DOMAIN:
			FORmstr(domain, data, ioth_confdata_getdatalen(data)) {
				if (iothconf_resolvconf_newdom(domain, cbarg))
					fprintf(cbarg->rc, " %s", domain);
			}
			break;
	}
	return 0;
}

/* add "nameserver" lines in resolv.conf */
static int iothconf_resolvconf_dns_cb(void *data, void *arg) {
	struct iothconf_resolvconf_cb_arg *cbarg = arg;
	uint8_t type = ioth_confdata_gettype(data);
	uint8_t *scan = data;
	uint8_t *limit = scan + ioth_confdata_getdatalen(data);
	while (scan < limit) {
		char addrbuf[INET6_ADDRSTRLEN];
		switch (type) {
			case IOTH_CONFDATA_DHCP6_DNS:
			case IOTH_CONFDATA_STATIC6_DNS:
				fprintf(cbarg->rc, "nameserver %s\n",
						inet_ntop(AF_INET6, scan, addrbuf, INET6_ADDRSTRLEN));
				scan += sizeof(struct in6_addr);
				break;
			case IOTH_CONFDATA_DHCP4_DNS:
			case IOTH_CONFDATA_STATIC4_DNS:
				fprintf(cbarg->rc, "nameserver %s\n",
						inet_ntop(AF_INET, scan, addrbuf, INET6_ADDRSTRLEN));
				scan += sizeof(struct in_addr);
				break;
			default:
				return 0;
		}
	}
	return 0;
}

char *iothconf_resolvconf(struct ioth *stack, uint32_t ifindex) {
	char *resolvconf = NULL;
	size_t resolvconflen = 0;
	struct iothconf_resolvconf_cb_arg cbarg = {0};
	ioth_confdata_forall_mask(stack, ifindex, IOTH_CONFDATA_DNS_DOM_BASE, IOTH_CONFDATA_DNS_DOM_MASK,
			iothconf_resolvconf_count_cb, &cbarg);
	if (cbarg.countupdated > 0) {
		cbarg.rc = open_memstream(&resolvconf, &resolvconflen);
		if (cbarg.countdomains > 0) {
			char *domains[cbarg.countdomains];
			cbarg.domains = domains;
			printf("countdomains %d\n", cbarg.countdomains);
			fprintf(cbarg.rc, "search");
			ioth_confdata_forall_mask(stack, ifindex, IOTH_CONFDATA_DOM_BASE, IOTH_CONFDATA_DOM_MASK,
					iothconf_resolvconf_domain_cb, &cbarg);
			fprintf(cbarg.rc, "\n");
		}
		ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_STATIC6_DNS,
				iothconf_resolvconf_dns_cb, &cbarg);
		ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_STATIC4_DNS,
				iothconf_resolvconf_dns_cb, &cbarg);
		ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_DHCP6_DNS,
				iothconf_resolvconf_dns_cb, &cbarg);
		ioth_confdata_forall(stack, ifindex, IOTH_CONFDATA_DHCP4_DNS,
				iothconf_resolvconf_dns_cb, &cbarg);
		fclose(cbarg.rc);
	} else
		errno = 0;
	return resolvconf;
}

#if 0
char *iothconf_resolvconf(struct ioth *stack, char *config) {
	char *iface = NULL;
	int ifindex = 0;
	if (config == NULL) config = "";
	int tagc = stropt(config, NULL, NULL, NULL);
	char buf[strlen(config) + 1];
	if(tagc > 1) {
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
	}
	if (iface == NULL) iface = DEFAULT_INTERFACE;
	if (ifindex == 0) ifindex = ioth_if_nametoindex(stack, DEFAULT_INTERFACE);
	if (ifindex <= 0)
		return errno = EINVAL, NULL;
	return _iothconf_resolvconf(stack, ifindex);
}
#endif

#if 0
uint8_t test[] = {
	0x06,0x64,0x6f,0x6d,0x61,0x69,0x6e,0x07,0x65,0x78,0x61,0x6d,0x70,0x6c,0x65,0x00,
	0x06,0x64,0x61,0x6d,0x61,0x69,0x6e,0x07,0x65,0x78,0x61,0x6d,0x70,0x6c,0x65,0x00,
	0x06,0x73,0x65,0x63,0x6f,0x6e,0x64,0x06,0x64,0x6f,0x6d,0x61,0x69,0x6e,0x02,0x69,0x74,0x00};
main() {
	char mstr[sizeof(test)];
	int len = iothconf_domain2mstr(test, mstr, sizeof(test));
	printf("%d %d %d\n",len, sizeof(test), count_mstr(mstr, len));
	FORmstr(scan, mstr, len)
		printf("=%s=\n", scan);

	//printf("=%s= %d %d\n",str,strlen(str), sizeof(test));
}
#endif
