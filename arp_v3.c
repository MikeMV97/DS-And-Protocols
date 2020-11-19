/*
	Codigo basado en una aportacion de alguien mas en la red
	por lo cual no es trabajo 100% mio.
*/

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>  //htons etc

#define PROTO_ARP 0x0806
#define ETH2_HEADER_LEN 14
#define HW_TYPE 1
#define MAC_LENGTH 6
#define IPV4_LENGTH 4
#define ARP_REQUEST 0x01
#define ARP_REPLY 0x02
#define BUF_SIZE 60

#define print(x...) printf(x);printf("\n");
#define print_ret(x,y) {printf(x);printf("\n");return (y);}

struct msgARP {
    unsigned char   destinoEthernet[6];//Dirección de difusión 0xFF
    unsigned char   origenEthernet[6];//Dirección MAC del transmisor
    unsigned short  tipoEthernet;//Tipo de mensaje en la trama Ethernet //__be16
    unsigned short  tipoHardware;//Tipo de hardware utilizado para difundir mensaje ARP (Ethernet)
    unsigned short  tipoProtocolo;//Tipo de protocolo de red utilizado para difundir el mensaje ARP (IP)
    unsigned char   longitudHardware;//Tamaño de direcciones de hardware (6bytes)
    unsigned char   longitudProtocolo;//Tamaño de direcciones del protocolo (4bytes)
    unsigned short  tipoMensaje;// Solicitud o respuesta
    unsigned char   origenMAC[MAC_LENGTH];//Dirección MAC del transmisor
    unsigned char   origenIP[IPV4_LENGTH];//Dirección IP del transmisor
    unsigned char   destinoMAC[MAC_LENGTH];//Dirección MAC del receptor (dirección solicitada)
    unsigned char   destinoIP[IPV4_LENGTH];//Dirección IP del receptor (dato de entrada)
};

struct envoltorio_args{
    int            fd;
    int            ifindex;
    unsigned char *src_mac;
    uint32_t       src_ip;
    uint32_t       dst_ip;
};

/*
 * Converts struct sockaddr with an IPv4 address to network byte order uint32_t.
 * Returns 0 on success.
 */
int int_ip4(struct sockaddr *addr, uint32_t *ip)
{
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *i = (struct sockaddr_in *) addr;
        *ip = i->sin_addr.s_addr;
        return 0;
    } else {
        print_ret("Not AF_INET", 1);
    }
}

/*
 * Formats sockaddr containing IPv4 address as human readable string.
 * Returns 0 on success.
 */
int format_ip4(struct sockaddr *addr, char *out)
{
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *i = (struct sockaddr_in *) addr;
        const char *ip = inet_ntoa(i->sin_addr);
        if (!ip) {
            return -2;
        } else {
            strcpy(out, ip);
            return 0;
        }
    } else {
        return -1;
    }
}

/*
 * Writes interface IPv4 address as network byte order to ip.
 * Returns 0 on success.
 */
int get_if_ip4(int fd, const char *ifname, uint32_t *ip) {
    int err = -1;
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    if (strlen(ifname) > (IFNAMSIZ - 1)) {
        print_ret("Too long interface name",err);
    }

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(fd, SIOCGIFADDR, &ifr) == -1) {
        perror("SIOCGIFADDR");
        return err;
    }

    if (int_ip4(&ifr.ifr_addr, ip) != 0) {
        return err;
    }

    return 0;
}

/*
 * Sends an ARP who-has request to dst_ip
 * on interface ifindex, using source mac src_mac and source ip src_ip.
 */
void* send_arp(void *st_args)
{
    struct envoltorio_args *args = (struct envoltorio_args *)st_args;
    int err = -1;

    unsigned char buffer[BUF_SIZE];
    memset(buffer, 0, sizeof(buffer));

    struct sockaddr_ll socket_address;
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_ARP);
    socket_address.sll_ifindex = args->ifindex;
    socket_address.sll_hatype = htons(ARPHRD_ETHER);
    socket_address.sll_pkttype = (PACKET_BROADCAST);
    socket_address.sll_halen = MAC_LENGTH;
    socket_address.sll_addr[5] = 0x00;
    socket_address.sll_addr[6] = 0x00;
    socket_address.sll_addr[7] = 0x00;

    struct msgARP *arp_req = (struct msgARP *) (buffer);
    ssize_t ret, length = 0;

    //Broadcast
    memset(arp_req->destinoEthernet, 0xff, MAC_LENGTH);

    //Target MAC zero
    memset(arp_req->destinoMAC, 0x00, MAC_LENGTH);

    //Set source mac to our MAC address
    memcpy(arp_req->origenEthernet, args->src_mac, MAC_LENGTH);
    memcpy(arp_req->origenMAC, args->src_mac, MAC_LENGTH);
    memcpy(socket_address.sll_addr, args->src_mac, MAC_LENGTH);

    /* Setting protocol of the packet */
    arp_req->tipoEthernet = htons(ETH_P_ARP);

    /* Creating ARP request */
    arp_req->tipoHardware = htons(HW_TYPE);
    arp_req->tipoProtocolo = htons(ETH_P_IP);
    arp_req->longitudHardware = MAC_LENGTH;
    arp_req->longitudProtocolo = IPV4_LENGTH;
    arp_req->tipoMensaje = htons(ARP_REQUEST);//optocode

    memcpy(arp_req->origenIP, (unsigned char*)&(args->src_ip), 4);
    memcpy(arp_req->destinoIP, (unsigned char*)&(args->dst_ip), 4);

    ret = sendto(args->fd, buffer, BUF_SIZE, 0, (struct sockaddr *) &socket_address, sizeof(socket_address));

    if (ret == -1) {
        perror("sendto():");//return err;
    }
    //return 0;
}

/*
 * Gets interface information by name:
 * IPv4
 * MAC
 * ifindex
 */
int out(int sd){
  if (sd <= 0)
    print("Clean up temporary socket");
    close(sd);
    return -1;
}
int get_if_info(const char *ifname, uint32_t *ip, char *mac, int *ifindex)
{
    //print("get_if_info for %s", ifname);
    int err = -1;
    struct ifreq ifr;
    int sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (sd <= 0) {
        perror("socket()");
        return err;
    }
    if (strlen(ifname) > (IFNAMSIZ - 1)) {
        print("Too long interface name, MAX=%i\n", IFNAMSIZ - 1);
        return err;
    }

    strcpy(ifr.ifr_name, ifname);

    //Get interface index using name
    if (ioctl(sd, SIOCGIFINDEX, &ifr) == -1) {
        perror("SIOCGIFINDEX");
        return out(sd);
    }
    *ifindex = ifr.ifr_ifindex;
    //printf("interface index is %d\n", *ifindex);

    //Get MAC address of the interface
    if (ioctl(sd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("SIOCGIFINDEX");
        return out(sd);
    }

    //Copy mac address to output
    memcpy(mac, ifr.ifr_hwaddr.sa_data, MAC_LENGTH);

    if (get_if_ip4(sd, ifname, ip) != 0) {
        return out(sd);
    }
    print("Configuracion de %s exitosa", ifname);

    return 0;
}

/*
 * Creates a raw socket that listens for ARP traffic on specific ifindex.
 * Writes out the socket's FD.
 * Return 0 on success.
 */
int out_bind(int *fd, int ret){
    if (ret && *fd > 0) {
        print("Cleanup socket");
        close(*fd);
    }
    return ret;
}
int bind_arp(int ifindex, int *fd) {
    //print("bind_arp: ifindex=%i", ifindex);
    // Submit request for a raw socket descriptor.
    *fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (*fd < 1) {
        perror("socket()");
        return out_bind(fd, -1);
    }

//    print("Binding to ifindex %i", ifindex);
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(struct sockaddr_ll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex;
    if (bind(*fd, (struct sockaddr*) &sll, sizeof(struct sockaddr_ll)) < 0) {
        perror("bind");
        return out_bind(fd, -1);
    }
    //print("Socket enlazado en %i", ifindex);
    return 0;
}

/*
 * Reads a single ARP reply from fd.
 * Return 0 on success.
 */
int read_arp(int fd)
{
    print("recvfrom...");
    int ret = -1;
    unsigned char buffer[BUF_SIZE];
    ssize_t length = recvfrom(fd, buffer, BUF_SIZE, 0, NULL, NULL);
    int index;
    if (length == -1) {
        perror("recvfrom()");
        return ret;
    }

    struct msgARP *arp_resp = (struct msgARP *) (buffer);// + ETH2_HEADER_LEN);
    if (ntohs(arp_resp->tipoEthernet) != PROTO_ARP) {
        //print("Not an ARP packet");
        return ret;
    }
    if (ntohs(arp_resp->tipoMensaje) != ARP_REPLY) {
        //print("Not an ARP reply");
        return ret;
    }
    print("\n+ Paquete ARP recibido: ");
    struct in_addr sender_a;
    memset(&sender_a, 0, sizeof(struct in_addr));
    memcpy(&sender_a.s_addr, arp_resp->origenIP, sizeof(uint32_t));
    print("\t-IP origen: %s", inet_ntoa(sender_a));

    print("\t-MAC origen: %02X:%02X:%02X:%02X:%02X:%02X",
          arp_resp->origenMAC[0], arp_resp->origenMAC[1], arp_resp->origenMAC[2],
          arp_resp->origenMAC[3], arp_resp->origenMAC[4], arp_resp->origenMAC[5]);

    return 0;
}

void *recibe_arp(void *arg){
    int *fd = (int *)arg;
    while(1)
        if (read_arp(fd) == 0)  break;
}

/*
 *
 * Sample code that sends an ARP who-has request on
 * interface <ifname> to IPv4 address <ip>.
 * Returns 0 on success.
 */
int ejecutar_arp(char *ifname, int N, char ip[N][15]) {
    int ret = -1, i;
    pthread_t sniffer_thread;

    int src;
    int ifindex;
    char mac[MAC_LENGTH];
    if (get_if_info(ifname, &src, mac, &ifindex) != 0) {
        print("get_if_info failed, interface %s not found or no IP set?", ifname);
        return ret;
    }
    int arp_fd;
    if (bind_arp(ifindex, &arp_fd) != 0) {
        print_ret("Failed to bind_arp()", ret);
    }

    struct envoltorio_args *env[N];// = (struct envoltorio_args **)malloc(sizeof(struct envoltorio_args)*N);
    for(i=0; i<N; i++)
        env[i] = (struct envoltorio_args *)malloc(sizeof(struct envoltorio_args));

    for(i=0; i<N; i++){
      // Para cada Dirección IP enviar y recibir ARP:
        env[i]->dst_ip = inet_addr(ip[i]);

        if (env[i]->dst_ip == 0 || env[i]->dst_ip == 0xffffffff) {
            printf("Invalid source IP\n");
            continue;
        }
        env[i]->fd = arp_fd;
        env[i]->ifindex = ifindex;
        env[i]->src_mac = mac;
        env[i]->src_ip = src;

        if(pthread_create(&sniffer_thread, NULL, &send_arp, (void *)env[i]) < 0){
            printf("Failed to send_arp");
            close(arp_fd);
            return ret;
        }
    }
    pthread_join(sniffer_thread, NULL);
    for(i=0; i<N ; i++){
        if (pthread_create(&sniffer_thread, NULL, &recibe_arp, (void *)arp_fd) < 0) {
          printf("Failed to recv_arp");
          close(arp_fd);
          return ret;
        }
    }
    pthread_join(sniffer_thread, NULL);
    return 0;
}

int main(int argc, char *argv[]) {
    int ret = -1, i;
    if (argc != 3) {
        printf("Ejecute como: %s <INTERFACE> <#Direcciones>\n", argv[0]);
        return 1;
    }

    char *ifname = argv[1];
    int N = atoi(argv[2]);
    char ip[N][15];

    for(i=0 ; i < N ; ++i){
        printf("Ingrese la IP_%d: ", i+1);
        scanf("%s", ip[i]);
    }

    return ejecutar_arp(ifname, N, ip);
}
