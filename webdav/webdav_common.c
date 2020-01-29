//
//  webdav_common.c
//  pwndav
//
//  Created by Chakra on 27/01/20.
//  Copyright © 2020 Sreejith Krishnan R. All rights reserved.
//

#include <stdio.h>
#include <pthread.h>

#include <sys/mount.h>
#include <sys/un.h>
#include <IOKit/kext/KextManager.h>

#include "webdav_common.h"

int webdav_kext_read(int socket, int* operation, void* request, size_t request_size) {
  if (operation == NULL || request == NULL) {
    printf("invalid args operation / request \n");
    return EINVAL;
  }
  
  struct iovec iov[2];
  bzero(iov, sizeof(iov));
  
  iov[0].iov_base = (caddr_t)operation;
  iov[0].iov_len = sizeof(int);
  
  iov[1].iov_base = request;
  iov[1].iov_len = request_size;
  
  struct msghdr msg;
  bzero(&msg, sizeof(msg));
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  
  size_t n = recvmsg(socket, &msg, 0);
  if (n < sizeof(int) + sizeof(struct webdav_cred)) {
    printf("message length %zu too small \n", n);
    return EINVAL;
  }
  
  return 0;
}

int webdav_kext_write(int error, union webdav_reply* reply, size_t reply_len, char* data, size_t data_len, int socket) {
  if (reply == NULL) {
    printf("invalid argument reply \n");
    return EINVAL;
  }
  
  struct msghdr msg;
  struct iovec iov[2];
  
  bzero(&msg, sizeof(msg));
  bzero(iov, sizeof(iov));
  msg.msg_iov = iov;
  
  iov[0].iov_base = (caddr_t)&error;
  iov[0].iov_len = sizeof(error);
  msg.msg_iovlen = 1;
  
  if (reply_len > 0 && data_len > 0) {
    printf("both reply and data is present \n");
    return EINVAL;
  }
  
  if (reply_len > 0) {
    iov[1].iov_base = (caddr_t)reply;
    iov[1].iov_len = reply_len;
    msg.msg_iovlen = 2;
  }
  
  if (data_len > 0) {
    iov[1].iov_base = (caddr_t)data;
    iov[1].iov_len = data_len;
    msg.msg_iovlen = 2;
  }
  
  size_t count = sendmsg(socket, &msg, 0);
  if (count < 0) {
    printf("socket send error %d \n", errno);
    return errno;
  }
  
  return 0;
}

int webdav_kext_route(
                      struct webdav_kext_handler* handler,
                      int operation,
                      union webdav_request* request,
                      union webdav_reply* reply,
                      size_t* reply_len,
                      char** data,
                      size_t* data_len
                      ) {
  int error;
  
  if (handler == NULL) {
    printf("handler is null \n");
    error = EINVAL;
    goto done;
  }
  
  if (request == NULL || reply == NULL || reply_len == NULL) {
    printf("request / reply / reply len is null \n");
    error = EINVAL;
    goto done;
  }
  
  char* op_str;
  switch (operation) {
    case WEBDAV_LOOKUP:
      error = (handler->handle_lookup)(handler->ctx, (struct webdav_request_lookup *) request, (struct webdav_reply_lookup *) reply);
      *reply_len = sizeof(struct webdav_reply_lookup);
      op_str = "LOOKUP";
      break;
    case WEBDAV_CREATE:
      error = (handler->handle_create)(handler->ctx, (struct webdav_request_create *) request, (struct webdav_reply_create *) reply);
      *reply_len = sizeof(struct webdav_reply_create);
      op_str = "CREATE";
      break;
    case WEBDAV_OPEN:
      error = (handler->handle_open)(handler->ctx, (struct webdav_request_open *) request, (struct webdav_reply_open *) reply);
      *reply_len = sizeof(struct webdav_reply_open);
      op_str = "OPEN";
      break;
    case WEBDAV_CLOSE:
      error = (handler->handle_close)(handler->ctx, (struct webdav_request_close *) request);
      *reply_len = 0;
      op_str = "CLOSE";
      break;
    case WEBDAV_GETATTR:
      error = (handler->handle_getattr)(handler->ctx, (struct webdav_request_getattr *) request, (struct webdav_reply_getattr *) reply);
      *reply_len = sizeof(struct webdav_reply_getattr);
      op_str = "GETATTR";
      break;
    case WEBDAV_SETATTR:
      error = (handler->handle_setattr)(handler->ctx, (struct webdav_request_setattr *) request);
      *reply_len = 0;
      op_str = "SETATTR";
      break;
    case WEBDAV_READ:
      error = (handler->handle_read)(handler->ctx, (struct webdav_request_read *) request, data, data_len);
      *reply_len = 0;
      op_str = "READ";
      break;
    case WEBDAV_WRITESEQ:
      error = (handler->handle_write_seq)(handler->ctx, (struct webdav_request_writeseq *) request, (struct webdav_reply_writeseq *) reply);
      *reply_len = sizeof(struct webdav_reply_write);
      op_str = "WRITE";
      break;
    case WEBDAV_FSYNC:
      error = (handler->handle_fsync)(handler->ctx, (struct webdav_request_fsync *) request);
      *reply_len = 0;
      op_str = "FSYNC";
      break;
    case WEBDAV_REMOVE:
      error = (handler->handle_remove)(handler->ctx, (struct webdav_request_remove *) request);
      *reply_len = 0;
      op_str = "REMOVE";
      break;
    case WEBDAV_RENAME:
      error = (handler->handle_rename)(handler->ctx, (struct webdav_request_rename *) request);
      *reply_len = 0;
      op_str = "RENAME";
      break;
    case WEBDAV_MKDIR:
      error = (handler->handle_mkdir)(handler->ctx, (struct webdav_request_mkdir *) request, (struct webdav_reply_mkdir *) reply);
      *reply_len = sizeof(struct webdav_reply_mkdir);
      op_str = "MKDIR";
      break;
    case WEBDAV_RMDIR:
      error = (handler->handle_rmdir)(handler->ctx, (struct webdav_request_rmdir *) request);
      *reply_len = 0;
      op_str = "RMDIR";
      break;
    case WEBDAV_READDIR:
      error = (handler->handle_readdir)(handler->ctx, (struct webdav_request_readdir *) request);
      *reply_len = 0;
      op_str = "READDIR";
      break;
    case WEBDAV_STATFS:
      error = (handler->handle_statfs)(handler->ctx, (struct webdav_request_statfs *) request, (struct webdav_reply_statfs *) reply);
      *reply_len = sizeof(struct webdav_reply_statfs);
      op_str = "STATFS";
      break;
    case WEBDAV_UNMOUNT:
      error = (handler->handle_unmount)(handler->ctx, (struct webdav_request_unmount *) request);
      *reply_len = 0;
      op_str = "UNMOUNT";
      break;
    case WEBDAV_INVALCACHES:
      error = (handler->handle_invalcaches)(handler->ctx, (struct webdav_request_invalcaches *) request);
      *reply_len = 0;
      op_str = "INVALCACHES";
      break;
    case WEBDAV_DUMP_COOKIES:
      error = (handler->handle_dump_cookies)(handler->ctx, (struct webdav_request_cookies* )request);
      *reply_len = 0;
      op_str = "DUMP_COOKIES";
      break;
    case WEBDAV_CLEAR_COOKIES:
      error = (handler->handle_clear_cookies)(handler->ctx, (struct webdav_request_cookies* )request);
      *reply_len = 0;
      op_str = "CLEAR_COOKIES";
      break;
    default:
      printf("unknown operation %d \n", operation);
      error = EINVAL;
      *reply_len = 0;
      op_str = "UNKNOWN";
      break;
  }
  
  if (error != 0) {
    printf("ERROR[%s]: %d \n", op_str,  error);
  }
  
done:
  return error;
}

int webdav_kext_handle(struct webdav_kext_handler* router, int socket) {
  int error = 0;
  int operation = -1;
  
  // Some request have variable length data at end
  char request[NAME_MAX + 1 + sizeof(union webdav_request)];
  bzero(request, sizeof(request));
  
  error = webdav_kext_read(socket, &operation, request, sizeof(request));
  if (error != 0) {
    printf("error reading kext request: %d \n", error);
    goto done;
  }
  
  union webdav_reply reply;
  bzero(&reply, sizeof(reply));
  size_t reply_len = 0;
  
  char* data;
  size_t data_len = 0;
  int result = webdav_kext_route(
                                 router, operation, (union webdav_request*)request, &reply, &reply_len, &data, &data_len);
  
  error = webdav_kext_write(result, &reply, reply_len, data, data_len, socket);
  
  if (data != NULL) {
    free(data);
  }
  
  if (error != 0) {
    printf("error writing kext request: %d \n", error);
    goto done;
  }
  
  /*
   If the operation was unmount and operation completed without error,
   then exit listener thread
   */
  if (operation == WEBDAV_UNMOUNT) {
    pthread_exit(NULL);
  }
  
done:
  return error;
}

int webdav_mount(struct sockaddr_un* un, char* mnt_name, char* vol_name, char* mnt_dir) {
  struct vfsconf vfsconf;
  bzero(&vfsconf, sizeof(vfsconf));
  
  if (getvfsbyname("webdav", &vfsconf) != 0) {
    OSReturn result = KextManagerLoadKextWithIdentifier(CFSTR("com.apple.filesystems.webdav"), NULL);
    if (result != KERN_SUCCESS) {
      printf("cannot load kext, err %d", result);
      return 1;
    }
    
    getvfsbyname("webdav", &vfsconf);
  }
  
  struct webdav_args args;
  bzero(&args, sizeof(args));
  
  args.pa_mntfromname = mnt_name;
  args.pa_version = kCurrentWebdavArgsVersion;
  args.pa_socket_name = (struct sockaddr *)un;
  args.pa_socket_namelen = (int)sizeof(struct sockaddr_un);
  args.pa_vol_name = vol_name;
  args.pa_flags = 0;
  args.pa_server_ident = kServerIdent;
  args.pa_root_id = CreateOpaqueID(1, 1);
  args.pa_root_fileid = WEBDAV_ROOTFILEID;
  args.pa_uid = geteuid();
  args.pa_gid = getegid();
  args.pa_dir_size = WEBDAV_DIR_SIZE;
  args.pa_link_max = 1;
  args.pa_name_max = NAME_MAX;
  args.pa_path_max = PATH_MAX;
  args.pa_pipe_buf = -1;
  args.pa_chown_restricted = (int)_POSIX_CHOWN_RESTRICTED;
  args.pa_no_trunc = (int)_POSIX_NO_TRUNC;
  
  int result = mount(vfsconf.vfc_name, mnt_dir, MNT_ASYNC | MNT_LOCAL | MNT_UNION, &args);
  if (result != 0) {
    printf("mount error: %d \n", errno);
    return errno;
  }
  
  return 0;
}

int webdav_listen(struct sockaddr_un* un, int (*handler)(void*, int), void* ctx) {
  if (handler == NULL || un == NULL) {
    printf("invalid webdav_listen args \n");
    return EINVAL;
  }
  
  int listen_socket = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (listen_socket < 0) {
    printf("cannot create listen socket: %d", errno);
    return errno;
  }
  
  if (bind(listen_socket, (struct sockaddr*)un, sizeof(struct sockaddr_un)) != 0) {
    printf("cannot bind to listen socket: %d \n", errno);
    return errno;
  }
  
  if (listen(listen_socket, 10) != 0) {
    printf("cannot listen to listen socket: %d \n", errno);
    return errno;
  }
  
  printf("started listening for webdav kext requests \n");
  
  int error = 0;
  while (TRUE) {
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    
    socklen_t addrlen = sizeof(addr);
    
    int accept_socket = accept(listen_socket, (struct sockaddr *)&addr, &addrlen);
    if (accept_socket < 0) {
      printf("socket receive error %d \n", errno);
      continue;
    }
    
    error = (handler)(ctx, accept_socket);
    if (error != 0) {
      printf("socket handle error %d \n", error);
      break;
    }
    
    close(accept_socket);
  }
  
  return error;
}
