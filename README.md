# Title

A malicious application running in standard user context can bypass read only or write only restriction enforced on a file by file system

# Summary

WebDAV kernal extension in MacOS accepts file descriptors to cache files without validating for read-write access when handling sysctl request of type `WEBDAV_ASSOCIATECACHEFILE_SYSCTL`

# Impact

# Steps to reproduce

The following steps will demonstrate writing on a protected file `secure.txt` by an unauthorized user

touch hello

1. ```bash
   touch ~/secure.txt
   echo 'Do not tamper' > ~/secure.txt
   ```

2. Set the owner of `secure.txt` to *root*. Set the file permission such that only root has write permission. Others users can only read the file
   
   ```bash
   sudo chown root ~/secure.txt
   sudo chmod 644 ~/secure.txt
   ```

3. Verify current user cannot write to `secure.txt`
   
   ```bash
   touch ~/secure.txt
   ```
   
   Running above command will fail with error `Permission denied`

4. Create a file named `payload.txt` in home directory with content `Tampered!`
   
   ```bash
   touch ~/payload.txt
   echo 'Tampered!' > ~/payload.txt
   ```

5. Overwrite contents of `secure.txt` with contents of `payload.txt` by running portal. Assuming you downloaded `portal` executable from mail attachment to current directory
   
   ```bash
   ./portal write ~/secure.txt ~/payload.txt
   ```

6. Verify
   
   ```bash
   cat ~/secure.txt
   ```
   
   Running above command will output `Tampered!`

# Working principle

WebDAV filesystem for MacOS have two main components - WebDAV agent and WebDAV kernel extension (kext). WebDAV agent is a process running in user space. It is responsible for connecting with and processing response from WebDAV server. WebDAV kext is a kernel extension which implements the virtual file system interface and connects with WebDAV agent via unix domain socket for performing file system operations. The isolation between kext and agent process reduces the impact of bugs susceptible to malicious responses from server.

For each vnode in WebDAV filesystem, there is a cache file associated with it. For IO operations (including read and write), instead of sending data over unix domain socket (UDS) the data is written to a cache file on sender side and the receiver side reads the data from cache file. The cache file is created by the agent process. The agent process opens the associated cache file and sends allocated file descriptor to kext. In kext side, there is no validation to ensure that the received file descriptor is authorized for read and write operations. The kext calls `file_vnode_withvid` function to obtain `vnode` pointer associated to cache file. Read and write IO operations on cache file uses  `VNOP_READ` and `VNOP_WRITE` functions respectively. Hence even if the file descriptor send by agent is not authorized for write operation, kext can successfully perform write operations on the cache file. Any read / write operation on node will happen through the associated to cache file. Hence by creating a fake WebDAV agent which responds to request from kext, a malicious application running in standard user context can perform IO operations on a file even if the filesystem permissions does not allow that operation.

Consider a WebDAV mount at `/Volumes/localhost`. Let the mount contains a file `portal` at root directory. When a process opens and writes to the file `portal`, the simplified communication between kext and agent process is as follows (only communication relevant to this exploit is mentioned for simplicity)

1. Kext sends a request with agent process with following data
   
   ```c
   struct webdav_request_open
   {
     /* user and groups */
     struct webdav_cred pcr;
     /* opaque_id of object */
     opaque_id obj_id;
     /* file access flags (O_RDONLY, O_WRONLY, etc.) */
     int flags;
     /* the reference to the webdav object that the cache object should be associated with */
     int ref;
   };
   ```
   
   Fields relevant to exploit in above request are `obj_id` and `ref`. The `obj_id` is a unique id assigned to each node (file / directory) in WebDAV mount. The `obj_id` is used by agent to obtain data structure assigned to node

2. The agent process will use `obj_id` to get reference to `node_entry` data structure associated with the file. The `node_entry` data structure holds important data related to node. 
   
   ```c
   struct node_entry
   {
   /* the utf8 name */
   char *name;
   ...
   /* the cache file's file descriptor or -1 if none */
   int file_fd;
   ...
   };
   ```
   
   The field we are interested in is `file_fd` which contains the file descriptor of cache file associated with the node. If at the time of open, the node does not have a associated cache file, a new cache file is created in `/tmp` directory.  The agent the calls the `sysctl` syscall as follows
   
   ```c
   int mib[5];
   	
   /* setup mib for the request */
   mib[0] = CTL_VFS;
   mib[1] = g_vfc_typenum;
   mib[2] = WEBDAV_ASSOCIATECACHEFILE_SYSCTL;
   
   /* reference id to the webdav object */
   /* obtained from kext request webdav_request_open */
   mib[3] = ref;
   /* cache file's file descriptor */
   mib[4] = fd;
   
   sysctl(mib, 5, NULL, NULL, NULL, 0)
   ```

3. This syscall will trigger `webdav_sysctl` function in `webdav_vfsops.c`
   
   ```c
   static int webdav_sysctl(int *name, u_int namelen, user_addr_t oldp, size_t *oldlenp,
   	user_addr_t newp, size_t newlen, vfs_context_t context)
   {
     switch ( name[0] )
     {
       case WEBDAV_ASSOCIATECACHEFILE_SYSCTL:
         {
           int ref;
           int fd;
           struct open_associatecachefile *associatecachefile;
           vnode_t vp;   
           
           /* Use ref to get reference to associatecachefile */
           webdav_translate_ref(ref, &associatecachefile);     
   
           ref = name[1];
           fd = name[2];
           
           file_vnode_withvid(fd, &vp, NULL);
           
           /* store the cache file's vnode in the webdavnode */
   	    associatecachefile->cachevp = vp;
   	    
           /* store the PID of the process that called us */
           /* for validation in webdav_open */
   	    associatecachefile->pid = vfs_context_pid(context);
           ...
         }
       ...
     }
   }
   ```
   
   **Note:** There is no validation to ensure that the received file descriptor `fd` is authorized for read and write operations. 

4. The agent will the send response to kext with following data
   
   ```c
   struct webdav_reply_open
   {
     /* process ID of file system daemon (for matching to ref's pid) */
     pid_t pid;
   };
   ```

5. Kext will read the response from agent and save the associated cache file `vnode` pointer to `webdav_vnode` data associated to the node
   
   ```c
   static int webdav_vnop_open_locked(struct vnop_open_args *ap)
   {
     struct webdavnode *pt;
     vnode_t vp;
     vp = ap->a_vp;
     pt = VTOWEBDAV(vp);
    
     // For simplicity, request sending and 
     // related instructions not shown
     ...
     
     if (reply_open.pid != associatecachefile.pid)
     {
       error = EPERM;
       goto dealloc_done;
     }
     
     pt->pt_cache_vnode = associatecachefile.cachevp;
     
     ...
   }
   ```

6. When data is written to a node in WebDAV mount, the kext will write the data to cache node first
   
   ```c
   static int webdav_vnop_write(struct vnop_write_args *ap)
   {
     struct webdavnode *pt;
     pt = VTOWEBDAV(ap->a_vp);	
     vp = ap->a_vp;
     
     ...
   
     cachevp = pt->pt_cache_vnode;
     in_uio = ap->a_uio;
     
     /* Write data to cache file first */
     VNOP_WRITE(cachevp, in_uio, 0, ap->a_context);
     
     VATTR_INIT(&vattr);
     VATTR_SET(&vattr, va_data_size, uio_offset(in_uio));
     /* Update size of cache file */
     vnode_setattr(cachevp, &vattr, ap->a_context);
     
     // Code for sending request to agent containing data size and offset
     // Agent will read the data of give size from cache file at
     // give offset and send it to webdav server 
     ...  
   }
   ```

7. The lack of validation in step 3 by kext can be exploited to perform unauthorized IO operations on a file. Consider a file `/etc/zprofile` in standard MacOS installation. The permissions for this file is `-r--r--r--` which prevents write access to all .  A malicious agent process running in standard user context can open the `zprofile` file for reading. Hence it can obtain file descriptor to that file using `open` syscall. If the malicious agent process sends the file descriptor to `zprofile` in step 2 `sysctl` syscall, the kext will associate the cache file to file named `portal` in root of WebDAV mount. Hence writing data to `portal` file will write data to `zprofile` profile.



Same process can be used to read data from a write only file.

# Conclusion
