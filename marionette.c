#include "marionette.h"

void close_cb(uv_handle_t * client) {
  free(((client_ctx *)client->data)->ip);
  free(client->data);
  free(client);
  client = NULL;
}

void shutdown_cb(uv_shutdown_t* req, int status) {
  uv_close((uv_handle_t*)req->handle, close_cb);
  free(req);
}

void write_cb(uv_write_t* req, int status) {
  write_req_t* w_req;
  uv_shutdown_t* shut_req;
  int i = 0;

  /* unwrap the uv_write_t to write_req_t */
  w_req = (write_req_t*) req;

  _LOGGER("(INFO) [%s]", "Cleaning Up Request");

  /* delete the args set, if they are set */
  if (w_req->arg_v != NULL) {
    for (i=0; i<w_req->arg_c+2; i++) {
      free(w_req->arg_v[i]);
    }
    free(w_req->arg_v);
  }

  /* Free the read/write buffer and the request */
  free(w_req->buf.base);
  free(w_req->result.base);	  /* processing result */
  free(w_req->worker);
  free(w_req);
  w_req = NULL;
  
  /* all good, then return */
  if (status == 0)
    return;

  /* if error */
  _LOGGER("uv_write error: %s\n", uv_strerror(uv_last_error(loop)));

  /* lets close/shutdown because status != 0 */
  shut_req = (uv_shutdown_t*) malloc(sizeof(uv_shutdown_t));
  uv_shutdown(shut_req, (uv_stream_t*)req->handle, shutdown_cb);  // uv_stream_t is a subclass of uv_handle_t

  // i could have used the below too, but for consitency i am sticking with shutdown
  // uv_close((uv_handle_t*)req->handle, close_cb);
  
  return;
}

void read_cb(uv_stream_t * stream, ssize_t nread, uv_buf_t buf) {
  uv_shutdown_t * shut_req;		/* Shutdown the outgoing (write) side of a duplex stream. */

  /* lets shutdown the connection, we are not happy either way w.r.t data size */
  if (nread > READ_SIZE || nread < 0) {
    if (nread > READ_SIZE)
      _LOGGER("Received nread with size [%d] way more than expected [client:%s]", nread, ((client_ctx *)stream->data)->ip);
    if (nread < 0) {
      _LOGGER("(INFO) Closing Connection with Client [%s]", ((client_ctx *)stream->data)->ip, nread);
    }
    /* free the buffer */
    if (buf.base) {
      free(buf.base);
    }

    /* shutdown the write */
    shut_req = (uv_shutdown_t*) malloc(sizeof(uv_shutdown_t));
    uv_shutdown(shut_req, stream, shutdown_cb);

    return;
  }

  /* incase no data was send, we are totally OK with it */
  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf.base);
    return;
  }

  /* we will free the read buffer in process_data */

  /* process the request from the client */
  process_data(stream, nread, buf);

  return;
}

void process_data(uv_stream_t * stream, ssize_t nread, uv_buf_t buf) {
  int r;			/* return */
  write_req_t * w_req;
  struct sockaddr_in addr;	/* get client name */
  int len = sizeof(addr);
  struct sockaddr_in *s;
  int process_error = 0;	/* error while processing */

  /* lets make a copy of what is read */
  w_req = (write_req_t *)malloc(sizeof(write_req_t));
  w_req->buf = uv_buf_init((char*) malloc(nread+1), nread+1); /* buffer */
  memcpy(w_req->buf.base, buf.base, nread);
  w_req->buf.base[nread] = '\0'; /* we don't send \0 */

  /* have the write point to the client context */
  w_req->c_ctx = (client_ctx *)stream->data;
  /* increment the request counter */
  w_req->c_ctx->req_cnt++;	/* globals are initialized to 0 */

  /* set arg_v to NULL */
  w_req->arg_v = NULL;

  /* create the worker */
  w_req->worker = (child_worker *)malloc(sizeof(child_worker));
  bzero(w_req->worker, sizeof(child_worker)); /* we need to set options to NULL and then start adding as per reqmnt */

  /* clear the read buffer */
  free(buf.base);

  //  process the request
  process_error = process_request(w_req, stream);

  /* TODO: do something with process_error */
  // just log??

  return;
}

/* allocate_buffer */
uv_buf_t alloc_buffer(uv_handle_t * handle, size_t size) {
  return uv_buf_init(malloc(size), size);
}

/* - accept the connection
 * - client init
 * - read_start
 * - TODO: close conenction if host is not in whitelist!
 */
void connection_cb(uv_stream_t * server, int status) {
  uv_tcp_t * client = NULL; /* new tcp client we are about to accept */
  int r;		    /* return */
  struct sockaddr_in addr;  /* get client name */
  int len = sizeof(addr);
  struct sockaddr_in *s;

  /* check for listen status on whether we can accept */
  if (status == -1) {
    _LOGGER("(ERROR) Listen Error : %s", uv_strerror(uv_last_error(loop)));
    return;
  }

  /* create room for client */
  client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));

  fflush(stderr);
  /* initialize the new client */
  r = uv_tcp_init(loop, client);
  if (r) {
    _LOGGER("(ERROR) TCP Init Error : %s", uv_strerror(uv_last_error(loop)));
    free(client);
    return;
  }

  /* bind the client to server to start the communication */
  r = uv_accept(server, (uv_stream_t *) client);
  if (r) {
    /* lets close the client stream, we got an error */
    uv_close((uv_handle_t *) client, NULL);
    _LOGGER("(ERROR) TCP Bind Error : %s", uv_strerror(uv_last_error(loop)));
  } else {			/* if bind is SUCCESSFUL */
    /* lets store the client ctx */
    client->data = (client_ctx *)malloc(sizeof(client_ctx));

    /* get client ip */
    r = uv_tcp_getpeername(client, (struct sockaddr *)&addr, &len);
    s = (struct sockaddr_in *)&addr;
    /* store the client ip into client ctx */
    ((client_ctx *)client->data)->ip = (char *)malloc(strlen(inet_ntoa(addr.sin_addr)));
    strcpy(((client_ctx *)client->data)->ip, inet_ntoa(addr.sin_addr));

    _LOGGER("(INFO) New Connection From: %s", inet_ntoa(addr.sin_addr));

    /* start reading from the client */
    r = uv_read_start((uv_stream_t *) client, alloc_buffer, read_cb);
    if (r) {
      uv_close((uv_handle_t *) client, NULL);
      _LOGGER("(ERROR) Read_Start Error : %s", uv_strerror(uv_last_error(loop)));
    }
  }

  return;
}

/* contraption starts here
 * - start tcp server
 * - listen move to callback
 * Return: 
 *  return value of uv_run(..)
 */
int start_marionette(void) {
  int r;			/* return */

  loop = uv_default_loop();	/* we use the default loop */

  /* convert human readable port and ip to struct */
  struct sockaddr_in addr = uv_ip4_addr(SERVER, PORT);

  /* initialize the tcp server */
  r = uv_tcp_init(loop, &server);
  if (r)
    FATAL("%s", "Socket Creation Error");

  /* bind the server to the address */
  r = uv_tcp_bind(&server, addr);
  if (r) 
    FATAL("%s", "Bind Error");

  /* listen to the port */
  r = uv_listen((uv_stream_t *) &server, SOMAXCONN, connection_cb);
  if (r)
    FATAL("%s", uv_strerror(uv_last_error(loop)));

  _LOGGER("(INFO) Marionette Server Started (Port:%d) (Listen:%s) (Conf Dir:%s)", PORT, SERVER, MARIONETTE_DIR);
  /* execute all tasks in queue */
  return uv_run(loop, UV_RUN_DEFAULT);
}

/* main */
int main(void) {
  /* new world order */
  return start_marionette();
}
