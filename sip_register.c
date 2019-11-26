/**
 * History:
 * ================================================================
 * 2019-11-20 qing.zou created
 *
 */

#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjmedia/sdp.h>
#include <pjsip_ua.h>

#include "sip_client.h"
#include "sip_internal.h"

#define THIS_FILE   "sip_register.c"

static pjsip_endpoint *endpt;

/* A module to inject error into outgoing sending operation */
static pj_status_t mod_send_on_tx_request(pjsip_tx_data *tdata);

static struct
{
    pjsip_module        mod;
    unsigned            count;
    unsigned            count_before_reject;
} send_mod = 
{
    {
        NULL, NULL,                         /* prev, next.		*/
        { "mod-send", 8 },                  /* Name.			*/
        -1,                                 /* Id			    */
        PJSIP_MOD_PRIORITY_TRANSPORT_LAYER, /* Priority			*/
        NULL,                               /* load()			*/
        NULL,                               /* start()			*/
        NULL,                               /* stop()			*/
        NULL,                               /* unload()			*/
        NULL,                               /* on_rx_request()		*/
        NULL,                               /* on_rx_response()		*/
        &mod_send_on_tx_request,            /* on_tx_request.		*/
        NULL,                               /* on_tx_response()		*/
        NULL,                               /* on_tsx_state()		*/
    },
    0,
    0xFFFF
};

static pj_status_t mod_send_on_tx_request(pjsip_tx_data *tdata)
{
    PJ_UNUSED_ARG(tdata);
    TRACE_((THIS_FILE, "callback===========mod_send_on_tx_request %d %d",
            send_mod.count, send_mod.count_before_reject));

    if (++send_mod.count > send_mod.count_before_reject)
        return PJ_ECANCELLED;
    else
        return PJ_SUCCESS;
}

/************************************************************************/
/* Registrar for testing */
static pj_bool_t regs_rx_request(pjsip_rx_data *rdata);

enum contact_op
{
    NONE,           /* don't put Contact header	    */
    EXACT,          /* return exact contact		    */
    MODIFIED,       /* return modified Contact header   */
};

struct registrar_cfg
{
    pj_bool_t       respond;            /* should it respond at all	    */
    unsigned        status_code;        /* final response status code   */
    pj_bool_t       authenticate;       /* should we authenticate?	    */
    enum contact_op contact_op;         /* What should we do with Contact   */
    unsigned        expires_param;      /* non-zero to put in expires param	*/
    unsigned        expires;            /* non-zero to put in Expires header*/

    pj_str_t        more_contacts;      /* Additional Contact headers to put*/
};

static struct registrar
{
    pjsip_module            mod;
    struct registrar_cfg    cfg;
    unsigned                response_cnt;
} registrar =
{
    {
        NULL, NULL,                     /* prev, next.		*/
        { "registrar", 9 },             /* Name.			*/
        -1,                             /* Id			    */
        PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
        NULL,                           /* load()			*/
        NULL,                           /* start()			*/
        NULL,                           /* stop()			*/
        NULL,                           /* unload()			*/
        &regs_rx_request,               /* on_rx_request()		*/
        NULL,                           /* on_rx_response()		*/
        NULL,                           /* on_tx_request.		*/
        NULL,                           /* on_tx_response()		*/
        NULL,                           /* on_tsx_state()		*/
    }
};

static pj_bool_t regs_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr hdr_list;
    int code;
    pj_status_t status;
    TRACE_((THIS_FILE, "callback===========regs_rx_request"));

    if (msg->line.req.method.id != PJSIP_REGISTER_METHOD)
        return PJ_FALSE;

    if (!registrar.cfg.respond)
        return PJ_TRUE;

    pj_list_init(&hdr_list);

    if (registrar.cfg.authenticate && pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL) == NULL)
    {
        pjsip_generic_string_hdr *hwww;
        const pj_str_t hname = pj_str("WWW-Authenticate");
        const pj_str_t hvalue = pj_str("Digest realm=\"test\"");

        hwww = pjsip_generic_string_hdr_create(rdata->tp_info.pool, &hname, &hvalue);
        pj_list_push_back(&hdr_list, hwww);

        code = 401;
    }
    else
    {
        if (registrar.cfg.contact_op == EXACT || registrar.cfg.contact_op == MODIFIED)
        {
            pjsip_hdr *hsrc;

            for (hsrc = msg->hdr.next; hsrc != &msg->hdr; hsrc = hsrc->next)
            {
                pjsip_contact_hdr *hdst;

                if (hsrc->type != PJSIP_H_CONTACT)
                    continue;

                hdst = (pjsip_contact_hdr *)
                    pjsip_hdr_clone(rdata->tp_info.pool, hsrc);

                if (hdst->expires == 0)
                    continue;

                if (registrar.cfg.contact_op == MODIFIED)
                {
                    if (PJSIP_URI_SCHEME_IS_SIP(hdst->uri) || PJSIP_URI_SCHEME_IS_SIPS(hdst->uri))
                    {
                        pjsip_sip_uri *sip_uri = (pjsip_sip_uri *)pjsip_uri_get_uri(hdst->uri);
                        sip_uri->host = pj_str("x-modified-host");
                        sip_uri->port = 1;
                    }
                }

                if (registrar.cfg.expires_param)
                    hdst->expires = registrar.cfg.expires_param;

                pj_list_push_back(&hdr_list, hdst);
            }
        }

        if (registrar.cfg.more_contacts.slen)
        {
            pjsip_generic_string_hdr *hcontact;
            const pj_str_t hname = pj_str("Contact");

            hcontact = pjsip_generic_string_hdr_create(rdata->tp_info.pool, &hname,
                                                       &registrar.cfg.more_contacts);
            pj_list_push_back(&hdr_list, hcontact);
        }

        if (registrar.cfg.expires)
        {
            pjsip_expires_hdr *hexp;

            hexp = pjsip_expires_hdr_create(rdata->tp_info.pool, registrar.cfg.expires);
            pj_list_push_back(&hdr_list, hexp);
        }

        registrar.response_cnt++;

        code = registrar.cfg.status_code;
    }

    status = pjsip_endpt_respond(endpt, NULL, rdata, code, NULL, &hdr_list, NULL, NULL);
    pj_assert(status == PJ_SUCCESS);

    return (status == PJ_SUCCESS);
}

/* Client registration session */
struct client
{
    /* Result/expected result */
    int             error;
    int             code;
    pj_bool_t       have_reg;
    int             expiration;
    unsigned        contact_cnt;
    pj_bool_t       auth;

    /* Commands */
    pj_bool_t       destroy_on_cb;

    /* Status */
    pj_bool_t       done;

    /* Additional results */
    int             interval;
    int             next_reg;

    struct sip_data *app;
};

/* regc callback */
static void client_cb(struct pjsip_regc_cbparam *param)
{
    struct client *client = (struct client *)param->token;
    pjsip_regc_info info;
    pj_status_t status;

    client->done = PJ_TRUE;

    status = pjsip_regc_get_info(param->regc, &info);
    pj_assert(status == PJ_SUCCESS);
    PJ_UNUSED_ARG(status);
    client->error = (param->status != PJ_SUCCESS);
    client->code = param->code;

    TRACE_((THIS_FILE, "callback===========regc client_cb code(%d) uri: %s %d expiration: %d, next reg: %d",
            param->code, info.client_uri, info.interval, param->expiration, info.next_reg));

    client_internal *c = &(client->app->client);
    if (c->cb.on_register_status)
    {
        sip_client_param data = {0};
        data.call_id = 0;
        data.status = param->code;
        if (param->code != c->reg_status)
            c->cb.on_register_status(c->ctx, (void *)&data);
    }
    else
    {
        PJ_LOG(3, (THIS_FILE, "without register callback: on_register_status."));
    }
    c->reg_status = param->code;


    if (client->error)
        return;

    client->have_reg = info.auto_reg && info.interval > 0 && param->expiration > 0;
    client->expiration = param->expiration;
    client->contact_cnt = param->contact_cnt;
    client->interval = info.interval;
    client->next_reg = info.next_reg;

    if (client->destroy_on_cb)
        pjsip_regc_destroy(param->regc);
}

static int do_user_register(const struct client *client_cfg, struct sip_data *app)
{
    char id_tmp[128] = {0}, uri_tmp[32] = {0};
    pj_str_t registrar_uri;
    pjsip_regc *regc;
    pjsip_tx_data *tdata;
    pj_status_t status;

    user_info_t *info = &app->info;

    pj_ansi_sprintf(id_tmp, "<sip:%s@%s>", info->account, info->server);
    pj_ansi_sprintf(uri_tmp, "sip:%s", info->server);
    //const pj_str_t aor = pj_str("<sip:timaB@172.20.25.40>");
    //sprintf(uri_tmp, "%s", "sip:172.20.25.40");
    //pj_str_t contacts = pj_str("<sip:timaB@172.20.25.40>");
    //pj_str_t contacts = pj_str("<sip:timaB@172.17.12.52:5060>");
    const pj_str_t aor = pj_str(id_tmp);
    //const pj_str_t contacts = pj_str(id_tmp);
    registrar_uri = pj_str(uri_tmp);
    
    /* Generate Contact URI */
    char temp[80], hostip[PJ_INET6_ADDRSTRLEN];
    pj_sockaddr hostaddr;
    int sip_af = pj_AF_INET();
    status = pj_gethostip(sip_af, &hostaddr);
    if (status != PJ_SUCCESS) {
        sip_perror(THIS_FILE, "Unable to retrieve local host IP", status);
        return PJ_TRUE;
    }
    pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);
    pj_ansi_sprintf(temp, "<sip:%s@%s:%s>", info->account, hostip, info->sip_port);
    const pj_str_t contacts = pj_str(temp);

    struct client *client_result = pj_pool_zalloc(app->pool, sizeof(struct client));
    //client_result->destroy_on_cb = client_cfg->destroy_on_cb;
    client_result->destroy_on_cb = PJ_FALSE;
    client_result->app = app;

    //pjsip_endpoint *endpt = app->endpt;
    status = pjsip_regc_create(endpt, client_result, &client_cb, &regc);
    if (status != PJ_SUCCESS)
        return -100;

    // status = pjsip_regc_init(regc, registrar_uri, &aor, &aor, contact_cnt,
    // 		     contacts, expires ? expires : 60);
    status = pjsip_regc_init(regc, &registrar_uri, &aor, &aor, 1, &contacts, 65);
    if (status != PJ_SUCCESS) {
        pjsip_regc_destroy(regc);
        return -110;
    }

    if (client_cfg->auth)
    {
        pjsip_cred_info cred;

        pj_bzero(&cred, sizeof(cred));
        // cred.realm = pj_str("91carnet.com");
        // cred.scheme = pj_str("digest");
        // cred.username = pj_str("timaB");
        // cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        // cred.data = pj_str("timaB");
        cred.realm = pj_str("91carnet.com");
        cred.scheme = pj_str("digest");
        cred.username = pj_str(info->account);
        cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
        cred.data = pj_str(info->account);

        status = pjsip_regc_set_credentials(regc, 1, &cred);
        if (status != PJ_SUCCESS) {
            pjsip_regc_destroy(regc);
            return -115;
        }
    }

    /* Register */
    status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status != PJ_SUCCESS)
    {
        pjsip_regc_destroy(regc);
        return -120;
    }
    status = pjsip_regc_send(regc, tdata);

    return status;
}

int sip_register(struct sip_data *app)
{
    enum { TIMEOUT = 60 };
    struct client client_cfg = 
	/* error	code	have_reg    expiration	contact_cnt auth?    destroy*/
	{ PJ_FALSE,	200,	PJ_TRUE,    TIMEOUT,	1,	    PJ_TRUE,   PJ_FALSE};

    int status = 0;
    //pjsip_endpoint *endpt = app->endpt;
    endpt = app->endpt;

    /* Register registrar module */
    status = pjsip_endpt_register_module(endpt, &registrar.mod);
    if (status != PJ_SUCCESS)
    {
        printf("========= erorr\n");
        sip_perror(THIS_FILE, "Unable to register module registrar.mod", status);
    }
    /* Register send module */
    status = pjsip_endpt_register_module(endpt, &send_mod.mod);
    if (status != PJ_SUCCESS)
    {
        printf("========= erorr\n");
        sip_perror(THIS_FILE, "Unable to register module send_mod.mod", status);
    }

    return do_user_register(&client_cfg, app);
}
