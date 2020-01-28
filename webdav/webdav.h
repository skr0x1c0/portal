//
//  webdav.h
//  pwndav
//
//  Created by Chakra on 27/01/20.
//  Copyright © 2020 Sreejith Krishnan R. All rights reserved.
//

#ifndef webdav_h
#define webdav_h

#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

/* Webdav file type constants */
#define WEBDAV_FILE_TYPE    1
#define WEBDAV_DIR_TYPE      2

typedef int webdav_filetype_t;

// XXX Dependency on __DARWIN_64_BIT_INO_T
// We cannot pass ino_t back and forth between the kext and
// webdavfs_agent, because ino_t is in flux right now.
// In user space ino_t is 64-bits, but 32 bits in kernel space.
// So we have to define our own type for now (webdav_ino_t).
//
// This was the root cause of:
// <rdar://problem/6491194> 10A245+10A246: WebDAV FS complains about file names being too long.
//
// Once ino_t is identical in length for kernel and user space, then
// we can get rid of webdav_ino_t and just use ino_t exclusively.
//
typedef uint32_t webdav_ino_t;

/*
 * An opaque_id is used to find a file system object in userland.
 * Lookup returns it. The rest of the operations which act upon a file system
 * object use it. If the file system object in userland goes away, the opaque_id
 * will be invalidated and messages to userland will fail with ESTALE.
 * The opaque_id 0 (kInvalidOpaqueID) is never valid.
 */
#define kInvalidOpaqueID   0
typedef uint32_t opaque_id;

enum
{
  /*
   * An opaque id is composed of two parts.  The lower half is an index into
   * a table which holds the actual data for the opaque id.  The upper half is a
   * counter which insures that a particular opaque id value isn't reused for a long
   * time after it has been disposed.  Currently, with 20 bits as the index, there
   * can be (2^20)-2 opaque ids in existence at any particular point in time
   * (index 0 is not used), and no opaque id value will be re-issued more frequently than every
   * 2^12th (4096) times. (although, in practice many more opaque ids will be issued
   * before one is re-used).
   */
  kOpaqueIDIndexBits = 20,
  kOpaqueIDIndexMask = ((1 << kOpaqueIDIndexBits) - 1),
  kOpaqueIDMaximumCount = (1 << kOpaqueIDIndexBits) - 1, /* all 0 bits are never a valid index */
  
  /*
   * This keeps a little 'extra room' in the table, so that if it gets
   * nearly full that a dispose and re-allocate won't quickly run thru the couple
   * couple free slots in the table.  So, if there are less than this number of
   * available items, the table will get grown.
   */
  kOpaqueIDMinimumFree = 1024,
  /* When the table is grown, grow by this many entries */
  kOpaqueIDGrowCount = 2048
};

/*
 * Create an MPOpaqeID by masking together a count and and index.
 */
#define CreateOpaqueID(counter, index) (opaque_id)(((counter) << kOpaqueIDIndexBits) | ((index) & kOpaqueIDIndexMask))

struct webdav_vfsstatfs
{
  uint32_t  f_bsize;  /* fundamental file system block size */
  uint32_t  f_iosize;  /* optimal transfer block size */
  uint64_t  f_blocks;  /* total data blocks in file system */
  uint64_t  f_bfree;  /* free blocks in fs */
  uint64_t  f_bavail;  /* free blocks avail to non-superuser */
  uint64_t  f_files;  /* total file nodes in file system */
  uint64_t  f_ffree;  /* free file nodes in fs */
};

struct webdav_args
{
  char *pa_mntfromname;            /* mntfromname */
  int  pa_version;                /* argument struct version */
  int pa_socket_namelen;            /* Socket to server name length */
  struct sockaddr *pa_socket_name;      /* Socket to server name */
  char *pa_vol_name;              /* volume name */
  u_int32_t pa_flags;              /* flag bits for mount */
  u_int32_t pa_server_ident;          /* identifies some (not all) types of servers we are connected to */
  opaque_id pa_root_id;            /* root opaque_id */
  webdav_ino_t pa_root_fileid;        /* root fileid */
  uid_t    pa_uid;              /* effective uid of the mounting user */
  gid_t    pa_gid;              /* effective gid of the mounting user */
  off_t pa_dir_size;              /* size of directories */
  /* pathconf values: >=0 to return value; -1 if not supported */
  int pa_link_max;              /* maximum value of a file's link count */
  int pa_name_max;              /* The maximum number of bytes in a file name (does not include null at end) */
  int pa_path_max;              /* The maximum number of bytes in a relative pathname (does not include null at end) */
  int pa_pipe_buf;              /* The maximum number of bytes that can be written atomically to a pipe (usually PIPE_BUF if supported) */
  int pa_chown_restricted;          /* Return _POSIX_CHOWN_RESTRICTED if appropriate privileges are required for the chown(2) */
  int pa_no_trunc;              /* Return _POSIX_NO_TRUNC if file names longer than KERN_NAME_MAX are truncated */
  /* end of webdav_args version 1 */
  struct webdav_vfsstatfs pa_vfsstatfs;        /* need this to fill out the statfs struct during the mount */
};

/*
 * kCurrentWebdavArgsVersion MUST be incremented anytime changes are made in
 * either the WebDAV file system's kernel or user-land code which require both
 * executables to be released as a set.
 */
#define kCurrentWebdavArgsVersion 5

#define kServerIdent 0;

/* special file ID values */
#define WEBDAV_ROOTFILEID 3

/* sizes passed to the kernel file system */
#define WEBDAV_DIR_SIZE 2048

#pragma pack(push, 1)
struct webdav_cred
{
  uid_t pcr_uid;                /* From ucred */
};

/* WEBDAV_LOOKUP */
struct webdav_request_lookup
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    dir_id;        /* directory to search */
  int        force_lookup;    /* if TRUE, don't use a cached lookup */
  uint32_t    name_length;    /* length of name */
  char      name[];        /* filename to find */
};

struct webdav_timespec64
{
  uint64_t  tv_sec;
  uint64_t  tv_nsec;
};

struct webdav_reply_lookup
{
  opaque_id    obj_id;        /* opaque_id of object corresponding to name */
  webdav_ino_t  obj_fileid;      /* object's file ID number */
  webdav_filetype_t obj_type;      /* WEBDAV_FILE_TYPE or WEBDAV_DIR_TYPE */
  struct webdav_timespec64 obj_atime;    /* time of last access */
  struct webdav_timespec64 obj_mtime;    /* time of last data modification */
  struct webdav_timespec64 obj_ctime;    /* time of last file status change */
  struct webdav_timespec64 obj_createtime; /* file creation time */
  off_t      obj_filesize;    /* filesize of object */
};

/* WEBDAV_CREATE */
struct webdav_request_create
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    dir_id;        /* The opaque_id for the directory in which the file is to be created */
  mode_t      mode;        /* file type and initial file access permissions for the file */
  uint32_t    name_length;    /* length of name */
  char      name[];        /* The name that is to be associated with the created file */
};

struct webdav_reply_create
{
  opaque_id    obj_id;        /* opaque_id of file corresponding to name */
  webdav_ino_t  obj_fileid;      /* file's file ID number */
};

/* WEBDAV_MKDIR */
struct webdav_request_mkdir
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    dir_id;        /* The opaque_id for the directory in which the file is to be created */
  mode_t      mode;        /* file type and initial file access permissions for the file */
  uint32_t    name_length;    /* length of name */
  char      name[];        /* The name that is to be associated with the created directory */
};

struct webdav_reply_mkdir
{
  opaque_id    obj_id;        /* opaque_id of directory corresponding to name */
  webdav_ino_t  obj_fileid;      /* directory's file ID number */
};

/* WEBDAV_OPEN */
struct webdav_request_open
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of object */
  int        flags;        /* file access flags (O_RDONLY, O_WRONLY, etc.) */
  int        ref;        /* the reference to the webdav object that the cache object should be associated with */
};

struct webdav_reply_open
{
  pid_t      pid;        /* process ID of file system daemon (for matching to ref's pid) */
};

/* WEBDAV_CLOSE */
struct webdav_request_close
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of object */
};

struct webdav_reply_close
{
};

struct webdav_stat {
  dev_t     st_dev;    /* [XSI] ID of device containing file */
  webdav_ino_t st_ino;  /* [XSI] File serial number */
  mode_t     st_mode;  /* [XSI] Mode of file (see below) */
  nlink_t    st_nlink;  /* [XSI] Number of hard links */
  uid_t    st_uid;    /* [XSI] User ID of the file */
  gid_t    st_gid;    /* [XSI] Group ID of the file */
  dev_t    st_rdev;  /* [XSI] Device ID */
  struct  webdav_timespec64 st_atimespec;  /* time of last access */
  struct  webdav_timespec64 st_mtimespec;  /* time of last data modification */
  struct  webdav_timespec64 st_ctimespec;  /* time of last status change */
  struct  webdav_timespec64 st_createtimespec;  /* time file was created */
  off_t    st_size;  /* [XSI] file size, in bytes */
  blkcnt_t  st_blocks;  /* [XSI] blocks allocated for file */
  blksize_t  st_blksize;  /* [XSI] optimal blocksize for I/O */
  uint32_t  st_flags;  /* user defined flags for file */
  uint32_t  st_gen;    /* file generation number */
};

/* WEBDAV_GETATTR */
struct webdav_request_getattr
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of object */
};

struct webdav_reply_getattr
{
  struct webdav_stat  obj_attr;      /* attributes for the object */
};

/* WEBDAV_SETATTR XXX not needed at this time */
struct webdav_request_setattr
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of object */
  struct stat    new_obj_attr;    /* new attributes of the object */
};

struct webdav_reply_setattr
{
};

/* WEBDAV_READ */
struct webdav_request_read
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of file object */
  off_t      offset;        /* position within the file object at which the read is to begin */
  uint64_t    count;        /* number of bytes of data to be read (limited to WEBDAV_MAX_IO_BUFFER_SIZE (8000-bytes)) */
};

struct webdav_reply_read
{
};

/* WEBDAV_WRITE XXX not needed at this time */
struct webdav_request_write
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of file object */
  off_t      offset;        /* position within the file object at which the write is to begin */
  uint64_t    count;        /* number of bytes of data to be written (limited to WEBDAV_MAX_IO_BUFFER_SIZE (8000-bytes)) */
  char      data[];        /* data to be written to the file object */
};

struct webdav_reply_write
{
  uint64_t    count;        /* number of bytes of data written to the file */
};

/* WEBDAV_FSYNC */
struct webdav_request_fsync
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of object */
};

struct webdav_reply_fsync
{
};

/* WEBDAV_REMOVE */
struct webdav_request_remove
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of entry to remove */
};

struct webdav_reply_remove
{
};

/* WEBDAV_RMDIR */
struct webdav_request_rmdir
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of directory object to remove */
};

struct webdav_reply_rmdir
{
};

/* WEBDAV_RENAME */
struct webdav_request_rename
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    from_dir_id;    /* opaque_id for the directory from which the entry is to be renamed */
  opaque_id    from_obj_id;    /* opaque_id for the object to be renamed */
  opaque_id    to_dir_id;      /* opaque_id for the directory to which the object is to be renamed */
  opaque_id    to_obj_id;      /* opaque_id for the object's new location if it exists (may be NULL) */
  uint32_t    to_name_length;    /* length of to_name */
  char      to_name[];      /* new name for the object */
};

struct webdav_reply_rename
{
};

/* WEBDAV_READDIR */
struct webdav_request_readdir
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of directory to read */
  int        cache;        /* if TRUE, perform additional caching */
};

struct webdav_reply_readdir
{
};

/* WEBDAV_STATFS */

struct webdav_statfs {
  uint64_t  f_bsize;    /* fundamental file system block size */
  uint64_t  f_iosize;    /* optimal transfer block size */
  uint64_t  f_blocks;    /* total data blocks in file system */
  uint64_t  f_bfree;    /* free blocks in fs */
  uint64_t  f_bavail;    /* free blocks avail to non-superuser */
  uint64_t  f_files;    /* total file nodes in file system */
  uint64_t  f_ffree;    /* free file nodes in fs */
};

struct webdav_request_statfs
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id  root_obj_id;      /* opaque_id of the root directory */
};

struct webdav_reply_statfs
{
  struct webdav_statfs   fs_attr;    /* file system information */
  /*
   * (required: f_bsize, f_iosize, f_blocks, f_bfree,
   * f_bavail, f_files, f_ffree. The kext will either copy
   * the remaining info from the mount struct, or the cached
   * statfs struct in the mount struct IS the destination.
   */
};

/* WEBDAV_UNMOUNT */
struct webdav_request_unmount
{
  struct webdav_cred pcr;        /* user and groups */
};

struct webdav_reply_unmount
{
};

/* WEBDAV_INVALCACHES */
struct webdav_request_invalcaches
{
  struct webdav_cred pcr;        /* user and groups */
};

struct webdav_reply_invalcaches
{
};

/* WEBDAV_SHOW_COOKIES */
/* WEBDAV_RESET_COOKIES */
struct webdav_request_cookies {
  struct webdav_cred pcr;        /* user and groups */
};

struct webdav_reply_cookies {
  
};

struct webdav_request_writeseq
{
  struct webdav_cred pcr;        /* user and groups */
  opaque_id    obj_id;        /* opaque_id of file object */
  off_t      offset;        /* position within the file object at which the write is to begin */
  off_t      count;        /* number of bytes of data to be written (limited to WEBDAV_MAX_IO_BUFFER_SIZE (8000-bytes)) */
  uint64_t    file_len;      /* length of the file after all sequential writes are done */
  uint32_t    is_retry;      /* non-zero indicates this request is a retry due to an EPIPE */
};

struct webdav_reply_writeseq
{
  uint64_t    count;        /* number of bytes of data written to the file */
};

union webdav_request
{
  struct webdav_request_lookup  lookup;
  struct webdav_request_create  create;
  struct webdav_request_open    open;
  struct webdav_request_close    close;
  struct webdav_request_getattr   getattr;
  struct webdav_request_setattr  setattr;
  struct webdav_request_read    read;
  struct webdav_request_write    write;
  struct webdav_request_fsync    fsync;
  struct webdav_request_remove  remove;
  struct webdav_request_rmdir    rmdir;
  struct webdav_request_rename  rename;
  struct webdav_request_readdir  readdir;
  struct webdav_request_statfs  statfs;
  struct webdav_request_invalcaches invalcaches;
  struct webdav_request_writeseq  writeseq;
};

union webdav_reply
{
  struct webdav_reply_lookup    lookup;
  struct webdav_reply_create    create;
  struct webdav_reply_open    open;
  struct webdav_reply_close    close;
  struct webdav_reply_getattr    getattr;
  struct webdav_reply_setattr    setattr;
  struct webdav_reply_read    read;
  struct webdav_reply_write    write;
  struct webdav_reply_fsync    fsync;
  struct webdav_reply_remove    remove;
  struct webdav_reply_rmdir    rmdir;
  struct webdav_reply_rename    rename;
  struct webdav_reply_readdir    readdir;
  struct webdav_reply_statfs    statfs;
  struct webdav_reply_invalcaches  invalcaches;
  struct webdav_reply_writeseq  writeseq;
};
#pragma pack(pop)

/* Webdav file operation constants */
#define WEBDAV_LOOKUP      1
#define WEBDAV_CREATE      2
#define WEBDAV_OPEN        3
#define WEBDAV_CLOSE      4
#define WEBDAV_GETATTR      5
#define WEBDAV_SETATTR      6
#define WEBDAV_READ        7
#define WEBDAV_WRITE      8
#define WEBDAV_FSYNC      9
#define WEBDAV_REMOVE      10
#define WEBDAV_RENAME      11
#define WEBDAV_MKDIR      12
#define WEBDAV_RMDIR      13
#define WEBDAV_READDIR      14
#define WEBDAV_STATFS      15
#define WEBDAV_UNMOUNT      16
#define WEBDAV_INVALCACHES    17
/* for the future */
#define WEBDAV_LINK        18
#define WEBDAV_SYMLINK      19
#define WEBDAV_READLINK      20
#define WEBDAV_MKNOD      21
#define WEBDAV_GETATTRLIST    22
#define WEBDAV_SETATTRLIST    23
#define WEBDAV_EXCHANGE      24
#define WEBDAV_READDIRATTR    25
#define WEBDAV_SEARCHFS      26
#define WEBDAV_COPYFILE      27
#define WEBDAV_WRITESEQ      28
#define WEBDAV_DUMP_COOKIES    29
#define WEBDAV_CLEAR_COOKIES  30

/*
 * If name[0] is WEBDAV_ASSOCIATECACHEFILE_SYSCTL, then
 *    name[1] = a pointer to a struct open_associatecachefile
 *    name[2] = fd of cache file
 */
#define WEBDAV_ASSOCIATECACHEFILE_SYSCTL   1

#endif /* webdav_h */
