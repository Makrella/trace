#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/icmp6.h>
#include <linux/errqueue.h>
#include <linux/icmp.h>
#include <sys/time.h>

using namespace std;

//exit codes
#define ERR_IPADDR              5
#define ERR_GETADDRINFO         6
#define ERR_SETSNDSOCK          7
#define ERR_SETSOCKRECVERR      8
#define ERR_SENDTO              9
#define ERR_SLCTERR             10
#define ERR_RECV                11
#define HOST_UNREACH            12
#define NET_UNREACH             13
#define PROT_UNREACH            14
#define PKT_FILTERED            15

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

    
    //variable for socket
    int sent_soc;

    //structures for getaddrinfo function, defining IPv4 or IPv6, type of socket, port,..
    struct addrinfo hints;
    struct addrinfo* results;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    //getaddrinfo function, no explanation needed
    if((return_value = getaddrinfo(address.c_str(), "33434", &hints, &results)) != 0) {
        cerr << "getaddrinfo - "<< address << ": " << gai_strerror(return_value) <<endl;
        return ERR_GETADDRINFO;
    }

    //setting up socket with information from getaddrinfo
    if((sent_soc = socket(results->ai_family, results->ai_socktype, IPPROTO_UDP)) == 0) {
        return ERR_SETSNDSOCK;
    }

    //setting socket to receive errors when receiving messages back, handling of IPv4 and 6 differences
    int optval = 1;
    if (results->ai_family == AF_INET) {
        if ((setsockopt(sent_soc, SOL_IP, IP_RECVERR, (char*)&optval, sizeof(optval)))) {
            printf("Error setting IPV4_RECVERR\n");
            return ERR_SETSOCKRECVERR;
        }
    } else if (results->ai_family == AF_INET6) {
        if ((setsockopt(sent_soc, SOL_IPV6, IPV6_RECVERR, (char*)&optval, sizeof(optval)))) {
            printf("Error setting IPV6_RECVERR\n");
            return ERR_SETSOCKRECVERR;
        }
    }


    //cycle and increment ttls from first ttl to max ttl
    for (int i = first_ttl; i <= max_ttl ; i++) {

        //structure to measure timeout
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        //set TTL of socket - IPv4 and IPv6 difference
        if (results->ai_family == AF_INET) {
            setsockopt(sent_soc, IPPROTO_IP, IP_TTL, &i, sizeof(i));
        } else {
            setsockopt(sent_soc, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &i, sizeof(i));
        }

        /*short message to be sent with packets
                  _              _        _
                 | |            | |      | |
                 | |_ ___  _ __ | | _____| | __
                 | __/ _ \| '_ \| |/ / _ \ |/ /
                 | || (_) | |_) |   <  __/   <
                  \__\___/| .__/|_|\_\___|_|\_\
                          | |
                          |_|
         */

        const char *socket_msg = "https://www.youtube.com/watch?v=oHg5SJYRHA0";
        size_t msg_length = strlen(socket_msg);

        //sendto function, if send fails, ERR_SENDTO
        return_value = (int)sendto(sent_soc, socket_msg, msg_length, 0, results->ai_addr, results->ai_addrlen);
        if(return_value <= 0){
            return ERR_SENDTO;
        }

        //get the time when the message was sent
        gettimeofday(&time, NULL);
        time_sent = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);

        //set up socket for receiving
        fd_set FDS;
        FD_ZERO(&FDS);
        FD_SET(sent_soc, &FDS);

        //select function to wait for answer/ timeout / error
        return_value = select(FD_SETSIZE, &FDS, NULL, NULL, &timeout);
        if(return_value == -1) {
            cout<<"SELECT ERROR"<<endl;
            return ERR_SLCTERR;

        //timeout
        } else if (return_value == 0) {
            cout<<i<<"\t\t*"<<endl;
            continue;

        //Select successful, something received
        } else {

            char cbuf[512];

            //structure with received message
            struct msghdr msg;
            struct cmsghdr *cmsg;

            //structure for address, IPv4 and IPv6 compatible
            struct sockaddr_storage target;

            msg.msg_name = &target; //address of sender (us)
            msg.msg_namelen = sizeof(target);
            msg.msg_iov = NULL; //ICMP header - not needed
            msg.msg_iovlen = 0;
            msg.msg_flags = 0; //no flags
            msg.msg_control = cbuf; //buffer for ctrl messages
            msg.msg_controllen = sizeof(cbuf);

            //receiving message
            return_value = (int)recvmsg(sent_soc, &msg, MSG_ERRQUEUE);
            if(return_value == -1)
                return ERR_RECV;

            for (cmsg = CMSG_FIRSTHDR(&msg); cmsg ; cmsg = CMSG_NXTHDR(&msg, cmsg))
            {
                //controling the origin of message
                if ((cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR) || (cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_RECVERR))
                {
                    //getting data from header
                    struct sock_extended_err *e = (struct sock_extended_err*) CMSG_DATA(cmsg);

                    //magic
                    if (e && e->ee_origin == SO_EE_ORIGIN_ICMP) {

                        //getting the address
                        struct sockaddr_in *sin = (struct sockaddr_in *)(e+1);

                        //time of message receive
                        gettimeofday(&time, NULL);
                        time_recv = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);

                        //total time difference of sent and received
                        total = time_recv - time_sent;
                        if (e->ee_type == ICMP_DEST_UNREACH) {
                            switch (e->ee_code) {
                                case ICMP_HOST_UNREACH:
                                    printf("%2d\t\tH!\n", i);
                                    return HOST_UNREACH;
                                case ICMP_NET_UNREACH:
                                    printf("%2d\t\tN!\n", i);
                                    return NET_UNREACH;
                                case ICMP_PROT_UNREACH:
                                    printf("%2d\t\tP!\n", i);
                                    return PROT_UNREACH;
                                case ICMP_PKT_FILTERED:
                                    printf("%2d\t\tX!\n", i);
                                    return PKT_FILTERED;
                                case ICMP_PORT_UNREACH:
                                    char r_addr[INET_ADDRSTRLEN];
                                    inet_ntop(PF_INET, &sin->sin_addr, r_addr, INET_ADDRSTRLEN);
                                    printf("%2d\t\t%s\t\t%.3f\tms\n", i, r_addr, total);
                                    return 0;
                                default: break;
                            }

                        } else if (e->ee_type == ICMP_TIME_EXCEEDED) {
                            char r_addr[INET_ADDRSTRLEN];
                            inet_ntop(PF_INET, &sin->sin_addr, r_addr, INET_ADDRSTRLEN);
                            printf("%2d\t\t%s\t\t%.3f\tms\n", i, r_addr, total);


                        }

                    } else if (e && e->ee_origin == SO_EE_ORIGIN_ICMP6) {

                        /*same as above...
                         *
                         *
                         * BUT IN IPv6!!!!!
                         * *literally shaking in anticipation*
                         */
                        struct sockaddr_in6 *sin = (struct sockaddr_in6 *)(e+1);


                        gettimeofday(&time, NULL);
                        time_recv = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);
                        total = time_recv - time_sent;


                        if (e->ee_type == ICMP6_DST_UNREACH) {

                            switch (e->ee_code) {
                                case ICMP6_DST_UNREACH_ADDR:
                                    printf("%2d\t\tH!\n", i);
                                    return HOST_UNREACH;
                                case ICMP6_DST_UNREACH_NOROUTE:
                                    printf("%2d\t\tN!\n", i);
                                    return NET_UNREACH;
                                case 7:
                                    printf("%2d\t\tP!\n", i);
                                    return PROT_UNREACH;
                                case ICMP6_DST_UNREACH_ADMIN:
                                    printf("%2d\t\tX!\n", i);
                                    return PKT_FILTERED;
                                case ICMP6_DST_UNREACH_NOPORT:
                                    char r_addr[INET6_ADDRSTRLEN];
                                    inet_ntop(AF_INET6, &sin->sin6_addr, r_addr, INET6_ADDRSTRLEN);
                                    printf("%2d\t\t%s\t\t%.3f\tms\n", i, r_addr, total);
                                    return 0;
                                default:
                                    break;
                            }

                        } else if (e->ee_type == ICMP6_TIME_EXCEEDED) {
                            char r_addr[INET6_ADDRSTRLEN];
                            inet_ntop(AF_INET6, &sin->sin6_addr, r_addr, INET6_ADDRSTRLEN);
                            printf("%2d\t\t%s\t\t%.3f\tms\n", i, r_addr, total);


                        }
                    }
                }
            }
        }
    }
    //some cool debug code
    //do not delete
    //cout<<first_ttl<<endl<<max_ttl<<endl<<address<<endl;
}