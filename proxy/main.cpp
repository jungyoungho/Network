#include <QCoreApplication>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>        /* for NF_ACCEPT */
#include <errno.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include "parse.h"
#include "cal_checksum.h"

#define ipchecksum 0
#define udpchecksum 1
#define tcpchecksum 2
#define icmpchecksum 3
#define OUT_OF_RANGE 65536

/* returns packet id */
static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data);
static u_int32_t modify_packet (struct nfq_data *tb, parse *ps);

int main(int argc, char *argv[])
{
    parse ps(argc,argv);
    ps.parse_data_in_linux();

    struct nfq_handle *h;
    struct nfq_q_handle *qh;
    struct nfnl_handle *nh;
    int fd;
    int rv;
    char buf[4096] __attribute__ ((aligned));

    printf("opening library handle\n");
    h = nfq_open();
    if (!h) {
        fprintf(stderr, "error during nfq_open()\n");
        exit(1);
    }

    printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
    if (nfq_unbind_pf(h, AF_INET) < 0) {
        fprintf(stderr, "error during nfq_unbind_pf()\n");
        exit(1);
    }

    printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
    if (nfq_bind_pf(h, AF_INET) < 0) {
        fprintf(stderr, "error during nfq_bind_pf()\n");
        exit(1);
    }

    printf("binding this socket to queue '0'\n");
    qh = nfq_create_queue(h,  0, &cb, &ps);
    if (!qh) {
        fprintf(stderr, "error during nfq_create_queue()\n");
        exit(1);
    }

    printf("setting copy_packet mode\n");
    if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
        fprintf(stderr, "can't set packet_copy mode\n");
        exit(1);
    }

    fd = nfq_fd(h);

    for (;;) {
        if ((rv = recv(fd, buf, sizeof(buf), 0)) >= 0) {
            printf("pkt received\n");
            nfq_handle_packet(h, buf, rv);
            continue;
        }
        /* if your application is too slow to digest the packets that
         * are sent from kernel-space, the socket buffer that we use
         * to enqueue packets may fill up returning ENOBUFS. Depending
         * on your application, this error may be ignored. nfq_nlmsg_verdict_putPlease, see
         * the doxygen documentation of this library on how to improve
         * this situation.
         */
        if (rv < 0 && errno == ENOBUFS) {
            printf("losing packets!\n");
            continue;
        }
        perror("recv failed");
        break;
    }

    printf("unbinding from queue 0\n");
    nfq_destroy_queue(qh);

#ifdef INSANE
    /* normally, applications SHOULD NOT issue this command, since
     * it detaches other programs/sockets from AF_INET, too ! */
    printf("unbinding from AF_INET\n");
    nfq_unbind_pf(h, AF_INET);
#endif

    printf("closing library handle\n");
    nfq_close(h);
    exit(0);
}
static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data)
{
    (void*)nfmsg;
    parse *ps=(parse *)data;
    u_int32_t id = modify_packet(nfa,ps);
    printf("entering callback\n");
    return nfq_set_verdict(qh, id, NF_ACCEPT, ps->using_send_packet_length(), ps->using_send_packet());
}

static u_int32_t modify_packet(struct nfq_data *tb, parse *ps)
{
    int id = 0;
    struct nfqnl_msg_packet_hdr *ph;
    struct nfqnl_msg_packet_hw *hwph;
    int ret;
    uint8_t *data;

    ph = nfq_get_msg_packet_hdr(tb);
    if (ph) {
        id = ntohl(ph->packet_id);
        //printf("hw_protocol=0x%04x hook=%u id=%u ", ntohs(ph->hw_protocol), ph->hook, id);
    }

    hwph = nfq_get_packet_hw(tb);
    if (hwph) {
        int i, hlen = ntohs(hwph->hw_addrlen);
        //printf("hw_src_addr=");
        //for (i = 0; i < hlen-1; i++)
            //printf("%02x:", hwph->hw_addr[i]);
        //printf("%02x ", hwph->hw_addr[hlen-1]);
    }

    ret = nfq_get_payload(tb, &data);
    if (ret <= 0)
    {
        cout << " >> No payload packet !! " << endl;
        return id;
    }
    printf("payload_len=%d ", ret);
    cal_checksum cc;
    struct iphdr *ipd = (struct iphdr *)data;
    if(ntohs(ipd->protocol==0x06))
    {
        ipd->saddr=ps->using_my_ip();
        cc.get_iphdr(ipd);
        ipd->check=ntohs(cc.checksum(ipchecksum));
        ps->get_send_packet_length(ret);
        ps->make_send_packet(ipd,data+ipd->ihl*4);
    }
    return id;
}

