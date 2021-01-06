#ifndef IOTHCONF_MOD_H
#define IOTHCONF_MOD_H
#include <ioth.h>
#include <iothconf.h>
#include <stdint.h>

/* defined in iothconf.h
 * #define IOTHCONF_STATIC   1 << 0
 * #define IOTHCONF_ETH      1 << 1
 * #define IOTHCONF_DHCP     1 << 2
 * #define IOTHCONF_DHCPV6   1 << 3
 * #define IOTHCONF_RD       1 << 4
 */
#define IOTHCONF_RD_SLAAC 1 << 24

#define DEFAULT_INTERFACE "vde0"
#define TIME_INFINITY 0xffffffff

int iothconf_eth   (struct ioth *stack, unsigned int ifindex, 
		const char *fqdn, const char *mac, uint32_t config_flags);
int iothconf_dhcp  (struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags);
int iothconf_dhcpv6(struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags);
int iothconf_rd    (struct ioth *stack, unsigned int ifindex, const char *fqdn, uint32_t config_flags);

void iothconf_ip_update(struct ioth *stack, unsigned int ifindex, uint8_t type, uint32_t config_flags);
void iothconf_ip_clean(struct ioth *stack, unsigned int ifindex, uint8_t type, uint32_t config_flags);

void iothconf_data_debug(struct ioth *stack, unsigned int ifindex);

char *iothconf_resolvconf(struct ioth *stack, uint32_t ifindex);
#endif
