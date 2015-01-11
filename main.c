#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <errno.h>


#define PNAME_MAX_LEN 32
#define IFACE_MAX_LEN 1024
#define IP_ADDR_LEN 16
#define MAC_ADDR_LEN 18

//#define SEND_ADDRESS 
#define SEND_PORT 8500
#define RCVR_PORT 18502

char program_name[PNAME_MAX_LEN];
char iface[IFACE_MAX_LEN];
char self_ip_address[IP_ADDR_LEN];
char self_mac_address[MAC_ADDR_LEN];

int is_iface(char *if_name) {
	struct ifaddrs *ifs, *tmp;
	int result = 0;
	getifaddrs(&ifs);
	tmp = ifs;
	while(tmp) {
		if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET && strncmp(if_name, tmp->ifa_name, sizeof(if_name)) == 0) {
			strncpy(self_ip_address, inet_ntoa(((struct sockaddr_in *)(tmp->ifa_addr))->sin_addr), sizeof(self_ip_address));
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

int set_mac_address(int socket) {
	struct ifreq ifr;
	int result = 0;
	strncpy(ifr.ifr_name, iface, strnlen(iface, sizeof(iface)) + sizeof(char));
	if((ioctl(socket, SIOCGIFHWADDR, &ifr) == 0) && (ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER)) {
		unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
		sprintf(self_mac_address, "%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		result = 1;
	}
	return result;
}

void print_usage() {
	char buf[] = "\nUsage:\n\t%s <interface>\n\n";
	fprintf(stderr, buf, program_name);
}

void transmitter_func() {
}

int main(int argc, char **argv) {
	int udp_socket;
	struct sockaddr_in reciever, transmitter;

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
	if((udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		fprintf(stderr, "Can't create UDP socket");
		return 3;
	}
	if(!set_mac_address(udp_socket)) {
		shutdown(udp_socket, 2);
		return 4;
	}
    /* now we can do the main task */

	printf("%s\t", iface);
	printf("%s\t", self_ip_address);
	printf("%s\n", self_mac_address);

	shutdown(udp_socket, 2);
	return 0;
}
