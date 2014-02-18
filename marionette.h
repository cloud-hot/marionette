/***********/
/* HEADERS */
/***********/

#include <stdio.h>
#include <uv.h>			/* libuv for all cool event stuffs */
#include <stdlib.h>		/* abort, malloc, free ... */
#include <string.h>		/* memcpy */
#include <sys/socket.h>		/* struct sockaddr */
#include <ctype.h>		/* iscntrl */
#include <errno.h>		/* strerr */


/**********/
/* DEFINE */
/**********/

/* rudimentary logger */
#define _LOGGER(fmt, ...) \
  do { fprintf(stderr, "%s:%d:%s()# " fmt "\n", __FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)

/* Die with loggs error. */
#define FATAL(fmt, ...)			      \
  do {                                        \
  fprintf(stderr, "(FATAL) %s:%d:%s()# " fmt "\n", __FILE__, __LINE__, __func__, __VA_ARGS__); \
  fflush(stderr);                             \
  abort();                                    \
  } while (0)


/* server port */
#define PORT 9990
/* listening interface */
#define SERVER "0.0.0.0"

/* max buffer size, i don't want to have buffer overflow attacks */
#define READ_SIZE 4096

/* max file content */
#define FILE_SIZE 1024

/* parent dir to find the custom command files */
#define MARIONETTE_DIR "/etc/marionette/"

/* dangerous chars */
#define DANGEROUS ";|&";       /* list of dangerous characters */

/* seperators */
#define CTRL_A 1
#define CTRL_E 5

/* status codes */
#define STATUS_LEN 3
#define STATUS_SIZE sizeof(char) * STATUS_LEN
#define START_DONUT_BAKING "200"	/* started the command */
#define END_DONUT_BAKING   "201"	/* ended successfully */
#define NO_DONUT_BAKING    "100"	/* execute failed, could be any reason (EXEC didn't happen) */
#define NO_DONUT_FOR_YOU   "101"	/* no command found */
#define ERR_DONUT_EXEC     "102"	/* execvp failed */
#define ERR_DONUT_BAKING   "103"	/* exec command failed */


/***********/
/* GLOBALS */
/***********/

uv_tcp_t server;		/* tcp server object */
uv_loop_t * loop;		/* our event loop */

/* we can't share the process object, 1:1 process to request  */
typedef struct {
  uv_process_t process;
  uv_process_options_t options;
} child_worker;

/* client struct, to store custom data */
typedef struct {
  int req_cnt;			/* request count */
  char *ip;			/* client name */
  /* TODO: Add more? */
} client_ctx;

/* incoming request data and contexts per execute request */
typedef struct {
  uv_write_t req;
  uv_buf_t buf;			/* data pointer of request */
  uv_buf_t result;		/* result of the processing (created by processing function) */
  char **arg_v;			/* user arguments */
  int arg_c;			/* arg count */
  client_ctx * c_ctx;
  child_worker *worker;
} write_req_t;

/* context of sub process */
typedef struct {
  write_req_t * w_req;
  uv_stream_t * client;
} process_req_ctx;

/************************/
/* FUNCTION DEFINITIONS */
/************************/

/* each function is documented in the function declaration */

/* marionette.c */
void process_data(uv_stream_t *, ssize_t, const uv_buf_t *);
void close_cb(uv_handle_t *);
void shutdown_cb(uv_shutdown_t * , int );
void write_cb(uv_write_t* , int );
void read_cb(uv_stream_t * , ssize_t, const uv_buf_t *);
void alloc_buffer(uv_handle_t *, size_t, uv_buf_t*);
void connection_cb(uv_stream_t *, int);

/* request.c */
int is_default_command(const char *, const char *);
int is_custom_command(const char *, char *);
int process_request(write_req_t *, uv_stream_t *);
int execute_command(const char*, write_req_t *, uv_stream_t *);
int get_command_args(char **, const char *, int);

/* json_parser.c */
int parse_json_file(const char *, int *, int *, char *, char *, char *);
int parse_json(const char *, int *, int *, char *, char *, char *);

/* commons.c */
int get_uid(const char *);
int get_gid(const char *);

/* spawn_process.c */
int spawn_command(uv_stream_t *, write_req_t *, int , int , int , char *);
void on_child_exit(uv_process_t *, int64_t, int);
