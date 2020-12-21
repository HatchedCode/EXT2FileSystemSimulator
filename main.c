/****************************************************************************
*                   ODB testing ext2 file system                            *
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <sys/stat.h>
#include <ext2fs/ext2_fs.h>

#include "type.h"

MINODE minode[NMINODE];
MINODE *root;
MOUNT mountTable[NFS_MOUNT];
PROC   proc[NPROC], *running;

char   gpath[256]; // global for tokenized components
char   *name[64];  // assume at most 64 components in pathname
int    n;          // number of component strings
int should_print = 0;
int prev_fs = 0;

int    fd, dev;
int    nblocks, ninodes, bmap, imap, inode_start;
char   line[256], cmd[32], pathname[256];

char *t1 = "xwrxwrxwr-------";
char *t2 = "----------------";

#include "util.c"
#include "cd_ls_pwd.c"
#include "level1.c"
#include "level2.c"
#include "level3.c"


int init()
{
  int i, j;
  MINODE *mip;
  PROC   *p;

  printf("init()\n");

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    mip->mptr = 0;
  }
  for (i=0; i<NPROC; i++){
    p = &proc[i];
    p->pid = i;
    p->uid = 0;
    p->cwd = 0;
    p->status = FREE;
    for (j=0; j<NFD; j++)
      p->fd[j] = 0;
  }

  //Set the mounted fs table devices to 0
  for(i = 0; i < NFS_MOUNT; i++)
  {
    mountTable[i].dev = 0;
    mountTable[i].bmap = 0;
    mountTable[i].imap = 0;
    mountTable[i].mounted_inode = 0;
    mountTable[i].ninodes = 0;
    mountTable[i].nblocks = 0;
    mountTable[i].iblk = 0;
    strcpy(mountTable[i].mount_name,"\0");
    strcpy(mountTable[i].name,"\0");
  }

  //Proc 0 set to zero and the cwd is set to point to CWD.
  proc[0].uid = 0;
  proc[0].cwd = 0;

  //Proc 1 is set to 1 and the CWD is set to root miNOde
  proc[1].cwd = root;

  //all minode refCount are set to 0
  for(i = 0; i < 64; i++)
  {
    minode[i].refCount = 0;
  }  

  // root MINOde is set to 0.
  root = 0;

}

// load root INODE and set root pointer to it 
// mount root file system, establish / and CWDs
int mount_root(char *disk)
{  
  char buf[BLKSIZE];
  printf("\nmounting root .... \n");

  printf("checking EXT2 FS ....");
  if ((fd = open(disk, O_RDWR)) < 0){
    printf("open %s failed\n", disk);  exit(1);
  }
  dev = fd;
  /********** read super block at 1024 ****************/
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;

  /* verify it's an ext2 file system *****************/
  if (sp->s_magic != 0xEF53){
      printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
      exit(1);
  }     
  printf("OK\n");
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;

  get_block(dev, 2, buf); 
  gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  inode_start = gp->bg_inode_table;
  printf("bmp=%d imap=%d inode_start = %d\n", bmap, imap, inode_start);


  root = iget(dev, 2);

  proc[0].cwd = iget(dev, 2);
  proc[1].cwd = iget(dev, 2);

  running = &proc[0];

  root->mounted = 1;
  root->mptr = &(mountTable[0]);
  mountTable[0].dev = dev;
  mountTable[0].bmap = bmap;
  mountTable[0].imap = imap;
  mountTable[0].iblk = inode_start;
  mountTable[0].nblocks = nblocks;
  mountTable[0].ninodes = ninodes;
  mountTable[0].mounted_inode = root;
  strcpy(mountTable[0].name,disk);
  strcpy(mountTable[0].mount_name,"/");


  printf("\nmounting root successful.... \n");
}

char *disk = "mydisk";
int main(int argc, char *argv[ ])
{
  char oldlink[256], newlink[256];
  int ino;
  char buf[BLKSIZE];
  if (argc > 1)
    disk = argv[1];

  init();  
  mount_root(disk);

  printf("root refCount = %d\n", root->refCount);
  
  printf("creating P0 as running process\n");
  running = &proc[0];
  running->status = READY;
  running->cwd = iget(dev, 2);
  
  printf("root refCount = %d\n", root->refCount);

  while(1){
    printf("input command : [ls|cd|pwd|mkdir|creat|rmdir|link|unlink|symlink|open|close|lseek|pfd|read|cat|write|cp|mv|mount|umount|clear|quit| ");
    fgets(line, 128, stdin);
    line[strlen(line)-1] = 0;
    if (line[0]==0)
      continue;
    pathname[0] = 0;
    newlink[0]=0;
    cmd[0] = 0;
    
    sscanf(line, "%s %s %s", cmd, pathname, newlink);

    if(( !strcmp(cmd, "cp") || !strcmp(cmd, "mv") || !strcmp(cmd, "link") || !strcmp(cmd, "symlink")) && newlink != NULL)
    {
      printf("cmd=%s oldfile=%s newfile=%s\n", cmd, pathname, newlink);
    }
    else if(!strcmp(cmd, "mount"))
    {
      printf("cmd=%s filesystem-name=%s mount_point=%s\n", cmd, pathname, newlink);
    }
    else
    {
      printf("cmd=%s pathname=%s\n", cmd, pathname);
    }

    should_print = 0;
  
    if(strcmp(cmd, "ls")==0)
       list_file();
    if(strcmp(cmd, "cd")==0)
       change_dir();
    if(strcmp(cmd, "pwd")==0)
    {
      pwd(running->cwd);
    }
    if(strcmp(cmd, "mkdir") == 0)
       make_dir();
    if(strcmp(cmd, "creat") == 0)
       creat_file();
    if(strcmp(cmd, "rmdir") == 0)
    {
      if(strcmp(pathname, ".") || strcmp(pathname, "..") || strcmp(pathname, "/") || strlen(pathname) < 1)
      {
        rmdir();
      }
      else
      {
        printf("Invalid pathname...\n");
      }
      
    }
    if(strcmp(cmd, "link") == 0)
    {
      strcpy(oldlink, pathname);
      link(oldlink, newlink);
    }
    if(strcmp(cmd, "unlink") == 0)
    {
      unlink(pathname);
    }
    if(strcmp(cmd, "symlink") == 0)
    {
      strcpy(oldlink, pathname);
      symlink(oldlink, newlink);
    }
    if(strcmp(cmd, "open") == 0)
    {
      int mode = -1;
      mode = getMode();
      open_file(mode);
    }
    if(strcmp(cmd, "close") == 0)
    {
      int file_descriptor = 0;
      while(1){
        printf("Please enter the file descriptor: ");
        fgets(line, 128, stdin);
        line[strlen(line)-1] = 0;
        if (line[0]==0)
          continue;
        sscanf(line, "%d", &file_descriptor);
        if(file_descriptor < 0)
          continue;
        break;
      }
            
      close_file(file_descriptor);
    }
    if(strcmp(cmd, "lseek") == 0)
    {
      int file_descriptor = 0, position = 0, cont = 0;
      while(1){
        printf("Please enter the file descriptor followed by the position or type exit to return to main menu: ");
        fgets(line, 128, stdin);
        line[strlen(line)-1] = 0;
        if (line[0]==0)
          continue;
        if(!strcmp(line, "exit"))
          break;
        sscanf(line, "%d %d", &file_descriptor, &position);
        if(file_descriptor < 0 || position < 0)
          continue;
        cont = 1;
        break;
      }

      if(cont == 1)
      {    
        lseek_level2(file_descriptor, position);
      }
    }
    if(strcmp(cmd, "pfd") == 0)
    {
      pfd();
    }
    if(strcmp(cmd, "read") == 0)
    {
      read_file();
    }
    if(strcmp(cmd, "cat") == 0)
    {
      cat_file();
    }
    if(strcmp(cmd, "write") == 0)
    {
      write_file();
    }
    if(strcmp(cmd, "cp") == 0)
    {
      cp_level2(pathname, newlink);
    }
    if(strcmp(cmd, "mv") == 0)
    {
      mv_level2(pathname, newlink);
    }
    if(strcmp(cmd, "mount") == 0)
    {
      mount(newlink);
    }
    if(strcmp(cmd, "umount") == 0)
    {
      umount(pathname);
    }    
    if(strcmp(cmd, "clear") == 0)
    {
      clear();
    }
    if(strcmp(cmd, "quit")==0)
       quit();
  }
}
 
int quit()
{
  int i;
  MINODE *mip;
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount > 0)
      iput(mip);
  }
  exit(0);
}
