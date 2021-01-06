#ifndef IOTHCONF_DATA_H
#define IOTHCONF_DATA_H
#include <stdint.h>
#include <netinet/in.h>

/* Data structure for ioth_config.
   struct ioth_confdata is the header.
   it is currently managed as a simple list, could be re-implemented as a more efficient
   data structure, each ioth node should have one to a few addresses so it would not care after all.

   entries are timestamped, each time is created or confirmed the timestamp is updated.
   The tiemstamp of the latest message processes is stored as an element in the data structure itself:
   type 0x40 for DHCP, 0x50 for RD (rotuer advertisement), 0x60 for DHCPv6, 0x70 for static defs.
   (0x40 is the msg timestamp for all 0x4* records, 0x50 for 0x5*, 0x60 for 0x6*, and 0x70 for 0x7*)

   Each update operation takes the following steps:
   - generate a new msg timestamp (do not record it in the data structure yet!).
   - add/update each record from the message using the new generated timestamp
   - update the new timestamp entry 0x40, 0x50, 0x60 or 0x70.

   The retrieve operation follows:
   - read the message timestamp (0x40, 0x50 or 0x60)
   - all the records with timestamp >= message timestamp are valid
   - those with timestamp < message timestamp are obsolete (have not been confirmed in the latest msg).

	 01xx 100x hex 48,49,58,59,68,69,78,79 are reserved fod DNS
	 01xx 101x hex 4a,4b,5a,5b,6a,6b,7a,7b are reserved fod DOMAIN
   */

#define IOTH_CONFDATA_STATIC_TIMESTAMP 0x70 // no data
#define IOTH_CONFDATA_STATIC4_ADDR     0x72 // struct ioth_confdata_ipaddr
#define IOTH_CONFDATA_STATIC4_ROUTE    0x73 // struct in_addr
#define IOTH_CONFDATA_STATIC6_ADDR     0x74 // struct ioth_confdata_ip6addr
#define IOTH_CONFDATA_STATIC6_ROUTE    0x75 // struct ioth_confdata_ip6addr
#define IOTH_CONFDATA_STATIC4_DNS      0x78 // struct in_addr
#define IOTH_CONFDATA_STATIC6_DNS      0x79 // struct in6_addr
#define IOTH_CONFDATA_STATIC_DOMAIN    0x7a // search domain string

#define IOTH_CONFDATA_DHCP4_TIMESTAMP  0x40 // no data
#define IOTH_CONFDATA_DHCP4_SERVER     0x41 // struct in_addr
#define IOTH_CONFDATA_DHCP4_ADDR       0x42 // struct ioth_confdata_ipaddr
#define IOTH_CONFDATA_DHCP4_ROUTER     0x43 // struct in_addr (maybe more than one)
#define IOTH_CONFDATA_DHCP4_DNS        0x48 // struct in_addr (maybe more than one)
#define IOTH_CONFDATA_DHCP4_DOMAIN     0x4a // search list multistring (*).

#define IOTH_CONFDATA_DHCP6_TIMESTAMP  0x60 // no data
#define IOTH_CONFDATA_DHCP6_SERVERID   0x61 // serverid option copied from the DHCPv6 msg
#define IOTH_CONFDATA_DHCP6_ADDR       0x62 // struct ioth_confdata_ip6addr
#define IOTH_CONFDATA_DHCP6_DNS        0x68 // struct in6_addr (maybe more than one)
#define IOTH_CONFDATA_DHCP6_DOMAIN     0x6a // search list multistring (*).

#define IOTH_CONFDATA_RD6_TIMESTAMP    0x50 // no data
#define IOTH_CONFDATA_RD6_PREFIX       0x51 // struct ioth_confdata_ip6addr
#define IOTH_CONFDATA_RD6_ADDR         0x52 // struct ioth_confdata_ip6addr
#define IOTH_CONFDATA_RD6_ROUTER       0x53 // struct ioth_confdata_ip6addr
#define IOTH_CONFDATA_RD6_MTU          0x5f // uint32_t

/* (*) a multistring is sequence of strings (null char terminated)
	 stored in a buffer one after the other */

#define TIMESTAMP(X) ((X) & 0xf0)

struct ioth;

/* add a record, it the same record stll exists, just update the timestamp */
#define ioth_confdata_add_data(stack, ifindex, type, time, flags, datatype, ...) \
	ioth_confdata_add(stack, ifindex, type, time, flags,\
			&((datatype) { __VA_ARGS__ }), sizeof(datatype))

void ioth_confdata_add(struct ioth *stack, uint32_t ifindex, uint8_t type, time_t time, uint8_t flags,
		void *data, uint16_t datalen);

/* iterate on all selected records:
	 stack/ifindex/type are select keys.
	 stack can be IOTH_CONFDATA_ANYSTACK, ifindex and type can be zero.
	 for each selected record run the callback function "callback"
	 */

#define IOTH_CONFDATA_FORALL_DELETE 0x01
#define IOTH_CONFDATA_FORALL_BREAK  0x02

#define IOTH_CONFDATA_ANYSTACK ((void *) -1)
// all records
#define IOTH_CONFDATA_MASK_ALL 0xff
// all record of a section (static, dhc4, dhcp6, rd)
#define IOTH_CONFDATA_MASK_TYPE 0xf0
// all DNS records
#define IOTH_CONFDATA_DNS_BASE 0x48
#define IOTH_CONFDATA_DNS_MASK 0xce
// all DOMAIN records
#define IOTH_CONFDATA_DOM_BASE 0x4a
#define IOTH_CONFDATA_DOM_MASK 0xce
// all DNS and DOMAIN
#define IOTH_CONFDATA_DNS_DOM_BASE 0x48
#define IOTH_CONFDATA_DNS_DOM_MASK 0xcc

typedef int ioth_confdata_forall_cb(void *data, void *arg);
void ioth_confdata_forall_mask(struct ioth *stack, uint32_t ifindex,
		uint8_t type, uint8_t mask, ioth_confdata_forall_cb *callback,  void *callback_arg);

static inline void ioth_confdata_forall(struct ioth *stack, uint32_t ifindex,
    uint8_t type, ioth_confdata_forall_cb *callback,  void *callback_arg) {
	ioth_confdata_forall_mask(stack, ifindex, type, IOTH_CONFDATA_MASK_ALL,
			callback, callback_arg);
}

/* get methods to retrieve record fields */
struct ioth *ioth_confdata_getstack(void *data);
uint8_t ioth_confdata_gettype(void *data);
uint32_t ioth_confdata_getifindex(void *data);
time_t ioth_confdata_gettimestamp(void *data);
uint16_t ioth_confdata_getdatalen(void *data);

#define IOTH_CONFDATA_ACTIVE 0x01
uint8_t ioth_confdata_setflags(void *data, uint8_t flags);
uint8_t ioth_confdata_clrflags(void *data, uint8_t flags);

/* delete and free (obsolete) records */
void ioth_confdata_free(struct ioth *stack, uint32_t ifindex, uint8_t type, time_t timestamp);

/* read the current timestamp, generate a new timestamp and update the latest timestamp */
time_t ioth_confdata_read_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type);
time_t ioth_confdata_new_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type);
void ioth_confdata_write_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type, time_t timestamp);
void ioth_confdata_del_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type);

/* data formats */
struct ioth_confdata_ip6addr {
	struct in6_addr addr;
	uint8_t prefixlen;
	uint8_t flags;
	uint32_t preferred_lifetime;
	uint32_t valid_lifetime;
};

struct ioth_confdata_ipaddr {
	struct in_addr addr;
	uint8_t prefixlen;
	uint32_t leasetime;
};

#endif
