/**
 * History:
 * ================================================================
 * 2019-11-18 qing.zou created
 *
 */

#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjmedia/sdp.h>
#include <pjsip_ua.h>

#include "sip_client.h"
#include "sip_internal.h"

#define THIS_FILE   "sip_client.c"

#define CHECK_STATUS()	do { if (status != PJ_SUCCESS) return status; } while (0)

static pj_bool_t on_rx_request(pjsip_rx_data *rdata);

static struct sip_data app;

static pjsip_module mod_initializer = 
{
    NULL, NULL,                     /* prev, next.		*/
    { "mod-sipclient", 11 },        /* Name.			*/
    -1,                             /* Id			    */
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
    NULL,                           /* load()			*/
    NULL,                           /* start()			*/
    NULL,                           /* stop()			*/
    NULL,                           /* unload()			*/
    &on_rx_request,                 /* on_rx_request()		*/
    NULL,                           /* on_rx_response()		*/
    NULL,                           /* on_tx_request.		*/
    NULL,                           /* on_tx_response()		*/
    NULL,                           /* on_tsx_state()		*/
};

/* Notification on incoming messages */
static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
    if (!app.enable_msg_logging)
        return PJ_FALSE;

    PJ_LOG(3, (THIS_FILE, "RX %d bytes %s from %s %s:%d:\n"
                          "%.*s\n"
                          "--end msg--",
               rdata->msg_info.len,
               pjsip_rx_data_get_info(rdata),
               rdata->tp_info.transport->type_name,
               rdata->pkt_info.src_name,
               rdata->pkt_info.src_port,
               (int)rdata->msg_info.len,
               rdata->msg_info.msg_buf));
    return PJ_FALSE;
}

/* Notification on outgoing messages */
static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{
    if (!app.enable_msg_logging)
        return PJ_SUCCESS;

    PJ_LOG(3, (THIS_FILE, "TX %d bytes %s to %s %s:%d:\n"
                          "%.*s\n"
                          "--end msg--",
               (tdata->buf.cur - tdata->buf.start),
               pjsip_tx_data_get_info(tdata),
               tdata->tp_info.transport->type_name,
               tdata->tp_info.dst_name,
               tdata->tp_info.dst_port,
               (int)(tdata->buf.cur - tdata->buf.start),
               tdata->buf.start));
    return PJ_SUCCESS;
}

/* The module instance. */
static pjsip_module mod_msg_logger = {
    NULL, NULL,                             /* prev, next.  */
    { "mod-msg-log", 13 },                  /* Name.		*/
    -1,                                     /* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER - 1, /* Priority	    */
    NULL,                                   /* load()		*/
    NULL,                                   /* start()		*/
    NULL,                                   /* stop()		*/
    NULL,                                   /* unload()		*/
    &logging_on_rx_msg,                     /* on_rx_request()	*/
    &logging_on_rx_msg,                     /* on_rx_response()	*/
    &logging_on_tx_msg,                     /* on_tx_request.	*/
    &logging_on_tx_msg,                     /* on_tx_response()	*/
    NULL,                                   /* on_tsx_state()	*/
};


/* Globals */
static int sip_af;
static int sip_port = 5060;
static pj_bool_t sip_tcp;

void sip_perror(const char *sender, const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1, (sender, "%s: %s [status=%d]", title, errmsg, status));
}

/* Worker thread function. */
static int worker_proc(void *arg)
{
    enum { THREAD_TIMEOUT = 10 };
    pj_status_t status;

    PJ_UNUSED_ARG(arg);

    while (!app.quit)
    {
        pj_time_val interval = {0, THREAD_TIMEOUT};
        status = pjsip_endpt_handle_events(app.endpt, &interval);
        if (status != PJ_SUCCESS)
            pj_thread_sleep(THREAD_TIMEOUT);
    }

    return 0;
}

static void call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e);
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);
static void call_on_rx_offer(pjsip_inv_session *inv, const pjmedia_sdp_session *offer);
static void call_on_media_update(pjsip_inv_session *inv, pj_status_t status);

static int make_call(char *uri);
static void hangup_all(void);

static int pjsip_module_init()
{
    pj_sockaddr addr;
    pj_status_t status;

    pj_log_set_level(5);

    pj_init();
    pj_log_set_level(3);

    status = pjlib_util_init();
    CHECK_STATUS();

    pj_caching_pool_init(&app.cp, NULL, 0);
    app.pool = pj_pool_create( &app.cp.factory, "sip-client", 512, 512, 0);

    //status = pjsip_endpt_create(&app.cp.factory, NULL, &app.endpt);
    status = pjsip_endpt_create(&app.cp.factory, pj_gethostname()->ptr, &app.endpt);
    if (status != PJ_SUCCESS) {
        sip_perror(NULL, "pjsip_endpt_create", status);
        pj_caching_pool_destroy(&app.cp);
        return status;
    }

    pj_log_set_level(4);
    pj_sockaddr_init((pj_uint16_t)sip_af, &addr, NULL, (pj_uint16_t)sip_port);
    if (sip_af == pj_AF_INET())
    {
        if (sip_tcp)
            status = pjsip_tcp_transport_start(app.endpt, &addr.ipv4, 1, NULL);
        else
            status = pjsip_udp_transport_start(app.endpt, &addr.ipv4, NULL, 1, NULL);
    }
    else if (sip_af == pj_AF_INET6())
    {
        status = pjsip_udp_transport_start6(app.endpt, &addr.ipv6, NULL, 1, NULL);
    }
    else
    {
        status = PJ_EAFNOTSUP;
    }

    pj_log_set_level(3);
    CHECK_STATUS();

    status = pjsip_tsx_layer_init_module(app.endpt);
    if (status != PJ_SUCCESS) {
        sip_perror(NULL, "pjsip_tsx_layer_init_module", status);
        goto end;
    }

    /* Init UA layer */
    if (pjsip_ua_instance()->id == -1) {
        status = pjsip_ua_init_module(app.endpt, NULL);
    }

    pjsip_inv_callback inv_cb;
    pj_bzero(&inv_cb, sizeof(inv_cb));
    inv_cb.on_state_changed = &call_on_state_changed;
    inv_cb.on_new_session = &call_on_forked;
    inv_cb.on_media_update = &call_on_media_update;
    inv_cb.on_rx_offer = &call_on_rx_offer;

    status = pjsip_inv_usage_init(app.endpt, &inv_cb);

    /* 100rel module */
    pjsip_100rel_init_module(app.endpt);

    /* Our module */
    pjsip_endpt_register_module(app.endpt, &mod_initializer);
    pjsip_endpt_register_module(app.endpt, &mod_msg_logger);

    pj_thread_create(app.pool, "sipclient", &worker_proc, NULL, 0, 0, &app.worker_thread);
    //CHECK_STATUS();

end:
    return status;
}

int sip_client_init(user_info_t *info)
{
    // IPv4
    sip_af = pj_AF_INET();

    pj_bzero(&app, sizeof(struct sip_data));
    pj_memcpy((void*)&app.info, (const void*)info, sizeof(user_info_t));
    strcpy(app.info.sip_port, "5060");
    app.enable_msg_logging = PJ_TRUE;

    pjsip_module_init();

    return 0;
}

//int sip_client_register(iclient_callback *ctx);
int sip_client_login(iclient_callback *cb, void *ctx)
{
    client_internal *client = &app.client;

	PJ_ASSERT_RETURN(ctx, PJ_EINVAL);

	client->ctx = ctx;
	memcpy(&client->cb, cb, sizeof(client->cb));

    sip_register(&app);

    return 0;
}

int sip_client_call(char *uri)
{
    make_call(uri);

    return 0;
}

void sip_client_hangup(void)
{
    hangup_all();
}

static void destroy_call(sip_call_t *call)
{
    call->inv = NULL;
}
static pjmedia_sdp_attr *find_remove_sdp_attrs(unsigned *cnt,
                                               pjmedia_sdp_attr *attr[],
                                               unsigned cnt_attr_to_remove,
                                               const char *attr_to_remove[])
{
    int i;
    pjmedia_sdp_attr *found_attr = NULL;

    for (i = 0; i < (int)*cnt; ++i)
    {
        unsigned j;
        for (j = 0; j < cnt_attr_to_remove; ++j)
        {
            if (pj_strcmp2(&attr[i]->name, attr_to_remove[j]) == 0)
            {
                if (!found_attr)
                    found_attr = attr[i];
                pj_array_erase(attr, sizeof(attr[0]), *cnt, i);
                --(*cnt);
                --i;
                break;
            }
        }
    }

    return found_attr;
}
static pjmedia_sdp_session *create_answer(int call_num, pj_pool_t *pool,
                                          const pjmedia_sdp_session *offer)
{
    const char *dir_attrs[] = {"sendrecv", "sendonly", "recvonly", "inactive"};
    const char *ice_attrs[] = {"ice-pwd", "ice-ufrag", "candidate"};
    pjmedia_sdp_session *answer = pjmedia_sdp_session_clone(pool, offer);
    pjmedia_sdp_attr *sess_dir_attr = NULL;
    unsigned mi;

    PJ_LOG(3, (THIS_FILE, "Call %d: creating answer:", call_num));

    //answer->name = pj_str("sipecho");
    answer->name = pj_str(app.info.account);
    sess_dir_attr = find_remove_sdp_attrs(&answer->attr_count, answer->attr,
                                          PJ_ARRAY_SIZE(dir_attrs),
                                          dir_attrs);

    for (mi = 0; mi < answer->media_count; ++mi)
    {
        pjmedia_sdp_media *m = answer->media[mi];
        pjmedia_sdp_attr *m_dir_attr;
        pjmedia_sdp_attr *dir_attr;
        const char *our_dir = NULL;
        pjmedia_sdp_conn *c;

        /* Match direction */
        m_dir_attr = find_remove_sdp_attrs(&m->attr_count, m->attr,
                                           PJ_ARRAY_SIZE(dir_attrs),
                                           dir_attrs);
        dir_attr = m_dir_attr ? m_dir_attr : sess_dir_attr;

        if (dir_attr)
        {
            if (pj_strcmp2(&dir_attr->name, "sendonly") == 0)
                our_dir = "recvonly";
            else if (pj_strcmp2(&dir_attr->name, "inactive") == 0)
                our_dir = "inactive";
            else if (pj_strcmp2(&dir_attr->name, "recvonly") == 0)
                our_dir = "inactive";

            if (our_dir)
            {
                dir_attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);
                dir_attr->name = pj_str((char *)our_dir);
                m->attr[m->attr_count++] = dir_attr;
            }
        }

        /* Remove ICE attributes */
        find_remove_sdp_attrs(&m->attr_count, m->attr, PJ_ARRAY_SIZE(ice_attrs), ice_attrs);

        /* Done */
        c = m->conn ? m->conn : answer->conn;
        PJ_LOG(3, (THIS_FILE, "  Media %d, %.*s: %s <--> %.*s:%d",
                   mi, (int)m->desc.media.slen, m->desc.media.ptr,
                   (our_dir ? our_dir : "sendrecv"),
                   (int)c->addr.slen, c->addr.ptr, m->desc.port));
    }

    return answer;
}
static void call_on_media_update(pjsip_inv_session *inv, pj_status_t status)
{
    TRACE_((THIS_FILE, "callback===========call_on_media_update %d", inv->state));
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(status);
}
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}
static void call_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
    TRACE_((THIS_FILE, "callback===========call_on_state_changed %d", inv->state));
    sip_call_t *call = (sip_call_t *)inv->mod_data[mod_initializer.id];
    if (!call)
        return;
    PJ_UNUSED_ARG(e);

    client_internal *client = &app.client;

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED)
    {
        PJ_LOG(3, (THIS_FILE, "Call %d: DISCONNECTED [reason=%d (%s)]",
                   call - app.call, inv->cause,
                   pjsip_get_status_text(inv->cause)->ptr));
        // code
        // 420 (Bad Extension)
        client->invited = 0;
        sip_client_param param = {0};
        param.call_id = call - app.call; //current_call;
        param.status = inv->cause;
        if (client->cb.on_call_disconnect)
            client->cb.on_call_disconnect(client->ctx, (void *)&param);
        else
            PJ_LOG(3, (THIS_FILE, "without register callback function: on_call_disconnect."));

        destroy_call(call);
    }
    else
    {
        PJ_LOG(3, (THIS_FILE, "Call %d: state changed to %s",
                        call - app.call, pjsip_inv_state_name(inv->state)));
    }

    if (inv->state == PJSIP_INV_STATE_CONFIRMED)
    {
        client->invited = 1;
        sip_client_param param = {0};
        param.call_id = call - app.call; //current_call;
        param.status = 0;
        if (client->cb.on_call_confirmed)
            client->cb.on_call_confirmed(client->ctx, (void *)&param);
        else
            PJ_LOG(3, (THIS_FILE, "without register callback function: on_call_confirmed."));
    }
    else
    {
        client->invited = 0;
    }
}
static void call_on_rx_offer(pjsip_inv_session *inv, const pjmedia_sdp_session *offer)
{
    TRACE_((THIS_FILE, "callback===========call_on_rx_offer %d", inv->state));
    sip_call_t *call = (sip_call_t *)inv->mod_data[mod_initializer.id];
    pjsip_inv_set_sdp_answer(inv, create_answer((int)(call - app.call), inv->pool_prov, offer));
}

static pj_bool_t on_rx_request(pjsip_rx_data *rdata)
{
    pj_sockaddr hostaddr;
    char temp[80], hostip[PJ_INET6_ADDRSTRLEN];
    pj_str_t local_uri;
    pjsip_dialog *dlg = NULL;
    pjsip_rdata_sdp_info *sdp_info;
    pjmedia_sdp_session *answer = NULL;
    pjsip_tx_data *tdata = NULL;
    sip_call_t *call = NULL;
    unsigned i;
    pj_status_t status;
    TRACE_((THIS_FILE, "callback===========on_rx_request"));

    PJ_LOG(3, (THIS_FILE, "RX %.*s from %s",
               (int)rdata->msg_info.msg->line.req.method.name.slen,
               rdata->msg_info.msg->line.req.method.name.ptr,
               rdata->pkt_info.src_name));

    if (rdata->msg_info.msg->line.req.method.id == PJSIP_REGISTER_METHOD)
    {
        /* Let me be a registrar! */
        pjsip_hdr hdr_list, *h;
        pjsip_msg *msg;
        int expires = -1;

        pj_list_init(&hdr_list);
        msg = rdata->msg_info.msg;
        h = (pjsip_hdr *)pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);
        if (h)
        {
            expires = ((pjsip_expires_hdr *)h)->ivalue;
            pj_list_push_back(&hdr_list, pjsip_hdr_clone(rdata->tp_info.pool, h));
            PJ_LOG(3, (THIS_FILE, " Expires=%d", expires));
        }
        if (expires != 0)
        {
            h = (pjsip_hdr *)pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
            if (h)
                pj_list_push_back(&hdr_list, pjsip_hdr_clone(rdata->tp_info.pool, h));
        }

        pjsip_endpt_respond(app.endpt, &mod_initializer, rdata, 200, NULL, &hdr_list, NULL, NULL);
        return PJ_TRUE;
    }

    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD)
    {
        if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD)
        {
            pj_str_t reason = pj_str("Go away");
            pjsip_endpt_respond_stateless(app.endpt, rdata, 400, &reason, NULL, NULL);
        }
        return PJ_TRUE;
    }

    sdp_info = pjsip_rdata_get_sdp_info(rdata);
    if (!sdp_info || !sdp_info->sdp)
    {
        pj_str_t reason = pj_str("Require valid offer");
        pjsip_endpt_respond_stateless(app.endpt, rdata, 400, &reason, NULL, NULL);
    }

    for (i = 0; i < MAX_CALLS; ++i)
    {
        if (app.call[i].inv == NULL)
        {
            call = &app.call[i];
            break;
        }
    }

    if (i == MAX_CALLS)
    {
        pj_str_t reason = pj_str("We're full");
        pjsip_endpt_respond_stateless(app.endpt, rdata, PJSIP_SC_BUSY_HERE, &reason, NULL, NULL);
        return PJ_TRUE;
    }

    /* Generate Contact URI */
    status = pj_gethostip(sip_af, &hostaddr);
    if (status != PJ_SUCCESS)
    {
        sip_perror(THIS_FILE, "Unable to retrieve local host IP", status);
        return PJ_TRUE;
    }
    pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);
    //pj_ansi_sprintf(temp, "<sip:sipecho@%s:%d>", hostip, sip_port);
    //pj_ansi_sprintf(temp, "<sip:%s@%s:%d>", app.info.account, hostip, sip_port);
    pj_ansi_sprintf(temp, "<sip:%s@%s:%d>", app.info.account, app.info.server, sip_port);
    local_uri = pj_str(temp);
    TRACE_((THIS_FILE, "callback===========on_rx_request local_uri:%s", temp));

    status = pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(), rdata, &local_uri, &dlg);

    if (status == PJ_SUCCESS)
        answer = create_answer((int)(call - app.call), dlg->pool, sdp_info->sdp);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_create_uas(dlg, rdata, answer, 0, &call->inv);

    if (dlg)
        pjsip_dlg_dec_lock(dlg);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_initial_answer(call->inv, rdata, 100, NULL, NULL, &tdata);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_send_msg(call->inv, tdata);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_answer(call->inv, 180, NULL, NULL, &tdata);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_send_msg(call->inv, tdata);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_answer(call->inv, 200, NULL, NULL, &tdata);

    if (status == PJ_SUCCESS)
        status = pjsip_inv_send_msg(call->inv, tdata);

    if (status != PJ_SUCCESS)
    {
        pjsip_endpt_respond_stateless(app.endpt, rdata, 500, NULL, NULL, NULL);
        destroy_call(call);
    }
    else
    {
        call->inv->mod_data[mod_initializer.id] = call;
    }

    return PJ_TRUE;
}


char *offer_sdp = /* Offer: */
	"v=0\r\n"
	"o=alice 1 1 IN IP4 172.17.13.222\r\n"
	"s= \r\n"
	"c=IN IP4 172.17.13.222\r\n"
	"t=0 0\r\n"
	"m=audio 5060 RTP/AVP 0\r\n"
	"a=rtpmap:0 PCMU/8000\r\n";

/**************** UTILS ******************/
static pjmedia_sdp_session *create_sdp(pj_pool_t *pool, const char *body)
{
    pjmedia_sdp_session *sdp;
    pj_str_t dup;
    pj_status_t status;

    pj_strdup2_with_null(pool, &dup, body);
    status = pjmedia_sdp_parse(pool, dup.ptr, dup.slen, &sdp);
    pj_assert(status == PJ_SUCCESS);
    PJ_UNUSED_ARG(status);

    return sdp;
}

static int make_call(char *uri)
{
    int i;
    pj_sockaddr hostaddr;
	char hostip[PJ_INET6_ADDRSTRLEN+2];
	char temp[80];
	sip_call_t *call;
	pj_str_t dst_uri = pj_str(uri);
	pj_str_t local_uri;
	pjsip_dialog *dlg;
	pj_status_t status;
	pjsip_tx_data *tdata;

	if (pj_gethostip(sip_af, &hostaddr) != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "Unable to retrieve local host IP"));
	    goto on_error;
	}
	pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);

    user_info_t *info = &app.info;
	//pj_ansi_sprintf(temp, "<sip:%s@%s:%d>", info->account, hostip, sip_port);
	//pj_ansi_sprintf(temp, "<sip:%s@%s:%d>", info->account, info->server, sip_port);
	pj_ansi_sprintf(temp, "<sip:%s@%s>", info->account, info->server);
	//pj_ansi_sprintf(temp, "<sip:timaB@172.20.25.40:5060>");
	local_uri = pj_str(temp);

	call = &app.call[0];
    // for (i = 0; i < MAX_CALLS; ++i)
    // {
    //     if (app.call[i].inv == NULL) {
    //         call = &app.call[i];
    //         break;
    //     }
    // }

    // if (i == MAX_CALLS)
    // {
    //     sip_perror(THIS_FILE, "We're full", PJSIP_SC_BUSY_HERE);
    //     return PJ_TRUE;
    // }

    status = pjsip_dlg_create_uac(pjsip_ua_instance(),
                                  &local_uri, /* local URI */
                                  &local_uri, /* local Contact */
                                  &dst_uri,   /* remote URI */
                                  &dst_uri,   /* remote target */
                                  &dlg);      /* dialog */
    if (status != PJ_SUCCESS)
    {
        sip_perror(THIS_FILE, "Unable to create UAC dialog", status);
        return 1;
    }

    char tmp_sdp[512] = {0};
    snprintf(tmp_sdp, 512, "v=0\r\n"
                           "o=%s 1 1 IN IP4 %s\r\n"
                           "s=sip \r\n"
                           "c=IN IP4 %s\r\n"
                           "t=0 0\r\n"
                           "m=audio %s RTP/AVP 0\r\n"
                           "a=rtpmap:0 PCMU/8000\r\n", 
                           pj_gethostname()->ptr, info->server, info->server, info->sip_port);
                           //info->server, info->server, info->sip_port);
    pjmedia_sdp_session *sdp = create_sdp(dlg->pool, tmp_sdp);

    //status = pjsip_inv_create_uac( dlg, NULL, 0, &call->inv);
    status = pjsip_inv_create_uac(dlg, sdp, 0, &call->inv);
    if (status != PJ_SUCCESS)
        goto on_error;

    call->inv->mod_data[mod_initializer.id] = call;

    if (1)
    {
        pjsip_cred_info cred;

        pj_bzero(&cred, sizeof(cred));
        // cred.realm = pj_str("91carnet.com");
        // cred.scheme = pj_str("digest");
        // cred.username = pj_str("timaB");
        // cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        // cred.data = pj_str("timaB");
        cred.realm = pj_str(info->realm);
        cred.scheme = pj_str("digest");
        cred.username = pj_str(info->account);
        cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        cred.data = pj_str(info->account);

        status = pjsip_auth_clt_set_credentials(&dlg->auth_sess, 1, &cred);
        //status = pjsip_regc_set_credentials(regc, 1, &cred);
        if (status != PJ_SUCCESS)
        {
            //pjsip_regc_destroy(regc);
            return -115;
        }
    }

    status = pjsip_inv_invite(call->inv, &tdata);
    if (status != PJ_SUCCESS)
        goto on_error;

    status = pjsip_inv_send_msg(call->inv, tdata);
    if (status != PJ_SUCCESS)
        goto on_error;

    return 0;
on_error:
    sip_perror(THIS_FILE, "An error has occurred.", status);
    return 1;
}

static void hangup_all(void)
{
    unsigned i;
    for (i = 0; i < MAX_CALLS; ++i)
    {
        sip_call_t *call = &app.call[i];

        if (call->inv && call->inv->state <= PJSIP_INV_STATE_CONFIRMED)
        {
            pj_status_t status;
            pjsip_tx_data *tdata;

            status = pjsip_inv_end_session(call->inv, PJSIP_SC_BUSY_HERE, NULL, &tdata);
            if (status == PJ_SUCCESS && tdata)
                pjsip_inv_send_msg(call->inv, tdata);
        }
    }
}
