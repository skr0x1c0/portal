//
//  webdav_common.h
//  pwndav
//
//  Created by Chakra on 27/01/20.
//  Copyright © 2020 Sreejith Krishnan R. All rights reserved.
//

#ifndef webdav_common_h
#define webdav_common_h

#include "webdav.h"

typedef struct webdav_kext_handler {
  void *ctx;
  int (*handle_lookup)(void*, struct webdav_request_lookup*, struct webdav_reply_lookup*);
  int (*handle_create)(void*, struct webdav_request_create*, struct webdav_reply_create*);
  int (*handle_open)(void*, struct webdav_request_open*, struct webdav_reply_open*);
  int (*handle_close)(void*, struct webdav_request_close*);
  int (*handle_getattr)(void*, struct webdav_request_getattr*, struct webdav_reply_getattr*);
  int (*handle_setattr)(void*, struct webdav_request_setattr*);
  int (*handle_read)(void*, struct webdav_request_read*, char** data, size_t* data_size);
  int (*handle_write_seq)(void*, struct webdav_request_writeseq*, struct webdav_reply_writeseq*);
  int (*handle_fsync)(void*, struct webdav_request_fsync*);
  int (*handle_remove)(void*, struct webdav_request_remove*);
  int (*handle_rename)(void*, struct webdav_request_rename*);
  int (*handle_mkdir)(void*, struct webdav_request_mkdir*, struct webdav_reply_mkdir*);
  int (*handle_rmdir)(void*, struct webdav_request_rmdir*);
  int (*handle_readdir)(void*, struct webdav_request_readdir*);
  int (*handle_statfs)(void*, struct webdav_request_statfs*, struct webdav_reply_statfs*);
  int (*handle_unmount)(void*, struct webdav_request_unmount*);
  int (*handle_invalcaches)(void*, struct webdav_request_invalcaches*);
  int (*handle_dump_cookies)(void*, struct webdav_request_cookies*);
  int (*handle_clear_cookies)(void*, struct webdav_request_cookies*);
} webdav_kext_handler_t;

int webdav_kext_handle(struct webdav_kext_handler* router, int socket);
int webdav_listen(struct sockaddr_un* un, int (*handler)(void*, int), void* ctx);
int webdav_mount(struct sockaddr_un* un, char* mnt_name, char* vol_name, char* mnt_dir);

#endif /* webdav_common_h */
