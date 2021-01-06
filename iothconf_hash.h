#ifndef IOTHCONF_HASH_H
#define IOTHCONF_HASH_H

void iothconf_hashaddr6(void *addr, const char *name);
void iothconf_hashmac(void *mac, const char *name);
void iothconf_eui64(void *addr, void *mac);

#endif
