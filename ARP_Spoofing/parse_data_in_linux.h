#ifndef PARSE_DATA_IN_LINUX_H
#define PARSE_DATA_IN_LINUX_H
#include <iostream>
#include <arpa/inet.h>
#include "parse_data.h"
#include "char_to_binary.h"

//ifconfig eth0 | grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}'

using namespace std;

void parse_data_in_linux(parse_data *parse)
{
    //-----------------------------get my(attacker) mac!!-----------------------------
    char host_mac[18];//mymac
    FILE *m;
    string str_ifconfig = "ifconfig ";
    string interface = parse->using_interface();
    string regex = " | grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}'";
    str_ifconfig=str_ifconfig+interface+regex;

    const char *command=str_ifconfig.c_str();
    m=popen(command,"r");
    fgets((char*)host_mac,18, m);
    uint8_t mac[6];
    char_to_binary(host_mac,mac);
    parse->get_attacker_mac(mac);

    //-----------------------------get my(attacker) ip!!-----------------------------
    FILE *i;
    i=popen("ip addr | grep 'inet' | grep brd | awk '{printf $2}' | awk -F/ ' {printf $1}'","r");
    char host_ip[15];
    fgets(host_ip,15,i);
    parse->get_attacker_ip(host_ip);
}
#endif // PARSE_DATA_IN_LINUX_H
