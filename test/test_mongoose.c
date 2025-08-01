#include "mongoose.h"

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    MG_INFO(("Received request: %.*s %.*s",
             (int) hm->method.len, hm->method.buf,
             (int) hm->uri.len, hm->uri.buf));
    mg_http_reply(c, 200, "", "Hello from Mongoose v7!\n");
  }
}

int main(void) {
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);

  if (mg_http_listen(&mgr, "http://0.0.0.0:44944", fn, NULL) == NULL) {
    MG_ERROR(("Cannot start server on port 44944"));
    return 1;
  }

  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }

  mg_mgr_free(&mgr);
  return 0;
}
