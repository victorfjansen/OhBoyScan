#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <iostream>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/if.h>

void getEth0NetworkDetails() {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1) {
    std::cerr << "Failed to open socket for ioctl. ";
    return;
  }

  struct ifreq ifr;
  std::memset(&ifr, 0, sizeof(struct ifreq));
  std::strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

  if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
    struct sockaddr_in *ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
    std::cout << "IP Address : " << inet_ntoa(ipaddr->sin_addr) << "\n";
  } else {
    std::cout << "IP Address : Not assigned / disconnected\n";
  }

  if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  }

  close(sock);
}

void watchNetworkChanges() {
  int nl_socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (nl_socket < 0)
    return;


  struct sockaddr_nl addr;
}