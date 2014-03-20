#include "marionette.h"
#include <unistd.h>
#include <sys/stat.h>

/* is_command 
 * compare 1st n chars & (n+1)th char should be a control char
 * return 1 if it matches the command name
 *        0 if not
 */
int is_default_command(const char *msg, const char *command) {
  int len = strlen(command);
  return (strncmp(command, msg, len) == 0) && (iscntrl(msg[len]) != 0);
}

/* Check for file in MARIONETTE_DIR which has the same name as command name.
 * If the command name as ., split . and use it as directory name and look
 * for command name file inside that dir. (NOTE: Supports only one level of dir struct.)
 * Return: 0 on failure, and set filename to random crap
 *         1 on success, filename
 * Logic:
 * - check for file in MARIONETTE_DIR
 * - if not found, see whether it has . in the request
 * - if yes, check for the command name inside the dir in MARIONETTE_DIR
 * - still not found? return 0
 */
int is_custom_command(const char *msg, char *filename) {
  char *ptr;		/* position holder */
  struct stat s;	/* for stat call */
  char _msg[30];	/* store the msg name */
  bzero(_msg, 30);	/* might miss a pointer arithematic somewhere and get garbage :-) */
  
  /* check for ^e */
  ptr = strchr(msg, 5);

  if (ptr == NULL) {
    _LOGGER("(ERROR) Didn't find ^e in the request [%s]", msg);
    return 0;
  } else {
    strncpy(_msg, msg, (ptr - msg)); /* ptr points to ^e */
    strcpy(filename, MARIONETTE_DIR);
    /* if we see ^e delimit, check whether we have '.' */
    ptr = strchr(_msg, '.');
    if (ptr == NULL) {		/* we have it in top level structure */
      strcat(filename, _msg);
      if (stat(filename, &s) == 0 && S_ISREG(s.st_mode)) { /* file is there */
	return 1;
      } else {
	_LOGGER("(ERROR) File (%s) Reason (%s) (and no '.' detected)", filename, strerror(errno));
	return 0;
      }
    } else {			/* split '.' and look in the dir */
      strncat(filename, _msg, (ptr - _msg)); /* copy till '.' */
      strcat(filename, "/");		 /* dir seperator */
      strcat(filename, ptr+1); /* copy rest from '.' */
      if (stat(filename, &s) == 0 && S_ISREG(s.st_mode)) { /* file is there */
	return 1;
      } else {
	_LOGGER("(ERROR) File (%s) Reason (%s)", filename, strerror(errno));
	return 0;
      }
    }
  }

  return 1;
}

/* Execute the Command file and sets the result on to write_req_t->result
 * Returns: 0 on success, write_req_t->result will have the result
 *          != 0 on error, (write_req_t->result.base could be garbage)
 * Logic:
 * To execute the command we need
 *   - Command and its arguments
 *   - userid, groupid
 *   - number of arguments expected
 *   - check whether arguments have dangerous characters
 */
int execute_command(const char* filename, write_req_t * w_req, uv_stream_t * client) {
  int live = 1;			/* is the command active */
  char user[10] = {'\0'};	/* username */
  char group[10] = {'\0'};	/* groupname */
  char cmd[30]	= {'\0'};	/* command name */
  char **arg_v;			/* arguments */

  int uid;			/* userid */
  int gid;			/* group id */

  int r;			/* return status */
  int i = 0;
  char *dangerous = NULL;      /* did we find a dangerous character */
  char *dangers = DANGEROUS;

  r = parse_json_file(filename, &w_req->arg_c, &live, user, group, cmd);
  
  /* if r, don't execute because json parsing failed */
  if (r) {
    _LOGGER("(ERROR) [%s] Json Parsing Failed", w_req->c_ctx->ip);
    goto end;
  }

  uid = get_uid(user);
  gid = get_gid(group);

  w_req->arg_v = (char **)malloc(sizeof(char *) * (w_req->arg_c + 2)); /* +1 to add the command, +1 to NULL terminate */
  /* initialize all to NULL, so i can blindly call free :) */
  for (i=0; i<w_req->arg_c+2; i++) {
    w_req->arg_v[i] = NULL;
  }

  /* arg_v[0] should be the command */
  w_req->arg_v[0] = malloc(sizeof(char) * strlen(cmd));
  strcpy(w_req->arg_v[0], cmd);

  /* argv_v[0] is command */
  r = get_command_args(&w_req->arg_v[1], w_req->buf.base, w_req->arg_c);	/* it will put the args in arg_v and number of args found in r */

  if (r != w_req->arg_c) {
    _LOGGER("(ERROR) [%s] Arg Count doesnot match [expected:%d != received:%d]", w_req->c_ctx->ip, w_req->arg_c, r);
    r = 1;			/* error */
    goto end;
  }
  /* reset r, is passed as error status */
  r = 0;

  /* check whether arguments contain dangerous stuff */
  for (i=1; i<w_req->arg_c+1; i++) {	/* avoid command and also NULL */
    dangerous = strpbrk(w_req->arg_v[i], dangers);
    if (dangerous != NULL) {
      _LOGGER("(ERROR) [%s] Dangerous Character Found in Argument list", w_req->c_ctx->ip);
      r = 1;
      goto end;
    }
  }

  /* if uid == -1, don't execute */
  if (uid == -1) {
    r = 1;			/* set it as failure */
    goto end;
  }

  /* spawn each command */
  if (live != 1) {		/* don't start if command is not live */
    _LOGGER("(INFO) [%s] Command is not set to live [File:%s]", w_req->c_ctx->ip, filename);
    r = 1;			/* set it as failure */
  } else {
    /* write 200 before you spawn */
    memcpy(w_req->result.base, START_DONUT_BAKING, STATUS_LEN);
    uv_write(&w_req->req, client, &w_req->result, 1, NULL);
    spawn_command(client, w_req, live, uid, gid, cmd);
  }

 end:

  return r;
}

/* returns the arguments after parsing the command message 
 * Returns: 
 *   number of args processed
 * If number of args > what is expected, the return value will show the new value
 * but those args won't be copied to arg_v. Anyways we will reject the request, why
 * over complicate :-)
 * Logic:
 *  - we know that there has been a ^e (every command should have ^e)
 *  - split with ^a
*/
int get_command_args(char **arg_v, const char *msg, int arg_c) {
  int msg_len = strlen(msg);	/* total len of msg */
  char *tmp_ptr1 = NULL;	/* start point */
  char *tmp_ptr2 = NULL;	/* end point */
  int r = 0;			/* arg_c counter */
  int total_args = 0;		/* keep counting even if we have gone ahead of expected arg count */
  int len = 0;
  
  /* we will start from first ^e */
  tmp_ptr1 = strchr(msg, CTRL_E);  /* this can never be NULL (we already checked for this earlier) */
  while(tmp_ptr1 < msg + msg_len - 2) { /* -1 or \0, -1 again for 0 offset */
    total_args++;
    /* check for ^a */
    tmp_ptr2 = strchr(tmp_ptr1+1, CTRL_A); /* tmp_ptr1+1 points to the begining of args */
    if (tmp_ptr2 == NULL) {		   /* we have hit the last argument */
      if (r < arg_c) {			   /* copy only if we are within expected count */
	len = msg + msg_len - 2 - tmp_ptr1; /* len of string to be copied */
	arg_v[r] = (char *) malloc(sizeof(char) * (len + 1)); /* for \0 */
	arg_v[r][len] = '\0';
	strncpy(arg_v[r++], tmp_ptr1 + 1, (msg + msg_len - 2 - tmp_ptr1)); /* we don't need ^e at the end of msg, also increment r after it */
      }
      tmp_ptr1 = (char *)(msg + msg_len - 1); /* we have reached till the end */
    } else {
      if (r < arg_c) {   /* copy only if we are within expected count */
	len = tmp_ptr2 - tmp_ptr1 -1; /* len of string to be copied, don't include ^a */
	arg_v[r] = (char *) malloc(sizeof(char) * (len + 1)); /* for \0 */
	arg_v[r][len] = '\0';
	strncpy(arg_v[r++], tmp_ptr1+1, len);
      }
      tmp_ptr1 = tmp_ptr2;	/* move ahead */
    }
  }

  return total_args;
}

/* Process Reuest
 * - All requests should be of format command[^e] || command[^e]arg1[^a]arg2[^a]arg3[^e]
 * - check whether we have a 
 * - TODO: create like a dispatch table
 * Returns:
 *  - 0 on success
 *  - != 0 on failure
 */
int process_request(write_req_t * w_req, uv_stream_t * client) {
  int error = 0;
  char result[] = "no_donut_for_you_now";
  int len = strlen(result);

  char filename[50];		/* location of command */
  bzero(filename, 50);		/* bad maths :) */


  _LOGGER("(INFO) [%s] Request: %s", w_req->c_ctx->ip, w_req->buf.base);

  /* Commands
   * - defaults
   *    o ruok
   *    o cpu_load (TODO)
   *    o uptime (TODO)
   *    o oom_in_lasthour (TODO)
   * - custom
   *    o check for file in MARIONETTE_DIR
   */
  /* check for defaults */
  if(is_default_command(w_req->buf.base, "ruok")) {
    /* we need to append START and END messages */
    w_req->result = uv_buf_init((char *)malloc(sizeof(char)*(5+STATUS_SIZE*2)), (5+STATUS_SIZE*2));
    memcpy(w_req->result.base, START_DONUT_BAKING, STATUS_LEN); /* add start message */
    strcpy(w_req->result.base + STATUS_LEN, "iamok");		/* after we add start msg, we have moved STATUS_LEN ahead */
    memcpy(w_req->result.base + 5 + STATUS_LEN, END_DONUT_BAKING, STATUS_LEN); /* start msg+iamok */
    if (uv_is_closing((uv_handle_t *)client)) {
      _LOGGER("(ERROR) CLIENT Has Gone Away! [uv_is_closing set to 1]) [Intended Msg: %s]", "iamok");
      write_cb(&w_req->req, 0);
    } else {
      uv_write(&w_req->req, client, &w_req->result, 1, write_cb);
    }
  } else if (is_custom_command(w_req->buf.base, filename)) { 
    _LOGGER("(INFO) [%s] File Found [%s]", w_req->c_ctx->ip, filename);
    /* set aside the space for status */
    w_req->result = uv_buf_init((char *)malloc(STATUS_SIZE), STATUS_LEN);
    /* lets execute the command */
    error = execute_command(filename, w_req, client);
    if (error) {		/* write error to client, we don't have to write about success, we are doing in execute_command */
      memcpy(w_req->result.base, NO_DONUT_BAKING, STATUS_LEN);
      uv_write(&w_req->req, client, &w_req->result, 1, write_cb);
    }
  } else {			/* no match */
    _LOGGER("(ERROR) [%s] Request: [%s] neither a custom nor default command", w_req->c_ctx->ip, w_req->buf.base);
    w_req->result = uv_buf_init((char *)malloc(STATUS_SIZE), STATUS_LEN);
    memcpy(w_req->result.base, NO_DONUT_FOR_YOU, STATUS_LEN);
    if (uv_is_closing((uv_handle_t *)client)) {
      _LOGGER("(ERROR) CLIENT Has Gone Away! [uv_is_closing set to 1]) [Intended Msg: %s]", NO_DONUT_FOR_YOU);
      write_cb(&w_req->req, 0);
    } else {
      uv_write(&w_req->req, client, &w_req->result, 1, write_cb);
    }
  }

  return error;
}
