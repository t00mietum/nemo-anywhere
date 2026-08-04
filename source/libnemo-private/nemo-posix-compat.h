/* Minimal POSIX user/group/ownership shims for non-Unix builds.
 *
 * Nemo's ownership and permission handling is Unix-centric (uid/gid, the
 * passwd/group database, chown). Windows has none of that, so on !G_OS_UNIX
 * these fall back to harmless no-ops: no user/group database (empty lists,
 * NULL lookups), never root, chown trivially succeeds. Callers compile
 * unchanged and naturally degrade - the owner/group UI shows nothing and
 * set-owner/set-group operations fail cleanly. */

#ifndef NEMO_POSIX_COMPAT_H
#define NEMO_POSIX_COMPAT_H

#include <glib.h>

#ifdef G_OS_UNIX

#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#else /* !G_OS_UNIX */

typedef int uid_t;
typedef int gid_t;

struct passwd {
	char  *pw_name;
	char  *pw_dir;
	char  *pw_gecos;
	uid_t  pw_uid;
	gid_t  pw_gid;
};

struct group {
	char  *gr_name;
	gid_t  gr_gid;
	char **gr_mem;
};

/* Nonzero so nemo_user_is_root() (geteuid()==0) is always false here. */
#define NEMO_COMPAT_FAKE_UID 1000

static inline uid_t getuid  (void) { return NEMO_COMPAT_FAKE_UID; }
static inline uid_t geteuid (void) { return NEMO_COMPAT_FAKE_UID; }
static inline int   chown   (const char *path, uid_t owner, gid_t grp)
	{ (void) path; (void) owner; (void) grp; return 0; }

static inline struct passwd *getpwuid (uid_t uid)      { (void) uid;  return NULL; }
static inline struct passwd *getpwnam (const char *nm) { (void) nm;   return NULL; }
static inline struct group  *getgrgid (gid_t gid)      { (void) gid;  return NULL; }
static inline struct group  *getgrnam (const char *nm) { (void) nm;   return NULL; }

static inline void           setpwent (void) { }
static inline struct passwd *getpwent (void) { return NULL; }
static inline void           endpwent (void) { }
static inline void           setgrent (void) { }
static inline struct group  *getgrent (void) { return NULL; }
static inline void           endgrent (void) { }

static inline int getgroups (int size, gid_t list[])
	{ (void) size; (void) list; return 0; }

#ifndef NGROUPS_MAX
#define NGROUPS_MAX 65536
#endif

/* setuid/setgid/sticky mode bits absent from mingw <sys/stat.h>. */
#ifndef S_ISUID
#define S_ISUID 04000
#endif
#ifndef S_ISGID
#define S_ISGID 02000
#endif
#ifndef S_ISVTX
#define S_ISVTX 01000
#endif

#endif /* G_OS_UNIX */

#endif /* NEMO_POSIX_COMPAT_H */
