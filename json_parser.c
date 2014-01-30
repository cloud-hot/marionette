#include "marionette.h"
#include <json/json.h>


/* Parses the given JSON String 
 * Returns
 *  - 0 on success and all the arguments will be pointing to parsed data
 *  - 1 on failure and garbage elsewhere
 */
int parse_json(const char* json_str, int * arg_c, int *live, char * user, char * group, char * command) {
  json_object * jobj;
  json_object *j_tmp;		/* tmp json object to store intermediate values */

  jobj = json_tokener_parse(json_str);
  /* if json parsing has failed */
  if (jobj == NULL) {
    json_object_put(jobj);
    _LOGGER("(ERROR) Json Parsing Failed for JSON String [%s]", json_str);
    return 1;
  }

  /* get command */
  j_tmp = json_object_object_get(jobj, "command");
  if (j_tmp == NULL) {
    json_object_put(jobj);
    return 1;
  }
  strcpy(command, json_object_get_string(j_tmp));

  /* get user */
  j_tmp = json_object_object_get(jobj, "runas_user");
  if (j_tmp == NULL) {
    json_object_put(jobj);
    return 1;
  }
  strcpy(user, json_object_get_string(j_tmp));

  /* get group, if group is missing, it is ok */
  j_tmp = json_object_object_get(jobj, "runas_group");
  if (j_tmp == NULL) {
    group[0] = '\0';
  } else {
    strcpy(group, json_object_get_string(j_tmp));
  }

  /* get argc */
  j_tmp = json_object_object_get(jobj, "argc");
  if (j_tmp == NULL) {
    json_object_put(jobj); 
    return 1;
  }
  *arg_c = json_object_get_int(j_tmp);

  /* get live, if set take it */
  j_tmp = json_object_object_get(jobj, "live");
  if (j_tmp != NULL)   
    *live = json_object_get_int(j_tmp);

  json_object_put(jobj);

  return 0;
}

/* Parses the given JSON File 
 * Returns
 *  - 0 on success and all the arguments will be pointing to parsed data
 *  - 1 on failure and garbage elsewhere
 */
int parse_json_file(const char *filename, int *arg_c, int *live, char *user, char *group, char *command) {
  int fd;			/* file descriptor */
  int r = 1;			/* result (default, failure) */
  char json_str[FILE_SIZE+10];	/* file content store */

  /* slurp the file contents */
  fd = open(filename, R_OK);
  if (fd == -1) {
    _LOGGER("(ERROR) Cannot Open File [%s] Reason [%s]", filename, strerror(errno));
    return 1;
  }

  r = read(fd, json_str, FILE_SIZE);
  if (r <= 0 || r >= FILE_SIZE) {
    _LOGGER("File size for file (%s) unexpected expected size (Expected Size:%d)", filename, FILE_SIZE);
    return 1;
  }
  json_str[r] = '\0';		/* more efficient than bzero :-) */

  close(fd);

  r = parse_json(json_str, arg_c, live, user, group, command);

  return r;
}
