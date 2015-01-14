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

#include "checksum.h"

//#pragma pack(2)

#define PNAME_MAX_LEN 32
#define IFACE_MAX_LEN 1024
#define IP_ADDR_LEN 16
#define MAC_ADDR_LEN 18
#define MAC_BIN_LEN 6
#define PORT_STR_LEN 6

//#define SEND_ADDRESS 
#define SEND_PORT 5800
#define SEND_SRC_PORT 15800
#define RCVR_PORT 15802

struct send_data {
	u_int16_t w1; // 1 
	u_int16_t w2; // 1
	u_int16_t w3; // 0
	u_int16_t w4; // 32
	u_int16_t unknown1[6]; // zeros
	u_int8_t mac_bin[MAC_BIN_LEN];
	u_int16_t unknown2[5]; // zeros
	u_int16_t w5; // 1
	u_int16_t w6; // 0x1c
	u_int16_t w7; // 0
	u_int16_t w8; // 1
	u_int16_t w9; // 0x16
	char ip_str[IP_ADDR_LEN];
	char port_str[PORT_STR_LEN];
} __attribute__ ((packed));

char program_name[PNAME_MAX_LEN];
char iface[IFACE_MAX_LEN];
char self_ip_address[IP_ADDR_LEN];
char self_mac_address_str[MAC_ADDR_LEN];
char self_mac_address_bin[MAC_BIN_LEN];

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
	int result = 0, i;
	strncpy(ifr.ifr_name, iface, strnlen(iface, sizeof(iface)) + sizeof(char));
	if((ioctl(socket, SIOCGIFHWADDR, &ifr) == 0) && (ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER)) {
		unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
		sprintf(self_mac_address_str, "%02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		for(i = 0; i < MAC_BIN_LEN; i++) {
			self_mac_address_bin[i] = mac[i];
		}
		memcpy(self_mac_address_bin, mac, sizeof(self_mac_address_bin));
		result = 1;
	}
	return result;
}

void print_usage() {
	char buf[] = "\nUsage:\n\t%s <interface>\n\n";
	fprintf(stderr, buf, program_name);
}

void *reciever_func(void *arg) {
	
}

void fill_data(struct send_data *data) {
	memset((void *)data, 0, sizeof(struct send_data));
	data->w1 = 256;
	data->w2 = 256;
	data->w4 = 8192;
	data->w5 = 256;
	data->w6 = 0x1c00;
	data->w8 = 256;
	data->w9 = 0x1600;
	memcpy(data->mac_bin, self_mac_address_bin, sizeof(data->mac_bin));
	strncpy(data->ip_str, self_ip_address, sizeof(data->ip_str)-sizeof(char));
	sprintf(data->port_str, "%d", RCVR_PORT);
}

int main(int argc, char **argv) {
	int send_sock;
	struct send_data data;
	struct sockaddr_in reciever, src_addr, dst_addr;
	int one = 1;

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
	if((send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "Can't create UDP socket");
		return 3;
	}
	if(setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0) // || setsockopt(send_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
	{
		fprintf(stderr, "Can't set socket options");
		return 3;
	}
	if(!set_mac_address(send_sock)) {
		shutdown(send_sock, 2);
		return 4;
	}

	fill_data(&data);

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.sin_family = AF_INET;
	src_addr.sin_port = htons(SEND_SRC_PORT);
	src_addr.sin_addr.s_addr = inet_addr(self_ip_address);

	if(bind(send_sock, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
		shutdown(send_sock, 2);
		return 5;
	}

	memset(&dst_addr, 0, sizeof(dst_addr));
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = htons(SEND_PORT);
	dst_addr.sin_addr.s_addr = htonl(-1); // 255.255.255.255

	int res = 0;
	if ((res = sendto(send_sock, &data, sizeof(data), 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr))) < 0) {
		shutdown(send_sock, 2);
		return 6;
	}

	printf("sent %i octets\n", res);

	shutdown(send_sock, 2);
	return 0;
}
