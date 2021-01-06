#ifndef IOTHCONF_DNS_H
#define IOTHCONF_DNS_H
#include <stdint.h>
#include <string.h>

/* convert an rfc1035 encoded sequence of domain names of length len
	 in multistring (sequence of strings).
	 The result is stored in mstr (whose length must be at least len) */
int iothconf_domain2mstr(uint8_t *domain, char *mstr, uint16_t len);

/* iterate on the strings of a multistring.
	 X = iteration varname
	 Y = multistring
	 LEN = length of Y */

#define FORmstr(X, Y, LEN) \
	for(char *X = Y; X < ((char *) Y) + LEN; X += strlen(X) + 1)

/* return the # of strings stored in the multistring */
static inline int count_mstr(char *mstr, int len) {
	int count = 0;
	FORmstr(__, mstr, len)
		count++;
	return count;
}
#endif
