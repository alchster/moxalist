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
#include <pthread.h>
#include <unistd.h>

#define PNAME_MAX_LEN 32
#define IFACE_MAX_LEN 1024
#define IP_ADDR_LEN 16
#define MAC_ADDR_LEN 18
#define MAC_BIN_LEN 6
#define PORT_STR_LEN 6

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

struct recv_data {
	u_int8_t unknown1[46];
	char model[33];
	char ip[IP_ADDR_LEN];
	char mac[MAC_ADDR_LEN];
	char mask[IP_ADDR_LEN];
	char gw[IP_ADDR_LEN];
	char unknown2[32];
	char to_ip[IP_ADDR_LEN];
	char fw[25];
} __attribute__ ((packed));

struct access_point {
	struct access_point *next;
	char ip[IP_ADDR_LEN];
	char mask[IP_ADDR_LEN];
	char gw[IP_ADDR_LEN];
	char mac[MAC_ADDR_LEN];
	char fw[25];
};

char program_name[PNAME_MAX_LEN];
char iface[IFACE_MAX_LEN];
char self_ip_address[IP_ADDR_LEN];
char self_mac_address_str[MAC_ADDR_LEN];
char self_mac_address_bin[MAC_BIN_LEN];
static int stop_thread = 0;
static struct access_point *first_ap = NULL;
static struct access_point *curr_ap = NULL;
static const size_t ap_size = sizeof(struct access_point);


struct access_point *find_ap(const char *mac) {
	struct access_point *curr = first_ap, *result = NULL;
	while(curr) {
		if(strncmp(mac, curr->mac, MAC_ADDR_LEN) == 0) {
			result = curr;
			break;
		}
		curr = curr->next;
	}
	return result;
}

void add_access_point(const struct recv_data *ap_info) {
	struct access_point *curr;
	if(!find_ap(ap_info->mac)) {
		curr = malloc(ap_size);
		memset(curr, 0, ap_size);
		if(!curr_ap) {
			first_ap = curr;
		}
		else {
			curr_ap->next = curr;
		}
		curr_ap = curr;
		size_t ip_len = IP_ADDR_LEN*sizeof(char);
		memcpy(curr_ap->ip, ap_info->ip, ip_len);
		memcpy(curr_ap->mask, ap_info->mask, ip_len);
		memcpy(curr_ap->gw, ap_info->gw, ip_len);
		memcpy(curr_ap->mac, ap_info->mac, MAC_ADDR_LEN*sizeof(char));
		memcpy(curr_ap->fw, ap_info->fw, 25*sizeof(char));
	}
}

void print_and_free_aps_list () {
	struct access_point *curr;
	struct access_point *next = first_ap;
	while(next) {
		curr = next;
		printf("%s\t%s\t%s\t%s\t\%s\n", curr->mac, curr->ip, curr->mask, curr->gw, curr->fw);
		next = curr->next;
		free(curr);
	}
}

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

void *receiver_func(void *arg) {
	int rcvr_sock;
	struct sockaddr_in receiver;
	struct recv_data buffer;
	int read;
	size_t len = sizeof(receiver);
	struct timeval tv = { 5, 0 }; // wait 5 seconds for data
	
	memset(&receiver, 0, sizeof(receiver));
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(RCVR_PORT);
	receiver.sin_addr.s_addr = htonl(INADDR_ANY);

	if((rcvr_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "Can't create UDP socket (reciever)");
		pthread_exit(NULL);
	}
	if(setsockopt(rcvr_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		fprintf(stderr, "Can't set socket options (reciever)");
		pthread_exit(exit);
	}
	if(bind(rcvr_sock, (struct sockaddr *)&receiver, len) < 0) {
		fprintf(stderr, "Can't bind socket (reciever)");
		pthread_exit(exit);
	}

	while(!stop_thread) {
		memset((void *)&buffer, 0, sizeof(buffer));
		read = recvfrom(rcvr_sock, &buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver, &len);
		if(read > 0) {
	//		printf("%s\t%s\t%s\t%s\t\%s\n", buffer.mac, buffer.ip, buffer.mask, buffer.gw, buffer.fw);
			add_access_point(&buffer);
		}
	}
	shutdown(rcvr_sock, 2);
	return NULL;
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
	struct sockaddr_in src_addr, dst_addr;
	int one = 1;
	pthread_t receiver_thread;

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
	if(setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0) {
		fprintf(stderr, "Can't set socket options");
		return 3;
	}
	if(!set_mac_address(send_sock)) {
		shutdown(send_sock, 2);
		return 4;
	}
	/* receiver */
	if(pthread_create(&receiver_thread, NULL, receiver_func, NULL)) {
		fprintf(stderr, "Can't create receiver thread");
		shutdown(send_sock, 2);
		return 5;
	}
	usleep(500000);

	/* sender */
	fill_data(&data);

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.sin_family = AF_INET;
	src_addr.sin_port = htons(SEND_SRC_PORT);
	src_addr.sin_addr.s_addr = inet_addr(self_ip_address);

	if(bind(send_sock, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
		shutdown(send_sock, 2);
		return 6;
	}

	memset(&dst_addr, 0, sizeof(dst_addr));
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = htons(SEND_PORT);
	dst_addr.sin_addr.s_addr = htonl(-1); // 255.255.255.255

	int i;
	for(i = 0; i < 3; i++) {
//		printf("sending...\n");
		if (sendto(send_sock, &data, sizeof(data), 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr)) < 0) {
			fprintf(stderr, "Can't send UDP message");
		}
		sleep(1);
	}

	stop_thread = 1;
	void *res;
	pthread_join(receiver_thread, &res);
	shutdown(send_sock, 2);

	print_and_free_aps_list();
	return 0;
}
