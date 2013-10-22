/*
 * file:        homework.c
 * description: skeleton file for CS 5600 homework 3
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, updated April 2012
 * $Id: homework.c 452 2011-11-28 22:25:31Z pjd $
 */

#define FUSE_USE_VERSION 27

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "cs5600fs.h"
#include "blkdev.h"
#include <time.h>

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 512-byte SECTORS, while the
 * file system uses 1024-byte BLOCKS. Remember to multiply everything
 * by 2.
 */

extern struct blkdev *disk;
struct cs5600fs_super *super = NULL;
struct cs5600fs_entry *fat = NULL;
char *cpath = NULL;
char *dpath = NULL;
char *databuf = NULL;
char *datapool = NULL;
char *strwrd(char*,char*, size_t,char*);  // split string by token '/'
int dir_exist(char *s_path, unsigned int blknum ); // check if directory exist
int this_block_full(unsigned int);//check if block is full
int find_free_block(void);// find a free block

/* init - this is called once by the FUSE framework at startup.
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void* hw3_init(struct fuse_conn_info *conn)
{
  super = calloc(1,1024);
  fat = calloc(4,1024);
  cpath = calloc(1,1024);
  dpath = calloc(1,1024);
  databuf = calloc(1,1024);
  datapool = calloc(1,4096);
  disk->ops->read(disk,0,2,(char*) super); // read block 0 into super block
  disk->ops->read(disk,2,8,(void*) fat); // read block 1 into fat
  return NULL;
}

char *strwrd(char *s, char *buf,size_t len, char *delim)
{
  s +=strspn(s, delim);
  int n = strcspn(s, delim);
  if( len - 1 < n)
    n = len - 1;
  memset(buf, 0, len);
  memcpy(buf,s, n);
  s +=n;
  return s;
}
/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in CS5600fs are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
int find_entry(struct cs5600fs_dirent *, char*,unsigned int*);
static int get_stat(struct cs5600fs_dirent,struct stat*);
static int hw3_getattr(const char *path, struct stat *sb)
{
   strcpy(cpath,path);
   char name[512];
   unsigned int blknum = super->root_dirent.start;
   struct cs5600fs_dirent directory[16];
   int ent_num;
   while(strlen(cpath) != 0)
   {
      memset(name,0,512);
      cpath = strwrd(cpath, name, 512, "/");
      disk->ops->read(disk,blknum*2,2,(void*)directory);
      ent_num = find_entry(directory,name,&blknum); // find the entry with the given name 
      if (ent_num == 256)//if not find, return ENOENT
        return -ENOENT;
      if(strlen(cpath) && !directory[ent_num].isDir) // return ENOTDIR if not a directory
        return -ENOTDIR;
      if (!(strlen(cpath)))
        return get_stat(directory[ent_num],sb);//get the attributes of the drectory
      blknum = directory[ent_num].start;
   }
  return 0;
}

int get_stat(struct cs5600fs_dirent directory, struct stat *sb)
{
  sb->st_dev = 0;
  sb->st_ino = 0;
  sb->st_rdev = 0;
  sb->st_blocks = (directory.length + FS_BLOCK_SIZE -1) / FS_BLOCK_SIZE;
  sb->st_blksize = FS_BLOCK_SIZE;//
  sb->st_nlink=1;//
  sb->st_uid = directory.uid;
  sb->st_gid = directory.gid;
  sb->st_size = directory.length;
  sb->st_mtime = directory.mtime;
  sb->st_ctime = directory.mtime;
  sb->st_atime = directory.mtime;//
  sb->st_mode = directory.mode|(directory.isDir ? S_IFDIR:S_IFREG);//directory.mode;
  return 0;
}

int find_entry(struct cs5600fs_dirent *directory, char *name, 
              unsigned int *blknum)
{
  int i;
  for(i = 0; i<16;i++)
  {
    if(directory[i].valid && !(strcmp(name,directory[i].name)))
       return i; // if find the entry, return it's entry number
  }
  if (!fat[*blknum].eof) // if it's not the end of the directory, cotinue to search other blocks of this directory
  { 
    disk->ops->read(disk,(fat[*blknum].next)*2,2,(void*)directory);
    *blknum = fat[*blknum].next;
    return find_entry(directory,name,blknum);
  }
  return 256;
 }
 

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
int show_dir_detail(unsigned int,void*,fuse_fill_dir_t);
static int hw3_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
  strcpy(cpath,(path+1));
  char name[512];
  unsigned int blknum = super->root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num=0;
  
  if(!strlen(cpath))
  {
    show_dir_detail(blknum,buf,filler);
    return 0;
  }
  
  // find the directory via path
  while(strlen(cpath) !=0)
  {
    memset(name,0,512);
    cpath = strwrd(cpath,name,512,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if(ent_num == 256)
      return -ENOENT; // if entry not find retrun error
    blknum = directory[ent_num].start;
      if(!(strlen(cpath)))
      { 
        if(!directory[ent_num].isDir)
          {printf("%s\n",(path+1));}
	else
          show_dir_detail(blknum,buf,filler);//if directory is find, show the entries details
      }
    
    blknum = directory[ent_num].start;
  }
  return 0;
}

int show_dir_detail(unsigned int blknum,void* buf,fuse_fill_dir_t filler)
{
  struct stat sb;
  int i;
  char name[126];
  struct cs5600fs_dirent directory[16];
  memset(&sb,0,sizeof(sb));
  disk->ops->read(disk,blknum*2,2,(void*)directory);
  for(;;)
  { 
    if(!fat[blknum].inUse)
      return 0;
    // get details of all the entries in the directory
    for(i=0;i<16;i++)
     {
      if (!directory[i].valid)
        continue;
      sb.st_dev = sb.st_ino = sb.st_rdev = 0;
      sb.st_nlink = 1;
      sb.st_blocks = (directory[i].length + FS_BLOCK_SIZE -1) / FS_BLOCK_SIZE;
      sb.st_blksize = FS_BLOCK_SIZE;
      sb.st_uid = directory[i].uid;
      sb.st_gid = directory[i].gid; 
      sb.st_mode = directory[i].mode;
      sb.st_size = directory[i].length;
      sb.st_atime = sb.st_ctime = sb.st_mtime = directory[i].mtime;
      strcpy(name,directory[i].name);
      filler(buf,name,&sb,0);
     }
    if (fat[blknum].eof)
      return 0;
    // continue if this block is not the end of this directory
    blknum = fat[blknum].next;
    disk->ops->read(disk,blknum*2,2,(void*)directory);
  } 
}

/* create - create a new file with permissions (mode & 01777)
 *
 * Errors - path resolution, EEXIST
 *
 * If a file or directory of this name already exists, return -EEXIST.
 */
int check_file_dir (char *s_path, unsigned int blknum );
int create_file (struct cs5600fs_dirent *directory, unsigned int blknum,
                 char *cpath, mode_t mode);

int insert_file_here(struct cs5600fs_dirent *,unsigned int, char*, mode_t);
static int hw3_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
  char name[44];
  unsigned int blknum = super->root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num;

  strcpy(cpath,path);
  
  // find directory via path
  while(strlen(cpath)!=0)
  {
    memset(name,0,44);
    cpath = strwrd(cpath,name,44,"/");
    disk->ops->read(disk, blknum*2,2,(void*)directory);
    ent_num = find_entry(directory, name, &blknum);
    if(ent_num == 256)
    {
       if(strlen(cpath)!=0)
         return -ENOTDIR; // if it is not a directory, return error
       cpath = cpath - strlen(name); // get the name of the file
         return insert_file_here(directory,blknum,cpath,(mode*01777));
     }
    blknum = directory[ent_num].start;
  }
  return create_file(directory, blknum, name, mode);
}


// insert file in the directory with mode 
int insert_file_here(struct cs5600fs_dirent * directory, unsigned int blknum,
                      char* cpath, mode_t mode)
{
 int i = 0;
 char name[44];
 unsigned int free_block = 65535;
 while (strlen(cpath))
 {
  cpath = strwrd(cpath,name,44,"/");
  if(!strlen(cpath))
   return create_file(directory,blknum,name,mode);

// if this block is full, find a free block and insert in it
 if(this_block_full(blknum))
  {
   fat[blknum].eof = 0;
   free_block = find_free_block();
   fat[blknum].next = free_block;
   disk->ops->read(disk,2*free_block,2,(void*)directory);
   directory[0].valid = 1;
   directory[0].isDir = 1;
   directory[0].mode = mode;
   directory[0].length = 0;
   free_block = find_free_block();
   directory[0].start = free_block;
   directory[0].uid = getuid();
   directory[0].gid = getgid();
   if (free_block <0)
   { 
    printf("Sys doesn't have enough block\n");
    exit(-1);
    }
   strcpy(directory[0].name,name);
   directory[0].mtime = time(NULL);
   disk->ops->write(disk,fat[blknum].next*2,2,(void*)directory);
   }
   else
   {
    for(i = 0;i<16;i++)
   // find a free entry 
      if(!directory[i].valid)
      {
       free_block = find_free_block();
       directory[i].start = free_block;
       if(free_block < 0)
       {
         printf("Sys doesn't have enough block \n");
	 exit(-1);
       }
       directory[i].uid = getuid();
       directory[i].valid = 1;
       directory[i].isDir = 1;
       directory[i].mode = mode;
       directory[i].length = 0;
       strcpy(directory[i].name,name);
       directory[i].uid = getuid();
       directory[i].gid = getgid();
       directory[i].mtime = time(NULL);
       disk->ops->write(disk,blknum*2,2,(void*)directory);
       break;
       }
       disk->ops->write(disk,2,8,(void*)fat);
      }
      blknum = free_block;
     }
    return 0;
}

int create_blk_full( struct cs5600fs_dirent *directory,unsigned int blknum,
                      char *name, mode_t mode );
int create_blk_avai( struct cs5600fs_dirent *directory, unsigned int blknum,
                      char *name, mode_t mode);

int create_file (struct cs5600fs_dirent *directory, unsigned int blknum,
                  char *cpath, mode_t mode)
{
  if(this_block_full(blknum))
    return create_blk_full(directory, blknum, cpath, mode);
  else
    return create_blk_avai(directory, blknum, cpath, mode);
}


// create file when the block is not full
int create_blk_avai( struct cs5600fs_dirent *directory, unsigned int blknum,
                      char *name, mode_t mode)
{
  unsigned int free_block = 65535;
  int i = 0;
  for(i = 0; i<16; i++)
  {

   // find a free entry
    if(!directory[i].valid)
    {
      free_block = find_free_block();
      if(free_block < 0)
      {
      printf("Sys don't have enough block\n");
      exit(-1);
      }
      directory[i].start = free_block;
      directory[i].uid = getuid();
      directory[i].gid = getgid();
      directory[i].valid = 1;
      directory[i].isDir = 0;
      directory[i].mode = mode;
      directory[i].length = 0;
      strcpy(directory[i].name,name);
      directory[i].mtime= time(NULL);
      disk->ops->write(disk, blknum*2,2, (void*)directory);
      break; 
    }
  }
  disk->ops->write(disk,2,8,(void*)fat);
  return 0;  
}

//create file when the block is full. need to find a new block as new directory, inset file entry as entry[0]
int create_blk_full( struct cs5600fs_dirent *directory,unsigned int blknum,
                      char *name, mode_t mode )
{
  unsigned int free_block = 65535;
  fat[blknum].eof = 0;
  free_block = find_free_block();
  if(free_block < 0)
  {
    printf("Sys don't have enough block\n");
    exit(-1);
  }   
  fat[blknum].next = free_block;
  disk->ops->read(disk, free_block*2,2,(void *)directory);
  directory[0].valid =1;
  directory[0].uid = getuid();
  directory[0].gid = getgid();
  directory[0].isDir =0;
  directory[0].mode = mode;
  directory[0].length = 0;
  strcpy(directory[0].name, name);
  free_block = 65535;
  free_block = find_free_block();
  if(free_block < 0)
  {
    printf("Sys don't have enough block\n");
    exit(-1);
  }
  //find a free block as dada file, let entry point to it
  directory[0].start = free_block;
  directory[0].mtime = time(NULL); 
  disk->ops->write(disk, fat[blknum].next*2,2,(void *)directory);
  disk->ops->write(disk, 2, 8, (void *)fat);
  return 0;  
}
//check if this is directory
int check_file_dir (char *s_path, unsigned int blknum )
{
  char name[44];
  int ent_num = 0;
  struct cs5600fs_dirent directory[16];
  while(strlen(s_path)!=0)
  {
    memset(name,0,44);
    s_path = strwrd(s_path,name,44,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory, name, &blknum);
    if (strlen(s_path)!=0)
    {
      if(!ent_num == 256)
       { if(!directory[ent_num].isDir)
        return 0;}
      else
       return 1;
      blknum = directory[ent_num].start;
    }
  }  
  return 1;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */
int insert_dir_here(struct cs5600fs_dirent *,unsigned int, char*, mode_t);

static int hw3_mkdir(const char *path, mode_t mode)
{
  char name[512];
  unsigned int blknum = super->root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num;
  strcpy(cpath,path);
  if(!check_file_dir(cpath, blknum))
    return -ENOTDIR;
  strcpy(cpath,path);

  while(strlen(cpath) !=0)
  {
    memset(name,0,512);
    cpath = strwrd(cpath,name,512,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if (ent_num == 256)
    { 
      if(strlen(cpath) !=0 )
        return -ENOTDIR;
      cpath = cpath - strlen(name);
      return insert_dir_here(directory,blknum,cpath,mode);
    }
    blknum = directory[ent_num].start;
  }
  return -EEXIST;
}

// insert new directory
int insert_dir_here(struct cs5600fs_dirent * directory, 
                    unsigned int blknum, char* cpath, mode_t mode)
{
  int i = 0;
  char name[512];
  unsigned int free_block = 65535; 
  while (strlen(cpath))
  {
    cpath = strwrd(cpath, name, 512, "/");
    if(this_block_full(blknum)) //if this bolock is full, find a new block as continued directory of the current one
    {
      fat[blknum].eof = 0;
      free_block = find_free_block();
      fat[blknum].next = free_block;
      disk->ops->read(disk,2*free_block,2,(void*)directory);
      directory[0].valid = 1;
      directory[0].isDir = 1;
      directory[0].mode = mode;
      directory[0].length = 0;
      free_block = find_free_block();
      directory[0].start = free_block;
      directory[0].uid = getuid();
      directory[0].gid = getgid();
      if (free_block<0)
      {
        printf("Sys don't have enough block\n");
        exit(-1);
      }
      strcpy(directory[0].name,name);
      directory[0].mtime = time(NULL);
      disk->ops->write(disk,fat[blknum].next*2,2,(void*)directory);
      disk->ops->write(disk, blknum*2,2,(void*)directory);//syy
    }
    else
    { 
      for(i = 0;i<16;i++)
        if(!directory[i].valid)  //if this entry is free, insert the entry in it
        {
          free_block = find_free_block();
	  directory[i].start = free_block;
	  if(free_block<0)
          { 
            printf("sys don't have enought block\n");
	    exit(-1);
	  }
	  directory[i].uid = getuid();
	  directory[i].valid = 1;
 	  directory[i].isDir = 1;
	  directory[i].mode = mode;
	  directory[i].length = 0;
          strcpy(directory[i].name,name);
          directory[i].uid = getuid();
          directory[i].gid = getgid();
          directory[i].mtime = time(NULL);
          disk->ops->write(disk,blknum*2,2,(void*)directory);
          break;
        }
      disk->ops->write(disk,2,8,(void*)fat);
    }
    blknum = free_block;
    disk->ops->read(disk,blknum*2,2,(void*)directory);
  } // END of While Loop
  return 0;
}


//check if this block is full
int this_block_full(unsigned int blknum)
{ 
  struct cs5600fs_dirent directory[16];
  int i;
  disk->ops->read(disk,blknum*2,2,(void*)directory);
  for(i=0;i<16;i++)
    if(!directory[i].valid)
      return 0;
  return 1;
}


// find a free block in the disk
int find_free_block()
{
  int i;
  char buf[1024];
  memset(buf,0,1024);
  for(i =0;i<1024;i++)
    if(!fat[i].inUse)
    {
     fat[i].inUse = 1;
     fat[i].eof = 1;
     disk->ops->write(disk,i*2,2,buf);	 
     return i;
    };
  return -3;
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 */
int unlink_file(unsigned int);

static int hw3_unlink(const char *path)
{
  char name[512];
  unsigned int blknum = super->root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num;

  strcpy(cpath,path);

  // find the directory of the file
  while(strlen(cpath) !=0)
  {
    memset(name,0,512);
    cpath = strwrd(cpath,name,512,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if (ent_num == 256)
      return -ENOENT;
    if(strlen(cpath) !=0)
      blknum = directory[ent_num].start;
  }
  if(directory[ent_num].isDir)
    return -EISDIR;
  unlink_file(directory[ent_num].start); // unlink it
  directory[ent_num].valid = 0;
  disk->ops->write(disk,2,8,(void*)fat); // change information in fat
  disk->ops->write(disk,blknum*2,2,(void*)directory); //change directory information
  return 0;
}

// unlink file
int unlink_file(unsigned int blknum)
{
  fat[blknum].inUse = 0;
  if (!fat[blknum].eof)
    unlink_file(fat[blknum].next);
  return 0;
}

/*
 *  rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
int clear_sub_dir(unsigned int);
int check_dir_empty(char *cpath);

static int hw3_rmdir(const char *path)
{
  strcpy(cpath,path);
  char name[512];
  unsigned int blknum = super->root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num;
  
  // dheck if directory is empty 
  if(!check_dir_empty(cpath))
    return -ENOTEMPTY;

  strcpy(cpath,path);
  while(strlen(cpath) != 0)
  {
    memset(name,0,512);
    cpath = strwrd(cpath,name,512,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if (ent_num == 256)
      return -ENOENT;
    if(strlen(cpath) !=0)
    blknum = directory[ent_num].start;
  }
  if(!directory[ent_num].isDir)
    return -ENOTDIR;

  // clear the directory
  clear_sub_dir(directory[ent_num].start);
  directory[ent_num].valid = 0;
  disk->ops->write(disk,2,8,(void*)fat);
  disk->ops->write(disk,blknum*2,2,(void*)directory);
  return 0;
}

//check if directory is empty
int check_dir_empty (char *cpath)
{
  char name[44];
  unsigned int blknum  = super->root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num = 0;
  int i;
  // find the directory
  while(strlen(cpath)!=0)
  {
    memset(name, 0, 44);
    cpath = strwrd(cpath, name, 44, "/");
    disk->ops->read(disk, blknum*2, 2, (void*)directory);
    ent_num = find_entry(directory, name, &blknum);
    blknum = directory[ent_num].start;
  } 
  disk->ops->read(disk, blknum*2,2,(void*)directory);
//check all its enties to see if its empty
  for( i = 0; i < 16; i ++)
  {
    if(directory[i].valid)
      return 0;
  }  
  return 1;
}
// clear the sub directories of the upper directory
int clear_sub_dir(unsigned int blknum)
{
  struct cs5600fs_dirent directory[16];
  int i;  
  disk->ops->read(disk,blknum*2,2,(void*)directory);
  for(i=0; i<16; i++)
  {
    if (directory[i].valid)
    {
      directory[i].valid = 0;
      if(directory[i].isDir)
      {
        clear_sub_dir(directory[i].start);
      }
      else
        unlink_file(directory[i].start);
    }
  }
  if ((!fat[blknum].eof)&&(fat[blknum].inUse))
    clear_sub_dir(fat[blknum].next);
  fat[blknum].next = 0;
  fat[blknum].eof = 1;
  fat[blknum].inUse = 0;
  disk->ops->write(disk,blknum*2,2,(void*)directory);
  return 0;
}
/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
//int path_exist(char *,unsigned int);
int check_paths_dir( char* , char* );
char* get_name( char *d_path);

static int hw3_rename(const char *src_path, const char *dst_path)
{   
  unsigned int blknum = super->root_dirent.start;
  char name[44];
  int ent_num = 0;
  struct cs5600fs_dirent directory[16];

  strcpy(cpath,src_path);
  strcpy(dpath,dst_path);
  if (!check_paths_dir(cpath, dpath))
    return -EINVAL;
  if (!dir_exist(cpath, blknum ))
    return -ENOENT; 
  if (dir_exist(dpath, blknum))
    return -EEXIST;
  strcpy(cpath, src_path);
  strcpy(dpath, dst_path);
  while(strlen(cpath) !=0)
//find the file directory
  {
    memset(name, 0, 44);
    cpath = strwrd(cpath, name, 44, "/");
    disk->ops->read(disk, blknum*2, 2, (void *)directory);
    ent_num = find_entry(directory, name, &blknum);
    if(strlen(cpath)!=0)
    blknum = directory[ent_num].start;
  } 
  
  // repalce the name field to the new name
  strcpy(directory[ent_num].name, get_name(dpath));
  directory[ent_num].mtime = time(NULL);    
  disk->ops->write(disk, blknum*2, 2, (void *)directory);
  return 0;
}

// get the new name
char* get_name( char *d_path)
{
  char name[44];
 
  while(strlen(d_path)!=0)
  {
    memset(name, 0, 44);
    d_path = strwrd(d_path, name, 44, "/");
  }   
  d_path = d_path - strlen(name);
  return d_path;
}


// check if directory exist
int dir_exist( char *s_path, unsigned int blknum)
{
  char name[44];
  int ent_num = 0;
  char *s;
  struct cs5600fs_dirent directory[16];
  
  s = calloc(1024, sizeof(char));
  strcpy(s, s_path);
  while(strlen(s)!=0)
  {
   memset(name,0,44);
   s = strwrd(s,name,44,"/");
   disk->ops->read(disk,blknum*2,2,(void*)directory);
   ent_num = find_entry(directory,name,&blknum);
   if(ent_num == 256)
     return 0;
   blknum = directory[ent_num].start;
  }
  return 1;
}
//check if source path and destination path are in the same directory
int check_paths_dir(char *s_path, char *d_path)
{
  char sname[44];
  char dname[44];
  char *s;
  char *d;
  s = calloc(1024, sizeof(char));
  d = calloc(1024, sizeof(char));
  strcpy(s, s_path);
  strcpy(d, d_path);
  s = strwrd(s, sname, 44, "/");
  d = strwrd(d, dname, 44, "/");
  while ((strlen(s))||(strlen(d)))
  {
    if (strlen(sname)!=strlen(dname))
      return 0;
    memset(sname, 0, 44);
    memset(dname, 0, 44);
    s = strwrd(s, sname, 44, "/");
    d = strwrd(d, dname, 44, "/");
  }
  return 1;
}

/* utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int hw3_chmod(const char *path, mode_t mode)
{
  strcpy(cpath,path);
  char name[512];
  unsigned int blknum = super -> root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num;
  while(strlen(cpath) !=0)
  {
    memset(name,0,512);
    cpath = strwrd(cpath,name,512,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if (ent_num == 256)
      return -ENOENT;
    if(strlen(cpath)&&!directory[ent_num].isDir)
      return -EISDIR;
    if(strlen(cpath))
      blknum = directory[ent_num].start;
  }

  // update information in the directory
  directory[ent_num].mode = mode;
  directory[ent_num].mtime = time(NULL);
  disk->ops->write(disk,blknum*2,2,(void*)directory);
  return 0;
}

// change modification time
int hw3_utime(const char *path, struct utimbuf *ut)
{
  char name[44];
  unsigned int blknum = super->root_dirent.start;
  int ent_num = 0;
  struct cs5600fs_dirent directory[16];
  strcpy(cpath,path);
  while(strlen(cpath)!=0)
  {
    memset(name,0,44);
    cpath = strwrd(cpath,name,44,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if(ent_num==256)
      return -ENOENT;
    if(strlen(cpath)!=0)
      blknum = directory[ent_num].start;
  }
  directory[ent_num].mtime = ut->modtime;
  disk->ops->write(disk,blknum*2,2,(void*)directory);
  return 0;
}
/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int hw3_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise. */
  if (len != 0)
    return -EINVAL;		/* invalid argument */
 
  char name[44];
  char *s;
  s=calloc(1,1024);
  unsigned int blknum = super->root_dirent.start;
  int ent_num = 0;
  struct cs5600fs_dirent directory[16];
  strcpy(cpath,path);
 
  while(strlen(cpath)!=0)
  {
    memset(name,0,44);
    cpath = strwrd(cpath,name,44,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if(ent_num==256)
      return -ENOENT;
    if(strlen(cpath)!=0)
      blknum = directory[ent_num].start;
  }
 // update infomation of the directory. set all the data block to unuse execpt the first one.   
  directory[ent_num].length = 0;
  disk->ops->write(disk,blknum*2,2,(void*)directory);
  blknum = directory[ent_num].start;
  disk->ops->write(disk,blknum*2,2,(void*)s); 
  if(fat[blknum].eof == 0)
  {
    fat[blknum].eof = 1;
    blknum = fat[blknum].next;
    while(fat[blknum].eof == 0)
    {
      blknum = fat[blknum].next;
      fat[blknum].inUse = 0;
    }
    fat[blknum].inUse =0;  
  }
  disk->ops->write(disk,2,8,(void*)fat);
  return 0;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= len, return 0
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */

// find the last block of a file
unsigned int blk_offset(int offset_num,unsigned int blknum)
{
 if(offset_num)
   blknum = blk_offset(--offset_num,fat[blknum].next);
 return blknum;
}

static int hw3_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
  strcpy(cpath,path);
  char name[512];
  memset(databuf,0,1024);
  memset(datapool,0,4096);
  unsigned int blknum = super->root_dirent.start;
  struct cs5600fs_dirent directory[16];
  int ent_num;
  int i ;
  while(strlen(cpath)!=0)
  {
    memset(name,0,512);
    cpath = strwrd(cpath,name,512,"/");
    disk->ops->read(disk,blknum*2,2,(void*)directory);
    ent_num = find_entry(directory,name,&blknum);
    if (ent_num == 256)
      return -ENOENT;
    blknum = directory[ent_num].start;
  }
  if(directory[ent_num].isDir)
    return -EISDIR;
  if(offset >= directory[ent_num].length)
    return 0;
   // when len less than the remaining data in the file
  if(len <= directory[ent_num].length- offset)
  {
    for(i =0; i<=((int)(len+offset)/super->blk_size);i++)
    {
      memset(databuf,0,1024);
      disk->ops->read(disk,blk_offset(i,blknum)*2,2,databuf);
      strcpy(datapool+strlen(datapool),databuf);
    }
    datapool+=offset;
    strncpy(buf,datapool,len);
    datapool-=offset;
    return len;
  }
  else
  {
    for(i =0;i<=(int)((directory[ent_num].length)/super->blk_size);i++)

    {
      memset(databuf,0,1024);
      disk->ops->read(disk,blk_offset(i,blknum)*2,2,databuf);
      strcpy(datapool+strlen(datapool),databuf);
    }
    datapool+=offset;
    strncpy(buf,datapool,(directory[ent_num].length -offset));
    datapool-=offset;
    return (directory[ent_num].length - offset);
  }  
}


unsigned int blk_last(unsigned int blknum)
{
  while(!fat[blknum].eof) // Has next block
   blknum = fat[blknum].next;
  return blknum;
}
/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 */
// The fuse_file_info *fi could be ignored.
static int hw3_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
  char *cpath = calloc(1,(strlen(path)+1));
  char *x_buf = calloc(1,1024);
  char *local_buf = calloc(1,(strlen(buf)+1));
  strcpy(cpath,path);
  char name[44];
  unsigned int blknum = super->root_dirent.start;
  unsigned int last_blk_num;
  struct cs5600fs_dirent directory[16];
  int ent_num,pages;
  int i;
  int free_block;
  memcpy(local_buf,buf,len);
  
 
  while(strlen(cpath)!=0)
  {
   memset(name,0,strlen(name));
   cpath = strwrd(cpath,name,44,"/");
   disk->ops->read(disk,blknum*2,2,(void*)directory);
   ent_num = find_entry(directory,name,&blknum);
   if (ent_num == 256)
    return -ENOENT;
   if(strlen(cpath))
    blknum = directory[ent_num].start;
  }
  if (directory[ent_num].isDir)
   return -EISDIR;
 /************ BEGIN WRITING ***********/
 if (offset > directory[ent_num].length)
  return -EINVAL;
 last_blk_num = blk_last(directory[ent_num].start);
// file_start_blk = directory[ent_num].start;
// last_blk_num = blk_last(file_start_blk); 
// disk->ops->read(disk,last_blk_num*2,2,(void*)x_buf);
if(((offset % FS_BLOCK_SIZE) + len) <= 1024)
  {
   memset(x_buf,0,strlen(x_buf));
   directory[ent_num].length += len;
   directory[ent_num].mtime = time(NULL);
   disk->ops->write(disk,blknum*2,2,(void*)directory);
   disk->ops->read(disk,last_blk_num*2,2,(void*)x_buf);
   memcpy((x_buf+strlen(x_buf)),buf,len);
   disk->ops->write(disk,last_blk_num*2,2,(void*)x_buf);
   memset((void*)buf,0,strlen(buf));
   return len;
  }
  else //create new block
  {
   memset(x_buf,0,strlen(x_buf));
   directory[ent_num].length +=len;
   directory[ent_num].mtime = time(NULL);
   disk->ops->write(disk,blknum*2,2,(void*)directory);
   disk->ops->read(disk,last_blk_num*2,2,(void*)x_buf);
   memcpy((x_buf+strlen(x_buf)),local_buf,(FS_BLOCK_SIZE-(offset%FS_BLOCK_SIZE)));
   

   disk->ops->write(disk,last_blk_num*2,2,(void*)x_buf);
   local_buf +=(FS_BLOCK_SIZE - (offset%FS_BLOCK_SIZE));

//  if( ((offset % FS_BLOCK_SIZE) + len) % FS_BLOCK_SIZE >0)
   //  {
    //  pages = ((offset % FS_BLOCK_SIZE) + len) / FS_BLOCK_SIZE +1;
  //   }
//   else
      pages = ((offset % FS_BLOCK_SIZE) + len) / FS_BLOCK_SIZE;

   for(i = 0; i < pages;i++)
    {
     memset(x_buf,0,strlen(x_buf));
     free_block = find_free_block();
     fat[last_blk_num].eof = 0;
     fat[last_blk_num].next = free_block;
     last_blk_num = free_block;
     
//     if(i!= (((offset+len)/FS_BLOCK_SIZE) - blk_offset_num - 1))
      memcpy(x_buf,local_buf,FS_BLOCK_SIZE);
      local_buf += FS_BLOCK_SIZE;
      disk->ops->write(disk,last_blk_num*2,2,(void*)x_buf);
      disk->ops->write(disk,2,8,(void *)fat);
     }
     return len;
  }
     return 0;


}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
int count_free_blk();
static int hw3_statfs(const char *path, struct statvfs *st)
{
  st->f_namemax = sizeof(super->root_dirent.name)-1;
  st->f_bsize = super->blk_size;
  st->f_blocks = 1019;
  st->f_bfree = count_free_blk();
  st->f_bavail = st->f_bfree;
  st->f_frsize = 0;
  st->f_files =0;
  st->f_ffree =0;
  st->f_fsid = 0;
  st->f_flag = 0;
  
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
  return 0;
}

//count free blocks in the disk
int count_free_blk()
{
 int i;
 int counter =0;
 for(i=0;i<1024;i++)
 {
   if(fat[i].inUse !=1)
   counter ++;
 } 
  return counter;
}
/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'hw3_ops'.
 */
struct fuse_operations hw3_ops = {
    .init = hw3_init,
    .getattr = hw3_getattr,
    .readdir = hw3_readdir,
    .create = hw3_create,
    .mkdir = hw3_mkdir,
    .unlink = hw3_unlink,
    .rmdir = hw3_rmdir,
    .rename = hw3_rename,
    .chmod = hw3_chmod,
    .utime = hw3_utime,
    .truncate = hw3_truncate,
    .read = hw3_read,
    .write = hw3_write,
    .statfs = hw3_statfs,
};

