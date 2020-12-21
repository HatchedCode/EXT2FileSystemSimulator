#include <ext2fs/ext2_fs.h>

/*************** type.h file ************************/
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc  GD;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;

SUPER *sp;
GD    *gp;
INODE *ip;
DIR   *dp;   

#define FREE        0
#define READY       1

#define BLKSIZE  1024
#define NFS_MOUNT  64
#define NMINODE    64
#define NFD         8
#define NPROC       2
#define DIR_TYPE 0x41ED
#define REGULAR_FILE_TYPE 0x81A4
#define SUPER_MAGIC       0xEF53
#define SUPER_USER        0

typedef struct minode{
  INODE INODE;
  int dev, ino;
  int refCount;
  int dirty;
  // for level-3
  int mounted;
  struct MOUNT *mptr;
}MINODE;


typedef struct oft{ // for level-2
  int  mode;
  int  refCount;
  MINODE *mptr;
  int  offset;
}OFT;

// Mount Table structure
typedef struct Mount{
  int    dev;   
  int    ninodes;
  int    nblocks;
  int    imap, bmap, iblk; 
  struct minode *mounted_inode;
  char   name[256]; 
  char   mount_name[64];
} MOUNT;

typedef struct proc{
  struct proc *next;
  int          pid;
  int          uid;
  int          status;
  MINODE      *cwd;
  OFT         *fd[NFD];
}PROC;
