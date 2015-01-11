#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>

#define PNAME_MAX_LEN 32
#define IFACE_MAX_LEN 1024

char program_name[PNAME_MAX_LEN];
char iface[IFACE_MAX_LEN];

int is_iface(char *if_name) {
	struct ifaddrs *ifs, *tmp;
	int result = 0;
	getifaddrs(&ifs);
	tmp = ifs;
	while(tmp) {
		if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET && strncmp(if_name, tmp->ifa_name, sizeof(iface)) == 0) {
			result = 1;
			break;
		}
		tmp = tmp->ifa_next;
	}
	freeifaddrs(ifs);
	return result;
}

void set_program_name(char *pname) {
	memset((void *)program_name, 0, sizeof(program_name));
	strncpy(program_name, pname, sizeof(program_name) - sizeof(char));
}

void set_iface_name(char *arg) {
	memset((void *)iface, 0, sizeof(iface));
	strncpy(iface, arg, sizeof(iface) - sizeof(char));
}

void print_usage() {
	char buf[] = "\nUsage:\n\t%s <interface>\n\n";
	fprintf(stderr, buf, program_name);
}

int main(int argc, char **argv) {
	set_program_name(basename(argv[0]));
	if(argc != 2) { /* if not one argument */
		print_usage();
		return 1;
	}
	set_iface_name(argv[1]);
	if(is_iface(iface) == 0) { /* check if iface is in system interfaces list */
		fprintf(stderr, "%s is not interface\n", iface);
		print_usage();
		return 2;
	}
    /* now we can do the main task */
	printf("%s\n", iface);

	return 0;
}
