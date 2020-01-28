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
#include <pthread.h>

#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/un.h>

#include "webdav_common.h"

#define SOCKET_PATH _PATH_TMP ".proxyUDS.XXXXXX"

struct handler_ctx {
  struct sockaddr_un un;
} handler_ctx_t;

int round_trip(void *ctx, int operation, union webdav_request* request, size_t request_len, void* var_data, size_t var_data_size, void* reply, size_t reply_len) {
  struct handler_ctx* handler_ctx = (struct handler_ctx*)ctx;
  
  struct msghdr msg;
  struct iovec iov[3];
  bzero(&msg, sizeof(msg));
  bzero(iov, sizeof(iov));
  
  msg.msg_iov = iov;
  iov[0].iov_len = sizeof(int);
  iov[0].iov_base = (caddr_t)&operation;
  iov[1].iov_len = request_len;
  iov[1].iov_base = (caddr_t)request;
  
  if (var_data_size > 0) {
    iov[2].iov_len = var_data_size;
    iov[2].iov_base = var_data;
    msg.msg_iovlen = 3;
  } else {
    msg.msg_iovlen = 2;
  }
  
  int socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    printf("webdav_agent socket creation failed, error: %d \n", errno);
    return errno;
  }
  
  int result = connect(socket_fd, (const struct sockaddr *) &handler_ctx->un, sizeof(handler_ctx->un));
  if (result != 0) {
    printf("webdav_agent socket connect failed, error: %d \n", errno);
    return errno;
  }
  
  size_t count = sendmsg(socket_fd, &msg, 0);
  if (count < 0) {
    printf("webdav_agent socket send message failed, error: %d \n", errno);
    return errno;
  }
  
  bzero(&msg, sizeof(msg));
  bzero(&iov, sizeof(iov));
  msg.msg_iov = iov;
  
  int remote_error;
  iov[0].iov_len = sizeof(remote_error);
  iov[0].iov_base = (caddr_t)&remote_error;
  msg.msg_iovlen = 1;
  
  if (reply_len > 0) {
    iov[1].iov_len = reply_len;
    iov[1].iov_base = (caddr_t)reply;
    msg.msg_iovlen = 2;
  }
  
  count = recvmsg(socket_fd, &msg, 0);
  if (result < 0) {
    printf("webdav_agent socket receive message failed, error: %d \n", errno);
    return errno;
  }
  
  result = close(socket_fd);
  if (result != 0) {
    printf("webdav_agent socket close error: %d \n", errno);
  }
  
  return remote_error;
}

int handle_lookup(void *ctx, struct webdav_request_lookup* request, struct webdav_reply_lookup* reply) {
  printf("[LOOKUP] \n");
  printf("=> dir_id: %d\n", request->dir_id);
  printf("=> force_lookup: %d\n", request->force_lookup);
  printf("=> name_length: %d\n", request->name_length);
  printf("=> name: %s\n", request->name);
  
  int result = round_trip(
                          ctx,
                          WEBDAV_LOOKUP,
                          (union webdav_request *) request,
                          offsetof(struct webdav_request_lookup, name),
                          request->name,
                          request->name_length,
                          (void *) reply,
                          sizeof(struct webdav_reply_lookup)
                          );
  
  printf("<= obj_id: %d\n", reply->obj_id);
  printf("<= obj_fileid: %d\n", reply->obj_fileid);
  printf("<= obj_type: %d\n", reply->obj_type);
  printf("<= obj_atime.tv_sec: %lld\n", reply->obj_atime.tv_sec);
  printf("<= obj_atime.tv_nsec: %lld\n", reply->obj_atime.tv_nsec);
  printf("<= obj_mtime.tv_sec: %lld\n", reply->obj_mtime.tv_sec);
  printf("<= obj_mtime.tv_nsec: %lld\n", reply->obj_mtime.tv_nsec);
  printf("<= obj_ctime.tv_sec: %lld\n", reply->obj_ctime.tv_sec);
  printf("<= obj_ctime.tv_nsec: %lld\n", reply->obj_ctime.tv_nsec);
  printf("<= obj_createtime.tv_sec: %lld\n", reply->obj_createtime.tv_sec);
  printf("<= obj_createtime.tv_nsec: %lld\n", reply->obj_createtime.tv_nsec);
  printf("<= obj_filesize: %lld\n", reply->obj_filesize);
  
  return result;
}

int handle_create(void *ctx, struct webdav_request_create *request, struct webdav_reply_create *reply) {
  printf("[CREATE] \n");
  printf("=> dir_id: %d\n", request->dir_id);
  printf("=> mode: %d\n", request->mode);
  printf("=> name_length: %d\n", request->name_length);
  printf("=> name: %s\n", request->name);
  
  int result = round_trip(
                          ctx,
                          WEBDAV_CREATE,
                          (union webdav_request *) request,
                          offsetof(struct webdav_request_create, name),
                          request->name,
                          request->name_length,
                          (void *) reply,
                          sizeof(struct webdav_reply_create)
                          );
  
  printf("<= obj_id: %d\n", reply->obj_id);
  printf("<= obj_fileid: %d\n", reply->obj_fileid);
  
  return result;
}

int handle_open(void *ctx, struct webdav_request_open* request, struct webdav_reply_open* reply) {
  printf("[OPEN] \n");
  printf("=> obj_id: %d\n", request->obj_id);
  printf("=> flags: %d\n", request->flags);
  printf("=> ref: %d\n", request->ref);
  
  int result = round_trip(
                          ctx,
                          WEBDAV_OPEN,
                          (union webdav_request *) request,
                          sizeof(struct webdav_request_open),
                          NULL,
                          0,
                          (void *) reply,
                          sizeof(struct webdav_reply_open)
                          );
  
  printf("<= pid: %d\n", reply->pid);
  
  return result;
}

int handle_close(void *ctx, struct webdav_request_close* request) {
  printf("[CLOSE] \n");
  printf("=> obj_id: %d\n", request->obj_id);
  
  return round_trip(
                    ctx,
                    WEBDAV_CLOSE,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_close),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

void print_webdav_stat(char* direction, char* prefix, struct webdav_stat* stat) {
  printf("%s %s.st_dev: %d\n", direction, prefix, stat->st_dev);
  printf("%s %s.st_ino: %d\n", direction, prefix, stat->st_ino);
  printf("%s %s.st_mode: %d\n", direction, prefix, stat->st_mode);
  printf("%s %s.st_nlink: %d\n", direction, prefix, stat->st_nlink);
  printf("%s %s.st_uid: %d\n", direction, prefix, stat->st_uid);
  printf("%s %s.st_gid: %d\n", direction, prefix, stat->st_gid);
  printf("%s %s.st_rdev: %d\n", direction, prefix, stat->st_rdev);
  printf("%s %s.st_atimespec.tv_sec: %lld\n", direction, prefix, stat->st_atimespec.tv_sec);
  printf("%s %s.st_atimespec.tv_nsec: %lld\n", direction, prefix, stat->st_atimespec.tv_nsec);
  printf("%s %s.st_mtimespec.tv_sec: %lld\n", direction, prefix, stat->st_mtimespec.tv_sec);
  printf("%s %s.st_mtimespec.tv_nsec: %lld\n", direction, prefix, stat->st_mtimespec.tv_nsec);
  printf("%s %s.st_ctimespec.tv_sec: %lld\n", direction, prefix, stat->st_ctimespec.tv_sec);
  printf("%s %s.st_ctimespec.tv_nsec: %lld\n", direction, prefix, stat->st_ctimespec.tv_nsec);
  printf("%s %s.st_createtimespec.tv_sec: %lld\n", direction, prefix, stat->st_createtimespec.tv_sec);
  printf("%s %s.st_createtimespec.tv_nsec: %lld\n", direction, prefix, stat->st_createtimespec.tv_nsec);
  printf("%s %s.st_size: %lld\n", direction, prefix, stat->st_size);
  printf("%s %s.st_blocks: %lld\n", direction, prefix, stat->st_blocks);
  printf("%s %s.st_blksize: %d\n", direction, prefix, stat->st_blksize);
  printf("%s %s.st_flags: %d\n", direction, prefix, stat->st_flags);
  printf("%s %s.st_gen: %d\n", direction, prefix, stat->st_gen);
}

int handle_getattr(void *ctx, struct webdav_request_getattr* request, struct webdav_reply_getattr* reply) {
  printf("[GETATTR]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  
  int error = round_trip(
                         ctx,
                         WEBDAV_GETATTR,
                         (union webdav_request *) request,
                         sizeof(struct webdav_request_getattr),
                         NULL,
                         0,
                         (void *) reply,
                         sizeof(struct webdav_reply_getattr)
                         );
  
  print_webdav_stat("<=", "obj_attr", &reply->obj_attr);
  
  return error;
}

void print_stat(char* direction, char* prefix, struct stat* stat) {
  printf("%s %s.st_dev: %d\n", direction, prefix, stat->st_dev);
  printf("%s %s.st_ino: %lld\n", direction, prefix, stat->st_ino);
  printf("%s %s.st_mode: %d\n", direction, prefix, stat->st_mode);
  printf("%s %s.st_nlink: %d\n", direction, prefix, stat->st_nlink);
  printf("%s %s.st_uid: %d\n", direction, prefix, stat->st_uid);
  printf("%s %s.st_gid: %d\n", direction, prefix, stat->st_gid);
  printf("%s %s.st_rdev: %d\n", direction, prefix, stat->st_rdev);
  printf("%s %s.st_atimespec.tv_sec: %ld\n", direction, prefix, stat->st_atimespec.tv_sec);
  printf("%s %s.st_atimespec.tv_nsec: %ld\n", direction, prefix, stat->st_atimespec.tv_nsec);
  printf("%s %s.st_mtimespec.tv_sec: %ld\n", direction, prefix, stat->st_mtimespec.tv_sec);
  printf("%s %s.st_mtimespec.tv_nsec: %ld\n", direction, prefix, stat->st_mtimespec.tv_nsec);
  printf("%s %s.st_ctimespec.tv_sec: %ld\n", direction, prefix, stat->st_ctimespec.tv_sec);
  printf("%s %s.st_ctimespec.tv_nsec: %ld\n", direction, prefix, stat->st_ctimespec.tv_nsec);
  printf("%s %s.st_birthtimespec.tv_sec: %ld\n", direction, prefix, stat->st_birthtimespec.tv_sec);
  printf("%s %s.st_birthtimespec.tv_nsec: %ld\n", direction, prefix, stat->st_birthtimespec.tv_nsec);
  printf("%s %s.st_size: %lld\n", direction, prefix, stat->st_size);
  printf("%s %s.st_blocks: %lld\n", direction, prefix, stat->st_blocks);
  printf("%s %s.st_blksize: %d\n", direction, prefix, stat->st_blksize);
  printf("%s %s.st_flags: %d\n", direction, prefix, stat->st_flags);
  printf("%s %s.st_gen: %d\n", direction, prefix, stat->st_gen);
}

int handle_setattr(void *ctx, struct webdav_request_setattr* request) {
  printf("[SETATTR]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  print_stat("=>", "new_obj_attr", &request->new_obj_attr);
  
  return round_trip(
                    ctx,
                    WEBDAV_SETATTR,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_setattr),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

int handle_read(void *ctx, struct webdav_request_read* request, char** data, size_t* data_len) {
  printf("[READ]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  printf("=> offset: %lld\n", request->offset);
  printf("=> count: %lld\n", request->count);
  
  *data_len = request->count;
  *data = malloc(*data_len);
  
  int result = round_trip(
                          ctx,
                          WEBDAV_READ,
                          (union webdav_request *) request,
                          sizeof(struct webdav_request_read),
                          NULL,
                          0,
                          data,
                          *data_len
                          );
  
  
  printf("<= data_len: %zuud\n", *data_len);
  printf("<= data: %s\n", *data); //TODO hex
  
  return result;
}

int handle_writeseq(void *ctx, struct webdav_request_writeseq* request, struct webdav_reply_writeseq* reply) {
  printf("[WRITESEQ]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  printf("=> offset: %lld\n", request->offset);
  printf("=> count: %lld\n", request->count);
  printf("=> file_len: %lld\n", request->file_len);
  printf("=> is_retry: %d\n", request->is_retry);
  
  int result = round_trip(
                          ctx,
                          WEBDAV_WRITESEQ,
                          (union webdav_request *) request,
                          sizeof(struct webdav_request_writeseq),
                          NULL,
                          0,
                          (void *) reply,
                          sizeof(struct webdav_reply_writeseq)
                          );
  
  printf("<= count: %lld\n", reply->count);
  
  return result;
}

int handle_fsync(void *ctx, struct webdav_request_fsync* request) {
  printf("[FSYNC]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  
  return round_trip(
                    ctx,
                    WEBDAV_FSYNC,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_fsync),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

int handle_remove(void *ctx, struct webdav_request_remove* request) {
  printf("[REMOVE]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  
  return round_trip(
                    ctx,
                    WEBDAV_REMOVE,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_remove),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

int handle_rename(void *ctx, struct webdav_request_rename* request) {
  printf("[RENAME]\n");
  printf("=> from_dir_id: %d\n", request->from_dir_id);
  printf("=> from_obj_id: %d\n", request->from_obj_id);
  printf("=> to_dir_id: %d\n", request->to_dir_id);
  printf("=> to_obj_id: %d\n", request->to_obj_id);
  printf("=> to_name_length: %d\n", request->to_name_length);
  printf("=> to_name: %s\n", request->to_name);
  
  return round_trip(
                    ctx,
                    WEBDAV_RENAME,
                    (union webdav_request *) request,
                    offsetof(struct webdav_request_rename, to_name),
                    request->to_name,
                    request->to_name_length,
                    NULL,
                    0
                    );
}

int handle_mkdir(void *ctx, struct webdav_request_mkdir* request, struct webdav_reply_mkdir* reply) {
  printf("[MKDIR]\n");
  printf("=> dir_id: %d\n", request->dir_id);
  printf("=> mode: %d\n", request->mode);
  printf("=> name_length: %d\n", request->name_length);
  printf("=> name: %s\n", request->name);
  
  int result = round_trip(
                          ctx,
                          WEBDAV_MKDIR,
                          (union webdav_request *) request,
                          offsetof(struct webdav_request_mkdir, name),
                          request->name,
                          request->name_length,
                          (void *) reply,
                          sizeof(struct webdav_reply_mkdir)
                          );
  
  printf("<= obj_id: %d\n", reply->obj_id);
  printf("<= obj_fileid: %d\n", reply->obj_fileid);
  
  return result;
}

int handle_rmdir(void *ctx, struct webdav_request_rmdir* request) {
  printf("[RMDIR]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  
  return round_trip(
                    ctx,
                    WEBDAV_RMDIR,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_rmdir),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

int handle_readdir(void *ctx, struct webdav_request_readdir* request) {
  printf("[READDIR]\n");
  printf("=> obj_id: %d\n", request->obj_id);
  printf("=> cache: %d\n", request->cache);
  
  return round_trip(
                    ctx,
                    WEBDAV_READDIR,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_readdir),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

void print_webdav_statfs(char* direction, char* prefix, struct webdav_statfs* stat) {
  printf("%s %s.f_bsize: %lld\n", direction, prefix, stat->f_bsize);
  printf("%s %s.f_iosize: %lld\n", direction, prefix, stat->f_iosize);
  printf("%s %s.f_blocks: %lld\n", direction, prefix, stat->f_blocks);
  printf("%s %s.f_bfree: %lld\n", direction, prefix, stat->f_bfree);
  printf("%s %s.f_bavail: %lld\n", direction, prefix, stat->f_bavail);
  printf("%s %s.f_files: %lld\n", direction, prefix, stat->f_files);
  printf("%s %s.f_ffree: %lld\n", direction, prefix, stat->f_ffree);
}

int handle_statfs(void *ctx, struct webdav_request_statfs* request, struct webdav_reply_statfs* reply) {
  printf("[STATFS]\n");
  printf("=> root_obj_id: %d\n", request->root_obj_id);
  
  int result = round_trip(
                          ctx,
                          WEBDAV_STATFS,
                          (union webdav_request *) request,
                          sizeof(struct webdav_request_statfs),
                          NULL,
                          0,
                          (void *) reply,
                          sizeof(struct webdav_reply_statfs)
                          );
  
  print_webdav_statfs("<=", "fs_attr", &reply->fs_attr);
  
  return result;
}

int handle_unmount(void *ctx, struct webdav_request_unmount* request) {
  pthread_exit(NULL);
  return round_trip(
                    ctx,
                    WEBDAV_UNMOUNT,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_unmount),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

int handle_invalcaches(void *ctx, struct webdav_request_invalcaches* request) {
  return round_trip(
                    ctx,
                    WEBDAV_INVALCACHES,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_invalcaches),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

int handle_dump_cookies(void *ctx, struct webdav_request_cookies* request) {
  return round_trip(
                    ctx,
                    WEBDAV_DUMP_COOKIES,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_cookies),
                    NULL,
                    0,
                    NULL,
                    0
                    );
}

int handle_clear_cookies(void *ctx, struct webdav_request_cookies* request) {
  return round_trip(
                    ctx,
                    WEBDAV_CLEAR_COOKIES,
                    (union webdav_request *) request,
                    sizeof(struct webdav_request_cookies),
                    NULL,
                    0,
                    NULL,
                    0
                    );
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

struct listen_thread_args {
  struct handler_ctx handler_ctx;
  struct sockaddr_un listen_addr;
};

void* listen_thread(void *argp) {
  struct listen_thread_args* args = (struct listen_thread_args*)argp;
  
  printf("start listening on %s \n", args->listen_addr.sun_path);
  
  int result = webdav_listen(&args->listen_addr, handler, &args->handler_ctx);
  if (result != 0) {
    printf("webdav listen failed, error: %d \n", result);
  }
  
  return NULL;
}

int main(int argc, char** argv) {
  if (argc != 5) {
    printf("Usage: inspector <path to webdav socket> <mount name> <vol name> <mnt dir> \n");
    return EINVAL;
  }
  
  struct listen_thread_args args;
  bzero(&args, sizeof(args));
  
  strlcpy(args.handler_ctx.un.sun_path, argv[1], sizeof(args.handler_ctx.un.sun_path));
  args.handler_ctx.un.sun_len = sizeof(args.handler_ctx.un);
  args.handler_ctx.un.sun_family = PF_LOCAL;
  
  strlcpy(args.listen_addr.sun_path, SOCKET_PATH, sizeof(args.listen_addr.sun_path));
  if (mktemp(args.listen_addr.sun_path) == NULL) {
    printf("cannot create temporary socket, error: %d \n", errno);
    return errno;
  }
  args.listen_addr.sun_len = sizeof(args.listen_addr);
  args.listen_addr.sun_family = PF_LOCAL;
  
  char mnt_name[NAME_MAX];
  bzero(mnt_name, sizeof(mnt_name));
  strlcpy(mnt_name, argv[2], sizeof(mnt_name));
  
  char vol_name[NAME_MAX];
  bzero(vol_name, sizeof(vol_name));
  strlcpy(vol_name, argv[3], sizeof(vol_name));
  
  char mnt_dir[PATH_MAX];
  bzero(mnt_dir, sizeof(mnt_dir));
  strlcpy(mnt_dir, argv[4], sizeof(mnt_dir));
  
  printf("staring inspector %s <-> %s \n", args.handler_ctx.un.sun_path, args.listen_addr.sun_path);
  
  pthread_t listen_thread_id;
  
  int result;
  if (pthread_create(&listen_thread_id, NULL, listen_thread, &args) != 0) {
    printf("failed to start listen thread, error: %d \n", errno);
    result = errno;
    goto done;
  }
  
  result = webdav_mount(&args.listen_addr, mnt_name, vol_name, mnt_dir);
  if (result != 0) {
    printf("webdav mount error, %d \n", result);
    goto done;
  }
  
  if (pthread_join(listen_thread_id, NULL) != 0) {
    printf("failed to wait for listen thread, error: %d \n", errno);
    result = errno;
    goto done;
  }
  
done:
  if (remove(args.listen_addr.sun_path) != 0) {
    printf("cannot remove temporary uds socket file %s, error: %d \n", args.listen_addr.sun_path, errno);
  }
  
  return result;
}
