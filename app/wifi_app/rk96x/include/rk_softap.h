#ifndef __RK_SOFTAP_H__
#define __RK_SOFTAP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RK_SOFTAP_STATE_IDLE=0,
	RK_SOFTAP_STATE_CONNECTING,
	RK_SOFTAP_STATE_SUCCESS,
	RK_SOFTAP_STATE_FAIL,
	RK_SOFTAP_STATE_DISCONNECT,
} RK_SOFTAP_STATE;

typedef enum {
	RK_SOFTAP_TCP_SERVER=0,
	RK_SOFTAP_UDP_SERVER,
} RK_SOFTAP_SERVER_TYPE;

typedef int (*RK_SOFTAP_STATE_CALLBACK)(RK_SOFTAP_STATE state, const char *data);

int RK_softap_register_callback(RK_SOFTAP_STATE_CALLBACK cb);
/**
 * Start the softAP with specified parameters
 * @param name    SSID of the access point
 * @param psk     Password of the access point (NULL for open network)
 * @param channel WiFi channel (1-13), if <= 0 will auto select best channel
 * @return        0 on success, -1 on failure
 */
int RK_softap_start(char *name, char *psk, int channel);
int RK_softap_stop(void);
int RK_softap_getState(RK_SOFTAP_STATE *pState);

#ifdef __cplusplus
}
#endif

#endif
