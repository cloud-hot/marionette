#include "marionette.h"

void on_child_exit(uv_process_t *req, int exit_status, int term_signal) {
  uv_buf_t a;
  int i = 0;
  process_req_ctx *p_ctx;
  p_ctx = (process_req_ctx *)req->data;
  a = uv_buf_init((char *)malloc(STATUS_SIZE), STATUS_LEN);

  /* if is closing */
  if (uv_is_closing((uv_handle_t *)p_ctx->client)) {
    _LOGGER("(ERROR) CLIENT Has Gone Away! [uv_is_closing set to 1] Process exited with status %d, signal %d [Bug in the Client]", exit_status, term_signal);
    write_cb(&p_ctx->w_req->req, 0);
    goto end;
  }

  _LOGGER("(INFO) [%s] Process exited with status %d, signal %d [Command:%s]", p_ctx->w_req->c_ctx->ip, exit_status, term_signal, p_ctx->w_req->arg_v[0]);
  if (exit_status < 0) {
    memcpy(a.base, ERR_DONUT_EXEC, STATUS_LEN);
  } else if (exit_status > 0) {
    memcpy(a.base, ERR_DONUT_BAKING, STATUS_LEN);
  } else {
    memcpy(a.base, END_DONUT_BAKING, STATUS_LEN);
  }
  /* write the status */
  uv_write(&p_ctx->w_req->req, p_ctx->client, &a, 1, write_cb);

 end:
  free(a.base);
  free(p_ctx);
  uv_close((uv_handle_t*) req, NULL);

  return;
}

int spawn_command(uv_stream_t *client, write_req_t *w_req, int live, int uid, int gid, char *command) {
  process_req_ctx *p_ctx; /* we need to store to wrap the request to notify the client on worse failures */
  int i;

  p_ctx = (process_req_ctx *)malloc(sizeof(process_req_ctx));

  uv_stdio_container_t child_stdio[3];
  child_stdio[0].flags = UV_IGNORE;
  child_stdio[1].flags = UV_INHERIT_STREAM;
  child_stdio[1].data.stream = client;
  child_stdio[2].flags = UV_INHERIT_STREAM;
  child_stdio[2].data.stream = client;

  w_req->worker->options.stdio_count = 3;
  w_req->worker->options.stdio = child_stdio;

  w_req->worker->options.flags |= UV_PROCESS_SETUID;
  w_req->worker->options.uid = uid;

  w_req->worker->options.cwd = NULL;

  if (gid != -1) {
    w_req->worker->options.flags |= UV_PROCESS_SETGID;
    w_req->worker->options.gid = gid;
  }

  w_req->worker->options.exit_cb = on_child_exit;
  w_req->worker->options.file = w_req->arg_v[0];
  w_req->worker->options.args = w_req->arg_v;

  p_ctx->w_req = w_req;
  p_ctx->client = client;

  /* log this request */
  _LOGGER("(INFO) [%s] Running [%s]", p_ctx->w_req->c_ctx->ip, w_req->arg_v[0]);
  for(i=0; i < w_req->arg_c; i++) {
    _LOGGER("(INFO) [%s] Command [%s] Argument[%d] [%s]", p_ctx->w_req->c_ctx->ip, w_req->arg_v[0], i, w_req->arg_v[i+1]);
  }

  w_req->worker->process.data = (void *) p_ctx;

  if (uv_spawn(loop, &w_req->worker->process, w_req->worker->options)) {
    _LOGGER("(ERROR) uv_spawn (%s)", uv_strerror(uv_last_error(loop)));
    return 1;
  }

  return 1;
}
