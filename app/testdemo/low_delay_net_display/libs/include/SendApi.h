#ifndef SEND_API_H
#define SEND_API_H

#include <stdio.h>
#include <unistd.h>
#include "typedefs.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*NetStatusCallbackType)(const uint32_t bitrate,
                                      const uint8_t fraction_loss,
                                      const uint32_t rtt);

typedef void (*KeyFrameRequestCallbackType)();

int init_send(char *ip, int rtp_port, int rtcp_port, \
              NetStatusCallbackType NSfunc, \
              KeyFrameRequestCallbackType KfcFunc);

int start_send();

int send_data(const unsigned char *data, int len);

int stop_send();

int deinit_send();


#ifdef __cplusplus
}
#endif

#endif
