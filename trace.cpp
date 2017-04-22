#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>

#define ERR_SOCKET      45
#define ERR_ADDRINFO    46

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


    char cbuf[512];

    struct msghdr msg;
    struct cmsghdr *cmsg;

    struct sockaddr_storage target;

    msg.msg_name = &target; //tu sa uloží cieľ správy, teda adresa nášho stroja
    msg.msg_namelen = sizeof(target); //obvious
    msg.msg_iov = NULL; //ICMP hlavičku reálne nepríjmeme - stačia controll správy
    msg.msg_iovlen = 0; //veľkosť štruktúry pre hlavičky - žiadna
    msg.msg_flags = 0; //žiadne flagy
    msg.msg_control = cbuf; //predpokladám že buffer pre control správy
    msg.msg_controllen = sizeof(cbuf);

    const short int port = 33434;
    const char * ip_address = address.c_str();

    int sent_socket, rets, retval;
    struct addrinfo hints, *ret_addrinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    fd_set FDS;


    if ((retval = getaddrinfo(ip_address, "33434", &hints, &ret_addrinfo)) != 0) {
        cerr << "getaddrinfo: "<< gai_strerror(retval) <<endl;
        return ERR_ADDRINFO;
    }

    sockaddr_in addrSend;

    if (ret_addrinfo->ai_family == AF_INET6) {
        if((sent_socket = socket(ret_addrinfo->ai_family, ret_addrinfo->ai_socktype, IPPROTO_UDP)) == 0) {
            return ERR_SOCKET;
        }



        sockaddr_in6 addrSend;

        memset(&addrSend, 0, sizeof(sockaddr_in));
        memcpy(&addrSend, ret_addrinfo->ai_addr, ret_addrinfo->ai_addrlen);

        addrSend.sin6_family = AF_INET6;
        addrSend.sin6_port = htons(port);
    }

    else {
        if((sent_socket = socket(ret_addrinfo->ai_family, ret_addrinfo->ai_socktype, IPPROTO_UDP)) == 0) {
            return ERR_SOCKET;
        }


        sockaddr_in addrSend;

        memset(&addrSend, 0, sizeof(sockaddr_in));
        memcpy(&addrSend, ret_addrinfo->ai_addr, ret_addrinfo->ai_addrlen);

        addrSend.sin_family = AF_INET;
        addrSend.sin_port = htons(port);
    }



    const char* messg = "Smajdova mankaa";
    size_t msg_length = strlen(messg);

    int optval = 1;
    if (ret_addrinfo->ai_family == AF_INET) {
        if ((setsockopt(sent_socket, SOL_IP, IP_RECVERR, (char*)&optval, sizeof(optval)))) {
            printf("Error setting IPV4_RECVERR\n");
        }
    } else if (ret_addrinfo->ai_family == AF_INET6) {
        if ((setsockopt(sent_socket, SOL_IPV6, IPV6_RECVERR, (char*)&optval, sizeof(optval)))) {
            printf("Error setting IPV6_RECVERR\n");
        }
    }

    if(connect(sent_socket, ret_addrinfo->ai_addr, ret_addrinfo->ai_addrlen) == -1){
        cout<<"SHIEEET"<<endl;
        exit(7);
    }


    /*if ((bind(sent_socket, (sockaddr*)&addrSend, sizeof(addrSend))) == -1)
        return 47;*/
    for (int i = first_ttl; i <= max_ttl; i++) {

        FD_ZERO(&FDS);
        FD_SET(sent_socket, &FDS);

        setsockopt(sent_socket, IPPROTO_IP, IP_TTL, &i, sizeof(i));


        retval = (int) sendto(sent_socket, messg, msg_length, 0, ret_addrinfo->ai_addr, ret_addrinfo->ai_addrlen);
        cout << retval << ": send to OK"<<endl;


        while (1) {
            retval = select(sizeof(FDS), &FDS, NULL, NULL, &timeout);
            cout << i << ": iteracia" << endl;
            cout << retval << ": Select" << endl;
            if (retval == -1) {
                return 50;
            }  else if (retval == 0) {
                cout << "Timeout" << endl <<"----------------------------------"<<endl;
                break;
            } else if (retval >= 1) {
                cout << "Daco doslo" << endl<<endl;
                /*if ((retval = recvmsg(sent_socket, &msg, MSG_ERRQUEUE)) == -1) {
                    cout << "rekt" << i << endl;
                    exit(5);
                }*/
                rets = recvmsg(sent_socket, &msg, MSG_ERRQUEUE);
                cout << "rets: "<< rets << endl;
                if(rets < 0)
                {
                    cout<<"error prijimania"<<endl;
                    exit(10);
                }
                cout<<"not kek"<<endl;
                

            }

        }
    }

    cout<<first_ttl<<endl<<max_ttl<<endl<<address<<endl;
}