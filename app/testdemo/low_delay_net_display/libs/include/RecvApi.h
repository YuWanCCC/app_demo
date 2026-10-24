#ifndef RECV_API_H
#define RECV_API_H

typedef void (*RECVDATACB)(unsigned char *data,
                           int len, \
                           int channel);

int init_recv(int channel, int port, RECVDATACB cb);

int start_recv(int channel);

int stop_recv(int channel);

int deinit_recv(int channel);

#endif