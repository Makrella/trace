#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <linux/errqueue.h>
#include <cstring>

using namespace std;


int first_ttl   = 1;
int max_ttl     = 30;
string address;

void help() {

    cerr<<"\033[36m Usage:\n./trace [-f first_ttl] [-m max_ttl] <ip-address>\n"
        "-f first_ttl - Specifies which TTL is used for first packet. Default value 1\n"
        "-m max_ttl   - Specifies maximal value of TTL. Default 30\n"
        "<ip_address  - Specifies the address to send the packet to. IPv4/IPv6 compatible.\033[0m\n";
}

int parser(int argc, char *argv[]) {

    int option = 0, counter = 0;
    char *endpt;
    while (optind < argc) {

        if ((option = getopt(argc, argv, "f:m:")) != -1) {

            switch (option) {

                case 'f': {

                    first_ttl = (int)strtol(optarg, &endpt, 10);
                    if (first_ttl > 30 || first_ttl < 1 || *endpt != '\0') {
                        return 1;
                    }
                    break;
                }

                case 'm': {

                    max_ttl = (int)strtol(optarg, &endpt, 10);
                    if (max_ttl > 30 || max_ttl < 1 || *endpt != '\0'){
                        return 2;
                    }
                    break;
                }

                default: {

                    return 3;
                }
            }
        }

        else {
            if (counter > 1) {
                return 4;
            }
            address = argv[optind];
            optind++;
            counter++;
        }
    }
    return 0;
}


int main(int argc, char *argv[]) {

    if(parser(argc, argv) || address.empty()) {
        help();
        return EXIT_FAILURE;
    }
    short int port = 33434;
    const char * ip_address = address.c_str();
    int socks, recvs, rv;
    struct addrinfo hints,
                    *ret;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    socks = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (getaddrinfo(ip_address, "33434", &hints, &ret)) {
        cerr << "getaddrinfo: "<< gai_strerror(rv) <<endl;
        exit(420);
    }

    sockaddr_in addrListen;

    memset(&addrListen, 0, sizeof(sockaddr_in));
    memcpy(&addrListen, ret->ai_addr, ret->ai_addrlen);

    addrListen.sin_family = AF_INET;
    addrListen.sin_port = htons(port);


    const char* msg = "Smajdova mankaa";
    size_t msg_length = strlen(msg);

    rv = sendto(socks, msg, msg_length, 0, (sockaddr*)&addrListen, sizeof(addrListen));
    cout<<rv<<endl;

    cout<<first_ttl<<endl<<max_ttl<<endl<<address<<endl;
}