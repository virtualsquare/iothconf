<!--
.\" Copyright (C) 2022 VirtualSquare. Project Leader: Renzo Davoli
.\"
.\" This is free documentation; you can redistribute it and/or
.\" modify it under the terms of the GNU General Public License,
.\" as published by the Free Software Foundation, either version 2
.\" of the License, or (at your option) any later version.
.\"
.\" The GNU General Public License's references to "object code"
.\" and "executables" are to be interpreted as the output of any
.\" document formatting or typesetting system, including
.\" intermediate and printed output.
.\"
.\" This manual is distributed in the hope that it will be useful,
.\" but WITHOUT ANY WARRANTY; without even the implied warranty of
.\" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
.\" GNU General Public License for more details.
.\"
.\" You should have received a copy of the GNU General Public
.\" License along with this manual; if not, write to the Free
.\" Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
.\" MA 02110-1301 USA.
.\"
-->

# NAME
ioth_config, ioth_resolvconf, ioth_newstackc -- Internet of Threads stack configuration library

# SYNOPSIS
`#include <iothconf.h>`

`int ioth_config(struct ioth *`_stack_`, char *`_config_`);`

`struct ioth *ioth_newstackc(const char *`_stack_config_`);`

`char *ioth_resolvconf(struct ioth *`_stack_`, char *`_config_`);`

These functions are provided by libiothconf. Link with -liothconf.

# DESCRIPTION

The `iothconf` library provides a simple way to configure Internet of Threads
networking stacks. All the configuration parameters are specified by a character string.
`iothconf` can use four sources of data to configure an `ioth` stack:

* static data (IPv4 and/or IPv6) \
* DHCP (IPv4, RFC 2131 and 6843) \
* router discovery (IPv6, RFC 4861) \
* DHCPv6 (IPv6, RFC 8415 and 4704)

  `ioth_config`
: `ioth_config` configures the stack whose descriptor is _stack_ using the parameters
written in _config_.

  `ioth_newstackc`
: `ioth_newstackc` is a shortcut to create a stack and configure it. It is equivalent
to a sequence `ioth_newstack` and `ioth_config`.

  `ioth_resolvconf`
: `ioth_resolvconf` retrieves a configuration string for the domain name resolution library.

## Configuration strings syntax

`ioth_config` configuration string is a comma separated list of flags and variable assignments:

 * `iface=...` : select the interface e.g. `iface=eth0` (default value `vde0`) \
 * `ifindex=...` : id of the interface (it can be used instead of iface) \
 * `fqdn=...`. : set the fully qualified domain name for dhcp, dhcpv6 slaac-hash-autoconf \
 * `mac=...` : (or `macaddr`) define the macaddr for eth here below.  (e.g. `eth,mac=10:a1:b2:c3:d4:e5`) \
 * `eth` : turn on the interface (and set the MAC address if requested or a hash based MAC address if fqdn is defined) \
 * `dhcp` : (or `dhcp4` or `dhcpv4`) use dhcp (IPv4) \
 * `dhcp6` : (or `dhcpv6`) use dhcpv6 (for IPv6) \
 * `rd` : (or `rd6`) use the router discovery protocol (IPv6) \
 * `slaac` : use stateless auto-configuration (IPv6) (requires rd) \
 * `auto` : shortcut for `eth,dhcp,dhcp6,rd` \
 * `auto4` : (or `autov4`) shortcut for `eth,dhcp` \
 * `auto6` : (or `autov6`) shortcut for `eth,dhcp6,rd` \
 * `ip=..../..` : set a static address IPv4 or IPv6 and its prefix length example: `ip=10.0.0.100/24` or `ip=2001:760:1:2::100/64` \
 * `gw=.....` : set a static default route IPv4 or IPv6 \
 * `dns=....` : set a static address for a DNS server \
 * `domain=....` : set a static domain for the dns search

`ioth_newstackc` supports all the options of ioth_config plus:

* `stack=...` : to select the stack implementation; \
* `vnl=...` : to select the VDE network.

`ioth_resolvconf` configuration string is a comma separated list of variable assignments:

 * `iface=...` : select the interface e.g. `iface=eth0` (default value `vde0`) \
 * `ifindex=...` : id of the interface (it can be used instead of iface)

# RETURN VALUE

`ioth_config` returns -1 in case of error and or a mask of the following bits:
each bit is set on if the corresponding configuration succeeded:

* `IOTHCONF_STATIC`: static IP \
* `IOTHCONF_ETH`: Ethernet  \
* `IOTHCONF_DHCP`: DHCPv4 \
* `IOTHCONF_DHCPV6`: DHCPv6 \
* `IOTHCONF_RD`: neighbor discovery (router advertisement).

`ioth_newstackc` returns the IoTh descriptor, NULL in case of error

`ioth_resolvconf` returns a configuration string for the domain name resolution library.
The syntax of the returned string is consistent with `resolv.conf`(5).
(the string is dynamically allocated: use `free`(3) to deallocate it)
It returns NULL and errno = 0 if nothing changed since the previous call.
In case of error it returns NULL and errno != 0

# SEE ALSO

ioth(3)

# AUTHOR

VirtualSquare. Project leader: Renzo Davoli

