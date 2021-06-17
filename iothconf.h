#ifndef IOTHCONF_H
#define IOTHCONF_H
#include <ioth.h>

/* config is a comma separated list of flags and variable assignments:
 *
 *   iface=... : select the interface e.g. iface=eth0 (default value vde0)
 *   ifindex=... : id of the interface (it can be used instead of iface)
 *   fqdn=.... : set the fully qualified domain name for dhcp, dhcpv6 slaac-hash-autoconf
 *   mac=... : (or macaddr) define the macaddr for eth here below. 
               (e.g. eth,mac=10:a1:b2:c3:d4:e5)
 *   eth : turn on the interface (and set the MAC address if requested 
 *              or a hash based MAC address if fqdn is defined)
 *   dhcp : (or dhcp4 or dhcpv4) use dhcp (IPv4)
 *   dhcp6 : (or dhcpv6) use dhcpv6 (for IPv6)
 *   rd : (or rd6) use the router discovery protocol (IPv6)
 *   slaac : use stateless auto-configuration (IPv6) (requires rd)
 *   auto : shortcut for eth+dhcp+dhcp6+rd
 *   auto4 : (or autov4) shortcut for eth+dhcp
 *   auto6 : (or autov6) shortcut for eth+dhcp6+rd
 *   ip=..../.. : set a static address IPv4 or IPv6 and its prefix length
 *                example: ip=10.0.0.100/24  or ip=2001:760:1:2::100/64
 *   gw=..... : set a static default route IPv4 or IPv6
 *   dns=.... : set a static address for a DNS server
 *   domain=.... : set a static domain for the dns search
 *   debug : show the status of the current configuration parameters
 *   -static, -eth, -dhcp, -dhcp6, -rd, -auto, -auto4, -auto
 *     (and all the synonyms + a heading minus)
 *     clean (undo) the configuration
 *
 *   ioth_config can use four sources to compute the current configuration:
 *   static data, dhcp, router discovery and dhcpv6
 *   The current confiuration is a merge of all the parameters collected from
 *   all the sources. It is possible to update each source independently:
 *   for each source an update/recofnigration overwrites all the configuration 
 *   parameters previously acquired from that source.
 */

int ioth_config(struct ioth *stack, char *config);

/* ioth_config return value is -1 in case of error or a mask of the following bits.
	 each bit is on if the corresponding configuration succeeded */
#define IOTHCONF_STATIC   1 << 0
#define IOTHCONF_ETH      1 << 1
#define IOTHCONF_DHCP     1 << 2
#define IOTHCONF_DHCPV6   1 << 3
#define IOTHCONF_RD       1 << 4

/* ioth_resolvconf returns a string in resolv.conf(5) format.
 *	 the string is dynamically allocated (use free(3) to deallocate it).
 *	 config is a comma separated list of flags and variable assignments:
 *
 *   iface=... : select the interface e.g. iface=eth0 (default value vde0)
 *   ifindex=... : id of the interface (it can be used instead of iface)
 *
 *   It returns NULL and errno = 0 if nothing changed since the previous call.
 *   In case of error it returns NULL and errno != 0
 */

char *ioth_resolvconf(struct ioth *stack, char *config);

#endif
