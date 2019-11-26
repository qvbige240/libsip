/**
 * History:
 * ================================================================
 * 2019-11-18 qing.zou created
 *
 */

#ifndef SIP_CLIENT_H
#define SIP_CLIENT_H

#include "sip_typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct user_info_s
{
	char	        account[32];	// timaA
	char	        passwd[32];		// timaA
	char	        realm[32];	    // "91carnet.com"
	char	        server[32];	    // "172.20.25.40"
	char	        sip_port[16];	// "5060"

	char	        relay[32];	    // "172.17.13.8:3488"
	char	        relay_port[16];	// "3488"
	char	        username[32];	// username2
	char	        password[32];	// password2

	int             conn_type;
} user_info_t;


typedef struct sip_client_param
{
	int			call_id;
	int			status;
} sip_client_param;

typedef struct iclient_callback
{
	void (*on_register_status)(void *ctx, void *param);

	void (*on_call_confirmed)(void *ctx, void *param);

	void (*on_call_disconnect)(void *ctx, void *param);

	void (*on_connect_success)(void *ctx, void *param);

	void (*on_connect_failure)(void *ctx, void *param);

	void (*on_sock_disconnect)(void *ctx, void *param);

	void (*on_socket_clearing)(void *ctx, void *param);

	void (*on_socket_writable)(void *ctx, void *param);

	void (*on_receive_message)(void *ctx, void *pkt, unsigned long bytes_read);
} iclient_callback;

int sip_client_init(user_info_t *info);

//int sip_client_register(iclient_callback *ctx);
int sip_client_login(iclient_callback *cb, void *ctx);

int sip_client_call(char *uri);

void sip_client_hangup(void);


#ifdef __cplusplus
}
#endif

#endif // SIP_CLIENT_H
