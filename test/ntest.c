#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ioth.h>
#include <iothconf.h>

int main(int argc, char *argv[]) {
	struct ioth *stack = ioth_newstack(argv[1], argv[2]);
	for (;;) {
		char buf[1024];
		printf("> "); fflush(stdout);
		if (fgets(buf, 1024, stdin)  == NULL)
			return 0;
		size_t len = strlen(buf);
		if (buf[len - 1] == '\n')
			buf[--len] = 0;
		if (strcmp(buf, "rc") == 0) {
			char *rc = ioth_resolvconf(stack, NULL);
			if (rc) {
				printf("----\n%s----\n", rc);
				free(rc);
			} else
				perror("rc");
		} else {
			int rv = ioth_config(stack, buf);
			if (rv < 0)
				perror("ioth_config");
		}
	}
}
