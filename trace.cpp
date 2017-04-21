#include <unistd.h>
#include <stdio.h>
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

using namespace std;

int first_ttl   = 1;
int max_ttl     = 30;
string address;

void parser(int argc, char *argv[]) {

    int option = 0;
    while (optind < argc) {

        if ((option = getopt(argc, argv, "f:m:")) != -1) {

            switch (option) {

                case 'f': {

                    first_ttl = (int)atol(optarg);
                    break;
                }

                case 'm': {

                    max_ttl = (int)atol(optarg);
                    break;
                }

                default: {
                    break;
                }
            }
        }

        else {

            address = argv[optind];
            optind++;
        }
    }
}


int main(int argc, char *argv[]) {

    parser(argc, argv);
}