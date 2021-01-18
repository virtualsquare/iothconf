#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
#include <ioth.h>
#include <iothconf.h>

static void usage(char *progname) {
	fprintf(stderr, 
			"Usage: %s OPTIONS [config_str]\n"
			"OPTIONS:\n"
			" -s --stack:       ioth stack implementation (default kernel)\n"
			" -v --vnl:         vde's virtual network locator\n"
			" -i --interactive: interctive mode\n"
			" -h, --help:       usage message\n", progname);
	exit(1);
}

static void configure(struct ioth *stack, char *config) {
	if (strcmp(config, "rc") == 0) {
      char *rc = ioth_resolvconf(stack, NULL);
      if (rc) {
        printf("----\n%s----\n", rc);
        free(rc);
      } else
        perror("rc");
    } else {
      int rv = ioth_config(stack, config);
      if (rv < 0)
        perror("ioth_config");
			else {
				printf("ioth_config confirmed:");
				if (rv & IOTHCONF_STATIC) printf(" static");
				if (rv & IOTHCONF_ETH) printf(" eth");
				if (rv & IOTHCONF_DHCP) printf(" dhcp");
				if (rv & IOTHCONF_DHCPV6) printf(" dhcpv6");
				if (rv & IOTHCONF_RD) printf(" rd");
				printf("\n");
			}
    }
}

int main(int argc, char *argv[]) {
	char *progname = basename(argv[0]);
	static char *short_options = "s:iv:h";
	static struct option long_options[] = {
                   {"stack",   required_argument, 0,  's' },
                   {"interactive",   no_argument, 0,  'i' },
                   {"vnl",     required_argument, 0,  'v' },
                   {"help",          no_argument, 0,  'h' },
                   {0,         0,                 0,  0 }
               };

	char *stacklib = NULL;
	char *vnl = NULL;
	int interactive = 0;
	int c;
	while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) >= 0) {
		switch (c) {
			case 's': stacklib = optarg; break;
			case 'i': interactive = 1; break;
			case 'v': vnl = optarg; break;
			case '?':
			case 'h':
			default: usage(progname); break;
		}
	}

	struct ioth *stack = ioth_newstack(stacklib, vnl);
	if (stack == NULL) {
		perror("stack");
		exit(1);
	}
	if (interactive) {
		for (;;) {
			char buf[1024];
			printf("> "); fflush(stdout);
			if (fgets(buf, 1024, stdin)  == NULL)
				return 0;
			size_t len = strlen(buf);
			if (buf[len - 1] == '\n')
				buf[--len] = 0;
			configure(stack, buf);
		} 
	} else
		configure(stack, argv[optind]);
}
