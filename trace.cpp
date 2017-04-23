#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <linux/errqueue.h>
#include <linux/icmp.h>
#include <sys/time.h>

using namespace std;

#define ERR_IPADDR  5
#define BAD_IP      6

int first_ttl   = 1;    //default values of first and max ttl
int max_ttl     = 30;   //in case the arguments are not given
string address;         //string which will hold address

//function which prints out usage of application
void help() {

    cerr<<"\033[36m Usage:\n./trace [-f first_ttl] [-m max_ttl] <ip-address>\n"
        "-f first_ttl - Specifies which TTL is used for first packet. Default value 1\n"
        "-m max_ttl   - Specifies maximal value of TTL. Default 30\n"
        "<ip_address  - Specifies the address to send the packet to. IPv4/IPv6 compatible.\033[0m\n";
}

//simple parser function
//self-explaining
//returns
//0 - correct
//1 - first_ttl argument error
//2 - max_ttl argument error
//3 - bad argument option
//4 - more arguments than needed
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

    //variable for exit and return values
    int return_value;

    /*verify correct arguments, and if any IP is given
     * exit codes imported from parser function
     * exit 5 - IP address not given*/

    if(parser(argc, argv) || address.empty()) {
        help();
        return_value = parser(argc, argv);
        if(return_value)
            return return_value;
        else
            return ERR_IPADDR;
    }

    //variables to measure time
    timeval time;
    double time_sent, time_recv, total;

    //structure to measure timeout
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    //variable for socket
    int sent_soc;
    struct addrinfo hints;
    struct addrinfo* results;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if((retval = getaddrinfo(address.c_str(), "33434", &hints, &results)) != 0) {
        cerr << "getaddrinfo: "<< gai_strerror(retval) <<endl;
        return 80;
    }

    if((sent_soc = socket(results->ai_family, results->ai_socktype, IPPROTO_UDP)) == 0) {
        return 81;
    }

    int optval = 1;
    if (results->ai_family == AF_INET) {
        if ((setsockopt(sent_soc, SOL_IP, IP_RECVERR, (char*)&optval, sizeof(optval)))) {
            printf("Error setting IPV4_RECVERR\n");
        }
    } else if (results->ai_family == AF_INET6) {
        if ((setsockopt(sent_soc, SOL_IPV6, IPV6_RECVERR, (char*)&optval, sizeof(optval)))) {
            printf("Error setting IPV6_RECVERR\n");
        }
    }

    for (int i = first_ttl; i <= max_ttl ; i++) {
        if (results->ai_family == AF_INET) {
            setsockopt(sent_soc, IPPROTO_IP, IP_TTL, &i, sizeof(i));
        } else {
            setsockopt(sent_soc, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &i, sizeof(i));
        }

        const char *socket_msg = "Smajdova manka";
        size_t msg_length = strlen(socket_msg);

        retval = (int)sendto(sent_soc, socket_msg, msg_length, 0, results->ai_addr, results->ai_addrlen);

        gettimeofday(&time, NULL);
        time_sent = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);

        fd_set FDS;
        FD_ZERO(&FDS);
        FD_SET(sent_soc, &FDS);

        retval = select(FD_SETSIZE, &FDS, NULL, NULL, &timeout);
        if(retval == -1) {
            cout<<"chyba"<<endl;

        } else if (retval == 0) {
            cout<<i<<"\t\t*"<<endl;

        } else {
            //cout<<"select uspesny, prijalo sa "<<i<<endl;

            char cbuf[512];
            char host[512]; //buffer pre odpoveď
            char service[20];

            struct msghdr msg; //prijatá správa - môže obsahovať viac control hlavičiek
            struct cmsghdr *cmsg;

            //štruktúra pre adresu kompatibilná s IPv4 aj v6
            struct sockaddr_storage target;

            msg.msg_name = &target; //tu sa uloží cieľ správy, teda adresa nášho stroja
            msg.msg_namelen = sizeof(target); //obvious
            msg.msg_iov = NULL; //ICMP hlavičku reálne nepríjmeme - stačia controll správy
            msg.msg_iovlen = 0; //veľkosť štruktúry pre hlavičky - žiadna
            msg.msg_flags = 0; //žiadne flagy
            msg.msg_control = cbuf; //predpokladám že buffer pre control správy
            msg.msg_controllen = sizeof(cbuf);//obvious

            retval = (int)recvmsg(sent_soc, &msg, MSG_ERRQUEUE); //prijme správu

            for (cmsg = CMSG_FIRSTHDR(&msg);cmsg; cmsg =CMSG_NXTHDR(&msg, cmsg))
            {
                /* skontrolujeme si pôvod správy - niečo podobné nám bude treba aj pre IPv6 */
                if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR)
                {
                    //získame dáta z hlavičky
                    struct sock_extended_err *e = (struct sock_extended_err*) CMSG_DATA(cmsg);

                    //bude treba niečo podobné aj pre IPv6 (hint: iný flag)
                    if (e && e->ee_origin == SO_EE_ORIGIN_ICMP) {

                        /* získame adresu - ak to robíte všeobecne tak sockaddr_storage */
                        struct sockaddr_in *sin = (struct sockaddr_in *)(e+1);

                        /*
                        * v sin máme zdrojovú adresu
                        * stačí ju už len vypísať viď: inet_ntop alebo getnameinfo
                        */
                        gettimeofday(&time, NULL);
                        time_recv = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);
                        total = time_recv - time_sent;

                        char r_addr[INET_ADDRSTRLEN];
                        inet_ntop(PF_INET, &sin->sin_addr, r_addr, INET_ADDRSTRLEN);
                        printf("%2d\t\t%s\t\t%.3f\tms\n", i, r_addr, total);



                        if (e->ee_type == ICMP_TIME_EXCEEDED) {
                            break;
                        } else if (e->ee_type == ICMP_DEST_UNREACH) {
                            cout<<"ITS TIME TO STOP"<<endl;
                            return 420;
                            /*
                            * Overíme si všetky možné návratové správy
                            * hlavne ICMP_TIME_EXCEEDED and ICMP_DEST_UNREACH
                            * v prvom prípade inkrementujeme TTL a pokračujeme
                            * v druhom prípade sme narazili na cieľ
                            *
                            * kódy pre IPv4 nájdete tu
                            * http://man7.org/linux/man-pages/man7/icmp.7.html
                            *
                            * kódy pre IPv6 sú ODLIŠNÉ!:
                            * nájdete ich napríklad tu https://tools.ietf.org/html/rfc4443
                            * strana 4
                            */
                        }
                    }
                }
            }
        }
    }

    //cout<<first_ttl<<endl<<max_ttl<<endl<<address<<endl;
}