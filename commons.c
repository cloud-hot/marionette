#include "marionette.h"
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

/* user id from the username
 * Returns
 *  - uid on success
 *  - 1 on failure
 */
int get_uid(const char * user) {
  int uid = -1;

  struct passwd *u_psswd;

  u_psswd = getpwnam(user);
  if (u_psswd == NULL) {
    _LOGGER("(ERROR) Cannot find UID for User [%s] (Reason:%s)", user, strerror(errno));
    return uid;			/* -1 */
  }

  return (int) u_psswd->pw_uid;
}


/* group id from the groupname
 * Returns
 *  - gid on success
 *  - 1 on failure
 */
int get_gid(const char * groupname) {
  int gid = -1;

  struct group *grp;

  /* empty group name (valid) */
  if (groupname[0] == '\0') return gid;

  grp = getgrnam(groupname);
  if (grp == NULL) {
    _LOGGER("(ERROR) Cannot find GID for User [%s] (Reason:%s)", groupname, strerror(errno));
    return gid;			/* -1 */
  }

  return (int) grp->gr_gid;
}
