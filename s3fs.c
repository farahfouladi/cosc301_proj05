
/* This code is based on the fine code written by Joseph Pfeiffer for his
  fuse system tutorial. */

#include "s3fs.h"
#include "libs3_wrapper.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

#define GET_PRIVATE_DATA ((s3context_t *) fuse_get_context()->private_data)

/*
* For each function below, if you need to return an error,
* read the appropriate man page for the call and see what
* error codes make sense for the type of failure you want
* to convey.  For example, many of the calls below return
* -EIO (an I/O error), since there are no S3 calls yet
* implemented.  (Note that you need to return the negative
* value for an error code.)
*/

/* *************************************** */
/*        Stage 1 callbacks                */
/* *************************************** */

/*
* Initialize the file system.  This is called once upon
* file system startup.
*/
void *fs_init(struct fuse_conn_info *conn)
{
   fprintf(stderr, "fs_init --- initializing file system.\n");
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
   s3fs_clear_bucket(s3bucket);
   s3dirent_t root_dir;
   root_dir.type = 'D';
   root_dir.name = ".";
   char* key = "/";
root_dir.protection = S_IFDIR;
root_dir.type = 'D';
root_dir.user_id = getuid();
root_dir.group_id = getgid();
root_dir.hard_links = 0;
root_dir.size = 0;
root_dir.last_access = time(NULL);
root_dir.mod_time = time(NULL);
root_dir.status_change = time(NULL);
   s3fs_put_object(s3bucket, key, (uint8_t*)&root_dir, sizeof(s3dirent_t)); 
   return ctx;
}

/*
* Clean up filesystem -- free any allocated data.
* Called once on filesystem exit.
*/
void fs_destroy(void *userdata) {
   fprintf(stderr, "fs_destroy --- shutting down file system.\n");
   free(userdata);
}


/* 
* Get file attributes.  Similar to the stat() call
* (and uses the same structure).  The st_dev, st_blksize,
* and st_ino fields are ignored in the struct (and 
* do not need to be filled in).
*/

int fs_getattr(const char *path, struct stat *statbuf) {
   fprintf(stderr, "fs_getattr(path=\"%s\")\n", path);
   s3context_t *ctx = GET_PRIVATE_DATA;
char* s3bucket = (char*)ctx;
s3dirent_t* dirs = NULL;
s3dirent_t* more_dirs = NULL;
   ssize_t size = s3fs_get_object(s3bucket, path, (uint8_t**)&dirs, 0, 0);
   if (size<0) {
       printf("This object does not exist\n");
free(dirs);
return -ENOENT;
}
char* copy_path_1 = strdup(path);
char* copy_path_2 = strdup(path);
char* directory = dirname(copy_path_1);
char* base = basename(copy_path_2);
   size = s3fs_get_object(s3bucket, directory, (uint8_t**)&more_dirs, 0, 0);
if (size < 0) {
free(dirs);
free(copy_path_1);
free(copy_path_2);
free(more_dirs);
return -EIO;
}
int entries = size/sizeof(s3dirent_t);
printf("ENTRIES = %d\n", entries);
int i=0;
for(;i<entries;i++) {
if ( strcasecmp(more_dirs[i].name, base) == 0) {
if (more_dirs[i].type == 'F') {
printf("found the file! & doing stat for file!\n");
statbuf->st_mode = more_dirs[i].protection;
statbuf->st_uid = more_dirs[i].user_id;
   	 statbuf->st_nlink = more_dirs[i].hard_links;
statbuf->st_size = more_dirs[i].size;
statbuf->st_atime = more_dirs[i].last_access;
statbuf->st_mtime = more_dirs[i].mod_time;
statbuf->st_ctime = more_dirs[i].status_change;
free(copy_path_1);
free(copy_path_2);
free(more_dirs);
free(dirs);
return 0;
}
}	
}
printf("doing stat for a directory!\n");
statbuf->st_mode = dirs[0].protection;
statbuf->st_uid = dirs[0].user_id;
   statbuf->st_nlink = dirs[0].hard_links;
statbuf->st_size = dirs[0].size;
statbuf->st_atime = dirs[0].last_access;
statbuf->st_mtime = dirs[0].mod_time;
statbuf->st_ctime = dirs[0].status_change;
free(dirs);
free(copy_path_1);
free(copy_path_2);
free(more_dirs);
return 0;
}


/*
* Open directory
*
* This method should check if the open operation is permitted for
* this directory
*/
int fs_opendir(const char *path, struct fuse_file_info *fi) {
   fprintf(stderr, "fs_opendir(path=\"%s\")\n", path);
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
s3dirent_t* dirs = NULL;
   ssize_t size = s3fs_get_object(s3bucket, path, (uint8_t**)&dirs, 0, 0);
   if (size>=0){
free(dirs);
return 0;
}
free(dirs);
return -EIO;
}


/*
* Read directory.  See the project description for how to use the filler
* function for filling in directory items.
*/
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
        struct fuse_file_info *fi)
{
   fprintf(stderr, "fs_readdir(path=\"%s\", buf=%p, offset=%d)\n",
         path, buf, (int)offset);
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
s3dirent_t* dirs_to_read = NULL;
   ssize_t size = s3fs_get_object(s3bucket, path, (uint8_t**)&dirs_to_read, 0, 0);
int entries = size/sizeof(s3dirent_t);
int i=0;
for (;i<entries;i++) {
if (filler(buf,dirs_to_read[i].name,NULL,0) != 0) {
free(dirs_to_read);
return -EIO;
}
}
free(dirs_to_read);
   return 0;
}


/*
* Release directory.
*/
int fs_releasedir(const char *path, struct fuse_file_info *fi) {
   fprintf(stderr, "fs_releasedir(path=\"%s\")\n", path);
   return 0;
}


/* 
* Create a new directory.
*
* Note that the mode argument may not have the type specification
* bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
* correct directory type bits (for setting in the metadata)
* use mode|S_IFDIR.
*/
int fs_mkdir(const char *path, mode_t mode) {
   fprintf(stderr, "fs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
   mode |= S_IFDIR;
s3dirent_t* init_dirs = NULL;
// check if the directory already exists
   ssize_t size = s3fs_get_object(s3bucket, path, (uint8_t**)&init_dirs, 0, 0);
if (size >= 0) {
       printf("This Directory Already Exists!\n");
free(init_dirs);
return -EIO;
}
// put a new object
s3dirent_t new_dir_obj;
new_dir_obj.name = ".";
new_dir_obj.type = 'D';
new_dir_obj.protection = mode;
new_dir_obj.user_id = getuid();
new_dir_obj.group_id = getgid();
new_dir_obj.hard_links = 0;
new_dir_obj.size = 0;
new_dir_obj.last_access = time(NULL);
new_dir_obj.mod_time = time(NULL);
new_dir_obj.status_change = time(NULL);
   s3fs_put_object(s3bucket, path, (uint8_t*)&new_dir_obj, sizeof(s3dirent_t) );
// add to end of directory
printf("JUST ADDED NEW OBJECT WITH KEY %s\n", path);
s3dirent_t* dirs = NULL;
char* copy_path_1 = strdup(path);
char* copy_path_2 = strdup(path);
char* directory = dirname(copy_path_1);
printf("DIRECTORY = %s\n", directory);
char* base = basename(copy_path_2);
printf("BASE = %s\n", base);
   size = s3fs_get_object(s3bucket, directory, (uint8_t**)&dirs, 0, 0);
if (size < 0) {
free(init_dirs);
free(copy_path_1);
free(copy_path_2);
free(dirs);
return -EIO;
}
int entries = size/sizeof(s3dirent_t);
printf("NEW ENTRIES mkdir %d\n", entries);
s3dirent_t new_dirent;
new_dirent.name = base;
s3dirent_t new_arr[entries+1];
int i=0;
for(;i<entries;i++) {
printf("ADDING TO NEW ARR %s at %d\n", dirs[i].name, i);
new_arr[i] = dirs[i];
}
new_arr[i] = new_dirent;
printf("ADDING TO NEW ARR %s at %d\n", new_dirent.name, i);
s3fs_put_object(s3bucket, directory, (uint8_t*)&new_arr, sizeof(s3dirent_t)*(entries+1));
//missing frees
free(init_dirs);
   return 0;
}


/*
* Remove a directory. 
*/
int fs_rmdir(const char *path) {
   fprintf(stderr, "fs_rmdir(path=\"%s\")\n", path);
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
s3dirent_t* dirs_r = NULL;
   ssize_t s = s3fs_get_object(s3bucket, path, (uint8_t**)&dirs_r, 0, 0);
if (s<0) {
free(dirs_r);
return -EIO; // directory does not exist
}
int e = s/sizeof(s3dirent_t);
if (e>1)
{
free(dirs_r);
return -EIO; // there is more than one thing in this 
}
char* cpy_path_1 = strdup(path);
char* cpy_path_2 = strdup(path);
char* d = dirname(cpy_path_1);
char* b = basename(cpy_path_2);
s3dirent_t* dirs_rem = NULL;
   ssize_t size = s3fs_get_object(s3bucket, d, (uint8_t**)&dirs_rem, 0, 0);
if (size<0) {
free(dirs_r);
return -EIO; // directory does not exist
}
int entries = size/sizeof(s3dirent_t);
printf("NUM ENTRIES IN REMOVE IS %d\n", entries);
s3dirent_t arr[entries-1];
int i=0;
int k=0;
for (;i<entries;i++) {
       printf("NAME? in remove %s\n", dirs_rem[i].name);
printf("base name is %s\n", b);
if (strcasecmp(dirs_rem[i].name,b) != 0) {
printf("NOT THE SAME!\n");
arr[k] = dirs_rem[i];
k++;
}
}
printf("ADDING TO NEW ARR IN REM\n");
s3fs_put_object(s3bucket, d, (uint8_t*)&arr, ((sizeof(s3dirent_t))*(entries-1)));
printf("REMOVING OBJECT (PATH) %s\n", path);
s3fs_remove_object(s3bucket, path);
free(dirs_r);	
return 0;
}


/* *************************************** */
/*        Stage 2 callbacks                */
/* *************************************** */


/* 
* Create a file "node".  When a new file is created, this
* function will get called.  
* This is called for creation of all non-directory, non-symlink
* nodes.  You *only* need to handle creation of regular
* files here.  (See the man page for mknod (2).)
*/
int fs_mknod(const char *path, mode_t mode, dev_t dev) {
   fprintf(stderr, "fs_mknod(path=\"%s\", mode=0%3o)\n", path, mode);
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
s3dirent_t* dirs = NULL;
ssize_t size = s3fs_get_object(s3bucket, path, (uint8_t**)&dirs, 0, 0);
if (size >= 0) {
       printf("This File Already Exists!\n");
return -EIO;
}
char* copy_path_1 = strdup(path);
char* copy_path_2 = strdup(path);
char* directory = dirname(copy_path_1);
printf("DIRECTORY = %s\n", directory);
char* base = basename(copy_path_2);
printf("BASE = %s\n", base);
// PUT a new file object containing empty content
FILE* f = fopen(base,"a+");
   if ( s3fs_put_object(s3bucket, path, (uint8_t*)f, sizeof(f) ) < 0 ) {
return -EIO;
}
printf("JUST ADDED NEW FILE OBJECT WITH KEY %s\n", path);
// add to end of directory
   size = s3fs_get_object(s3bucket, directory, (uint8_t**)&dirs, 0, 0);
if (size < 0) {
return -EIO;
}
int entries = size/sizeof(s3dirent_t);
printf("NUM ENTRIES mknod %d\n", entries);
s3dirent_t file_obj;
file_obj.name = base;
file_obj.type = 'F';
file_obj.protection = mode;
file_obj.user_id = getuid();
file_obj.group_id = getgid();
file_obj.hard_links = 0;
file_obj.size = sizeof(f);
file_obj.last_access = time(0);
file_obj.mod_time = time(0);
file_obj.status_change = time(0);

s3dirent_t new_arr[entries+1];
int i=0;
for(;i<entries;i++) {
printf("ADDING TO NEW ARR %s at %d\n", dirs[i].name, i);
new_arr[i] = dirs[i];
}
new_arr[i] = file_obj;
printf("ADDING TO NEW NEW ARR %s at %d\n", file_obj.name, i);
s3fs_put_object(s3bucket, directory, (uint8_t*)&new_arr, sizeof(s3dirent_t)*(entries+1));
   return 0;
}


/* 
* File open operation
* No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
* will be passed to open().  Open should check if the operation
* is permitted for the given flags.  
* 
* Optionally open may also return an arbitrary filehandle in the 
* fuse_file_info structure (fi->fh).
* which will be passed to all file operations.
* (In stages 1 and 2, you are advised to keep this function very,
* very simple.)
*/
int fs_open(const char *path, struct fuse_file_info *fi) {
   fprintf(stderr, "fs_open(path\"%s\")\n", path);
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
s3dirent_t* f_dirs = NULL;
   ssize_t size = s3fs_get_object(s3bucket, path, (uint8_t**)&f_dirs, 0, 0);
   if (size>=0){
return 0;
}
return -EIO;    
}


/* 
* Read data from an open file
*
* Read should return exactly the number of bytes requested except
* on EOF or error, otherwise the rest of the data will be
* substituted with zeroes.  
*/
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   fprintf(stderr, "fs_read(path=\"%s\", buf=%p, size=%d, offset=%d)\n",
         path, buf, (int)size, (int)offset);
   s3context_t *ctx = GET_PRIVATE_DATA;
   char* s3bucket = (char*)ctx;
s3dirent_t* read_f_dirs = NULL;
ssize_t size_obj = s3fs_get_object(s3bucket, path, (uint8_t**)&buf, offset, size);
if (size_obj<0){
return -EIO;
printf("This path does not exist!");
}
return 0;
}


/*
* Write data to an open file
*
* Write should return exactly the number of bytes requested
* except on error.
*/
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   fprintf(stderr, "fs_write(path=\"%s\", buf=%p, size=%d, offset=%d)\n",
         path, buf, (int)size, (int)offset);
   s3context_t *ctx = GET_PRIVATE_DATA;
char* s3bucket = (char*)ctx;
FILE* f = NULL;
ssize_t size_obj = s3fs_get_object(s3bucket, path, (uint8_t**)&f, offset, size);
if (size_obj<0){
return -EIO;
}
fseek(f,offset,SEEK_SET);
fwrite(buf,1,size,f);
   return 0;
}


/*
* Release an open file
*
* Release is called when there are no more references to an open
* file: all file descriptors are closed and all memory mappings
* are unmapped.  
*
* For every open() call there will be exactly one release() call
* with the same flags and file descriptor.  It is possible to
* have a file opened more than once, in which case only the last
* release will mean, that no more reads/writes will happen on the
* file.  The return value of release is ignored.
*/
int fs_release(const char *path, struct fuse_file_info *fi) {
   fprintf(stderr, "fs_release(path=\"%s\")\n", path);
   //s3context_t *ctx = GET_PRIVATE_DATA;
   //return -EIO;
return 0;
}


/*
* Rename a file.
*/
int fs_rename(const char *path, const char *newpath) {
   fprintf(stderr, "fs_rename(fpath=\"%s\", newpath=\"%s\")\n", path, newpath);
   s3context_t *ctx = GET_PRIVATE_DATA;
char* s3bucket = (char*)ctx;
FILE* f = NULL;
s3dirent_t* more_dirs = NULL;
   ssize_t size = s3fs_get_object(s3bucket, path, (uint8_t**)&f, 0, 0);
   if (size<0) {
       printf("This object does not exist\n");
return -ENOENT;
}
s3fs_remove_object(s3bucket,path);
s3fs_put_object(s3bucket, path, (uint8_t*)f, sizeof(f));
char* copy_path_1 = strdup(path);
char* copy_path_2 = strdup(path);
char* directory = dirname(copy_path_1);
char* base = basename(copy_path_2);
char* copy_new_path = strdup(newpath);
char* newbase = basename(copy_new_path);
   size = s3fs_get_object(s3bucket, directory, (uint8_t**)&more_dirs, 0, 0);
if (size < 0) {
return -EIO;
}
int entries = size/sizeof(s3dirent_t);
printf("ENTRIES = %d\n", entries);
int i=0;
for(;i<entries;i++) {
if ( strcasecmp(more_dirs[i].name,base) == 0) {
more_dirs[i].name = newbase;
}
}
s3fs_put_object(s3bucket, directory, (uint8_t*)more_dirs, sizeof(more_dirs));
   return -EIO;
}


/*
* Remove a file.
*/
int fs_unlink(const char *path) {
   fprintf(stderr, "fs_unlink(path=\"%s\")\n", path);
       s3context_t *ctx = GET_PRIVATE_DATA;
char* cpy_path_1 = strdup(path);
char* cpy_path_2 = strdup(path);
char* d = dirname(cpy_path_1);
char* b = basename(cpy_path_2);
   char* s3bucket = (char*)ctx;
s3dirent_t* dirs_rem = NULL;
   ssize_t size = s3fs_get_object(s3bucket, d, (uint8_t**)&dirs_rem, 0, 0);
if (size<0) {
return -EIO; // directory does not exist
}
int entries = size/sizeof(s3dirent_t);
s3dirent_t arr[entries-1];
int i=0;
int k=0;
for (;i<entries;i++) {
if (strcasecmp(dirs_rem[i].name,b) != 0) {
arr[k] = dirs_rem[i];
k++;
}
}
s3fs_put_object(s3bucket, d, (uint8_t*)&arr, ((sizeof(s3dirent_t))*(entries-1)));
s3fs_remove_object(s3bucket, path);	
return 0;
}
/*
* Change the size of a file.
*/
int fs_truncate(const char *path, off_t newsize) {
   fprintf(stderr, "fs_truncate(path=\"%s\", newsize=%d)\n", path, (int)newsize);
   //s3context_t *ctx = GET_PRIVATE_DATA;
   return -EIO;
}


/*
* Change the size of an open file.  Very similar to fs_truncate (and,
* depending on your implementation), you could possibly treat it the
* same as fs_truncate.
*/
int fs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
   fprintf(stderr, "fs_ftruncate(path=\"%s\", offset=%d)\n", path, (int)offset);
   //s3context_t *ctx = GET_PRIVATE_DATA;
   return -EIO;
}


/*
* Check file access permissions.  For now, just return 0 (success!)
* Later, actually check permissions (don't bother initially).
*/
int fs_access(const char *path, int mask) {
   fprintf(stderr, "fs_access(path=\"%s\", mask=0%o)\n", path, mask);
   //s3context_t *ctx = GET_PRIVATE_DATA;
   return 0;
}


/*
* The struct that contains pointers to all our callback
* functions.  Those that are currently NULL aren't 
* intended to be implemented in this project.
*/
struct fuse_operations s3fs_ops = {
 .getattr     = fs_getattr,    // get file attributes
 .readlink    = NULL,          // read a symbolic link
 .getdir      = NULL,          // deprecated function
 .mknod       = fs_mknod,      // create a file
 .mkdir       = fs_mkdir,      // create a directory
 .unlink      = fs_unlink,     // remove/unlink a file
 .rmdir       = fs_rmdir,      // remove a directory
 .symlink     = NULL,          // create a symbolic link
 .rename      = fs_rename,     // rename a file
 .link        = NULL,          // we don't support hard links
 .chmod       = NULL,          // change mode bits: not implemented
 .chown       = NULL,          // change ownership: not implemented
 .truncate    = fs_truncate,   // truncate a file's size
 .utime       = NULL,          // update stat times for a file: not implemented
 .open        = fs_open,       // open a file
 .read        = fs_read,       // read contents from an open file
 .write       = fs_write,      // write contents to an open file
 .statfs      = NULL,          // file sys stat: not implemented
 .flush       = NULL,          // flush file to stable storage: not implemented
 .release     = fs_release,    // release/close file
 .fsync       = NULL,          // sync file to disk: not implemented
 .setxattr    = NULL,          // not implemented
 .getxattr    = NULL,          // not implemented
 .listxattr   = NULL,          // not implemented
 .removexattr = NULL,          // not implemented
 .opendir     = fs_opendir,    // open directory entry
 .readdir     = fs_readdir,    // read directory entry
 .releasedir  = fs_releasedir, // release/close directory
 .fsyncdir    = NULL,          // sync dirent to disk: not implemented
 .init        = fs_init,       // initialize filesystem
 .destroy     = fs_destroy,    // cleanup/destroy filesystem
 .access      = fs_access,     // check access permissions for a file
 .create      = NULL,          // not implemented
 .ftruncate   = fs_ftruncate,  // truncate the file
 .fgetattr    = NULL           // not implemented
};



/* 
* You shouldn't need to change anything here.  If you need to
* add more items to the filesystem context object (which currently
* only has the S3 bucket name), you might want to initialize that
* here (but you could also reasonably do that in fs_init).
*/
int main(int argc, char *argv[]) {
   // don't allow anything to continue if we're running as root.  bad stuff.
   if ((getuid() == 0) || (geteuid() == 0)) {
   	fprintf(stderr, "Don't run this as root.\n");
   	return -1;
   }
   s3context_t *stateinfo = malloc(sizeof(s3context_t));
   memset(stateinfo, 0, sizeof(s3context_t));

   char *s3key = getenv(S3ACCESSKEY);
   if (!s3key) {
       fprintf(stderr, "%s environment variable must be defined\n", S3ACCESSKEY);
       return -1;
   }
   char *s3secret = getenv(S3SECRETKEY);
   if (!s3secret) {
       fprintf(stderr, "%s environment variable must be defined\n", S3SECRETKEY);
       return -1;
   }
   char *s3bucket = getenv(S3BUCKET);
   if (!s3bucket) {
       fprintf(stderr, "%s environment variable must be defined\n", S3BUCKET);
       return -1;
   }
   strncpy((*stateinfo).s3bucket, s3bucket, BUFFERSIZE);

   fprintf(stderr, "Initializing s3 credentials\n");
   s3fs_init_credentials(s3key, s3secret);

   fprintf(stderr, "Totally clearing s3 bucket\n");
   s3fs_clear_bucket(s3bucket);

   fprintf(stderr, "Starting up FUSE file system.\n");
   int fuse_stat = fuse_main(argc, argv, &s3fs_ops, stateinfo);
   fprintf(stderr, "Startup function (fuse_main) returned %d\n", fuse_stat);
   
   return fuse_stat;
}
