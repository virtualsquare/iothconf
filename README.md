# iothconf

## Internet of Threads (IoTh) stack configuration made easy peasy

`iothconf` can use four sources of data to configure an `ioth` stack:

* static data (IPv4 and/or IPv6)
* DHCP (IPv4, RFC 2131 and 6843)
* router discovery (IPv6, RFC 4861)
* DHCPv6 (IPv6, RFC 8415 and 4704)

The API of the iothconf library has two entries:

* `ioth_config`: configure the stack
```C
     int ioth_config(struct ioth *stack, char *config);
```

* `ioth_resolvconf`: return a congiuration string for the domain name resolution library (e.g. [iothdns](
https://github.com/virtualsquare/iothdns). The syntax of the configuration file is consistent with `resolv.conf`(5).
(the string is dynamically allocated: use free(3) to deallocate it).
It returns NULL and errno = 0 if nothing chaned since the previous call. In case of error it returns NULL and errno != 0.

```C
     char *ioth_resolvconf(struct ioth *stack, char *config);
```

## Compile and Install

Pre-requisites: [`libioth`](https://github.com/virtualsquare/libioth).

`iothconf` uses cmake. The standard building/installing procedure is:

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

An uninstaller is provided for your convenience. In the build directory run:
```
sudo make uninstall
```

## examples:

### Create a new IoTh stack
...as described in [libioth](https://github.com/virtualsquare/libioth).
The following example creates an IoTh stack with one interface connected to `vxvde://234.0.0.2`.

```C
     struct ioth *stack = ioth_sewstack("vdestack","vxvde://234.0.0.1");
```

### Configure the stack:

* IPv4 set static IP and gateway

```C
     ioth_config(stack, "eth,ip=10.0.0.1/24,gw=10.0.0.254");
```

* IPv4 dhcp

```C
     ioth_config(stack, "eth,dhcp");
```

* IPv4 and IPv6 using all the available autoconfigurations and set the fully qualified domain name

```C
     ioth_config(stack, "eth,dhcp,dhcp6,rd,fqdn=host.v2.cs.unibo.it");
```

* the same above using the shotcut auto ( = eth,dhcp,dhcp6,rd )

```C
     ioth_config(stack, "auto,fqdn=host.v2.cs.unibo.it");
```

## Options supported by `ioth_config`
 *   `iface=...` : select the interface e.g. `iface=eth0` (default value vde0)
 *   `ifindex=...` : id of the interface (it can be used instead of `iface`)
 *   `fqdn=....` : set the fully qualified domain name for dhcp, dhcpv6 slaac-hash-autoconf
 *   `mac=...` : (or macaddr) define the macaddr for eth here below. (e.g. `eth,mac=10:a1:b2:c3:d4:e5`)
 *   `eth` : turn on the interface (and set the MAC address if requested  or a hash based MAC address if fqdn is defined)
 *   `dhcp` : (or dhcp4 or dhcpv4) use dhcp (IPv4)
 *   `dhcp6` : (or dhcpv6) use dhcpv6 (for IPv6)
 *   `rd` : (or rd6) use the router discovery protocol (IPv6)
 *   `slaac` : use stateless auto-configuration (IPv6) (requires rd)
 *   `auto` : shortcut for eth+dhcp+dhcp6+rd
 *   `auto4` : (or autov4) shortcut for eth+dhcp
 *   `auto6` : (or autov6) shortcut for eth+dhcp6+rd
 *   `ip=..../..` : set a static address IPv4 or IPv6 and its prefix length example: `ip=10.0.0.100/24`  or `ip=2001:760:1:2::100/64`
 *   `gw=.....` : set a static default route IPv4 or IPv6
 *   `dns=....` : set a static address for a DNS server
 *   `domain=....` : set a static domain for the dns search
 *   `debug` : show the status of the current configuration parameters
 *   `-static, -eth, -dhcp, -dhcp6, -rd, -auto, -auto4, -auto6` (and all the synonyms + a heading minus) clean (undo) the configuration

## TODO: missing features

* `thread` option to start a thread for leases renewal.
