#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mongoose.h"
#include "log.h"
#include "server.h"
#include "calldyn.h"
#include "global.h"

static const char *get_peer_ip(struct mg_connection *c)
{
    static char ip[64];
    if (c == NULL)
        return "unknown";
    mg_snprintf(ip,sizeof(ip), "%M", mg_print_ip, &c->rem);
    return ip;
}

static int call_soket_demo(const char *func_name, const char *json_in, char **json_out){
    char ffi_path[512];
    snprintf( ffi_path, sizeof(ffi_path), "%s%ssoket_demo%s", LIBARY_DIR, PATH_SEP, FFI_EXT);
    log_debug("[WS] call %s", func_name);
    return call_local_dyn_libffi( ffi_path, func_name, json_in,  json_out);
}


void ws_ev_handler(struct mg_connection *c, int ev, void *ev_data){

    switch(ev){
    case MG_EV_WS_OPEN:{

        char response[128];
        snprintf(
            response,
            sizeof(response),
            "{\"code\":0,\"msg\":\"connected\",\"peer\":\"%s\"}",
            get_peer_ip(c)
        );
        mg_ws_send(c, response, strlen(response), WEBSOCKET_OP_TEXT);
        log_info("[WS] opened %s", get_peer_ip(c));
        break;
    }

    case MG_EV_WS_MSG:{

        struct mg_ws_message *msg =
            (struct mg_ws_message *)ev_data;

        char *json_in =
            calloc(1, msg->data.len + 1);

        if(json_in){
            memcpy(json_in, msg->data.buf, msg->data.len);
            log_info("[WS] recv:%s", json_in);
        }

        const char *func_name = "process_status";
        if(json_in){
            char *p = strstr( json_in, "\"action\"");
            if(p){
                char *q = strchr(p,'"');
                if(q){
                    q = strchr(q + 1, '"');


                    if(q){
                        char *start = q + 2;
                        char *end = strchr( start, '"' );
                        if(end){

                            static char action[64];
                            int len = end - start;
                            if(len > 0 && len < 63) {

                                memcpy( action, start, len);
                                action[len]=0;
                                func_name =  action;
                            }
                        }
                    }
                }
            }
        }
        char *json_out=NULL;
        int rc = call_soket_demo( func_name, json_in ? json_in:"{}", &json_out);
        if(rc == 0 && json_out)
        {
            mg_ws_send( c, json_out, strlen(json_out), WEBSOCKET_OP_TEXT);
            free(json_out);
        } else {

            const char *err = "{\"code\":-1,\"msg\":\"call failed\"}";
            mg_ws_send(c, err, strlen(err), WEBSOCKET_OP_TEXT);
        }
        free(json_in);
        break;
    }

    case MG_EV_CLOSE:{
        log_info("[WS] closed %s",get_peer_ip(c));
        break;
    }

    case MG_EV_ERROR:{
        log_error("[WS] error:%s",(char *)ev_data);
        break;
    }

    default:
        break;

    }

}


void ws_accept_manual( struct mg_connection *c, struct mg_http_message *hm){

    if (hm->uri.len == 3 && memcmp( hm->uri.buf,"/ws", 3) == 0){

        c->fn = ws_ev_handler;
        mg_ws_upgrade(c, hm, NULL);
        log_info("[WS] upgrade %s", get_peer_ip(c));

    } else{
        mg_http_reply(c,200,
            "Content-Type: application/json\r\n",
            "{\"code\":0,\"msg\":\"WebSocket server\"}"
        );

    }
}

void ev_handler_ws_api( struct mg_connection *c, int ev, void *ev_data){
    if(ev != MG_EV_HTTP_MSG)
        return;
    struct mg_http_message *hm = ev_data;

    char uri[128]={0};
    int len = hm->uri.len < sizeof(uri)-1 ? hm->uri.len : sizeof(uri)-1;
    memcpy(uri, hm->uri. buf, len);
    uri[len]=0;
    char func_name[64] =
        "process_status";
    char *p = strstr( uri, "/ws/api/");
    if(p){
        snprintf(func_name, sizeof(func_name), "process_%s", p + 8 );
    }

    char *json_in=NULL;
    if(hm->body.len)
    {
        json_in = calloc( 1, hm->body.len + 1);
        memcpy( json_in, hm->body.buf, hm->body.len);
    }

    char *json_out=NULL;
    int rc = call_soket_demo(func_name, json_in ? json_in:"{}", &json_out);
    free(json_in);
    if(rc !=0 || !json_out)
    {
        mg_http_reply(c, 500, "Content-Type: application/json\r\n",  "{\"code\":-1,\"msg\":\"call failed\"}" );
        return;
    }

    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", json_out);
    free(json_out);
}