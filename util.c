/*********** util.c file ****************/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;

extern char   gpath[256];
extern char   *name[64];
extern int    n;

extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256];

extern int prev_fs;

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   

int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
} 

int tst_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  if (buf[i] & (1 << j))
     return 1;
  return 0;
}

int set_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] |= (1 << j);
}

int clr_bit(char *buf, int bit)
{
  int i, j;
  i = bit/8; j=bit%8;
  buf[i] &= ~(1 << j);
}


int decFreeInodes(int dev)
{
  char buf[BLKSIZE];
  // dec free inodes count by 1 in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count--;
  put_block(dev, 2, buf);
}

int decFreeBlocks(int dev)
{
  char buf[BLKSIZE];
  // dec free inodes count by 1 in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count--;
  put_block(dev, 2, buf);
}

int incFreeInodes(int dev)
{
  char buf[BLKSIZE];
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buf);
  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buf);
}

int incFreeBlocks(int dev)
{
  char buf[BLKSIZE];
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_blocks_count++;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count++;
  put_block(dev, 2, buf);
}

int ialloc(int dev)  // allocate an inode number
{
  int  i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, imap, buf);

  for (i=0; i < ninodes; i++){
    if (tst_bit(buf, i)==0){
       set_bit(buf,i);
       put_block(dev, imap, buf);
       decFreeInodes(dev); //here dev might need to be changed.
       return i+1;
    }
  }
  return 0;
}

// Page 338 in the book
int idealloc(int dev, int ino)
{
  if(should_print)
  {
    printf("INSIDE IDEALLOC\n");
  }
  int i;
  char buf[BLKSIZE];

  if (ino > ninodes)
  {
    printf("I: inumber %d out of range\n", ino);
    return;
  }

  get_block(dev, imap, buf);
  clr_bit(buf, ino - 1);
  put_block(dev, imap, buf);
  incFreeInodes(dev);
}

//CREATED THIS FUNCTION 3:51, Friday Oct 25
// returns a FREE disk block number
int balloc(int dev)
{
  int i;
  char buf[BLKSIZE];

  get_block(dev, bmap, buf);

  for(i =0; i < nblocks; i++)
  {
    if(tst_bit(buf, i) == 0)
    {
      set_bit(buf, i);
      put_block(dev, bmap, buf);
      decFreeBlocks(dev);
      return i+1;
    }
  }

  return 0; // There are no blocks that are free.
}

int bdealloc(int dev, int ino)
{
  if(should_print)
  {
    printf("INSIDE BDEALLOC\n");
  }
  int i;
  char buf[BLKSIZE];
  // MTABLE *mp = (MTABLE *)get_mtable(dev);


  if (ino > nblocks)
  {
    printf("B: inumber %d out of range\n", ino);
    return;
  }

  get_block(dev, bmap, buf);
  clr_bit(buf, ino - 1);
  put_block(dev, bmap, buf);
  if(should_print)
  {
    printf("BEFORE INCFREEBLOCKS\n");
  }
  incFreeBlocks(dev);

  if(should_print)
  {
    printf("AFTER INCFREEBLOCKS\n");
  }
}

int tokenize(char *pathname)
{
  // tokenize pathname in GLOBAL gpath[]; pointer by name[i]; n tokens
  char *s;
  strcpy(gpath, pathname);
  n = 0;
  s = strtok(pathname, "/");
  while(s){
    name[n++] = s;
    s = strtok(0, "/");
  }
}

int clear()
{
  #if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
      system("clear");
  #endif

  #if defined(_WIN32) || defined(_WIN64)
      system("cls");
  #endif

  return 1;
}

int getMode()
{
    char line[256];
    do
    {
        printf("Enter the file MODE (R | W | RW | APPEND) or exit to return to main menu: ");
        fgets(line, 128, stdin);
        line[strlen(line)-1] = 0;
        if (line[0]==0)
            continue;

        if(!strcmp(line, "exit"))
        {
            return -1;
        }

        if(!strcmp(line, "R"))
        {
            return 0;
            break;
        }

        if(!strcmp(line, "W"))
        {
            return 1;
            break;
        }

        if(!strcmp(line, "RW"))
        {
            return 2;
            break;
        }

        if(!strcmp(line, "APPEND"))
        {
            return 3;
            break;
        }

        printf("open_file(): Invalid Mode.\n");
    } while (1);

    return -1;
}

int truncate(INODE *ip)
{
  int i;

  for (i=0; i<12; i++)
  {
    if (ip->i_block[i] == 0)
    {
      break;
    }
    bdealloc(dev, ip->i_block[i]);

  }
  return 0;
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, disp;
  INODE *ip;

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];

    if (mip->refCount && mip->dev == dev && mip->ino == ino)
    {
       mip->refCount++;
       if(should_print)
       {
         printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       }
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
      //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino to buf    
       blk  = (ino-1) / 8 + inode_start;
       disp = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d disp=%d\n", ino, blk, disp);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + disp;
       // copy INODE to mp->INODE
       mip->INODE = *ip;

       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

int iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip==0) 
     return;

 mip->refCount--;
 
 if (mip->refCount > 0) return;
 if (!mip->dirty)       return;
 
 /* write back */
 if(should_print)
 {
  printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino); 
 }

 block =  ((mip->ino - 1) / 8) + inode_start;
 offset =  (mip->ino - 1) % 8;

 /* first get the block containing this inode */
 get_block(mip->dev, block, buf);

  // printf("iput(): dev=%d    block=%d\n", mip->dev, block);
  // printf("iput(): buf is\n");
  // printf("%s\n", buf);
 ip = (INODE *)buf + offset;
 *ip = mip->INODE;

 put_block(mip->dev, block, buf);

} 

int search(MINODE *mip, char *name)
{
  // YOUR serach() fucntion as in LAB 5
  int i;
  char *cp, temp[256], sbuf[BLKSIZE];
  DIR *dp;

  // search DIR direct blocks only  
  for (i=0; i<12; i++)
  {
    if (mip->INODE.i_block[i] == 0)
    {
      return 0;
    }
    get_block(mip->dev, mip->INODE.i_block[i], sbuf);
    dp = (DIR *)sbuf;
    cp = sbuf;
    while (cp < sbuf + BLKSIZE)
    {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;
      if(should_print)
      {
        printf("ino=%8d   rec_len=%8d  name_len=%8u   \nname=%s\n",
        dp->inode, dp->rec_len, dp->name_len, temp);
      }
      
      if(should_print)
      {
        printf("search(): name:\n%s\n\n\n\n", name);
        printf("temp=\n%s\n\n\n\n\n", temp);
      }

      if (strcmp(name, temp)==0)
      {
        if(should_print)
        {
          printf("found %s : inumber = %d\n", name, dp->inode);
        }

        // printf("search(): ino returned=%d\n", dp->inode);
        return dp->inode;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
     }
  }

  // printf("search(): ino returned=%d\n", 0);
  return 0;
}


int get_myname(MINODE *parent_minode, int my_ino, char *my_name)
{
  char dbuf[BLKSIZE];
  INODE *ip = &(parent_minode->INODE);
  DIR* dp;
  dp = (DIR *)dbuf;
  char *cp, temp[256];
  cp = dbuf;

  if(root->ino == my_ino)
  {
    strcpy(my_name, "/");
  }

  if(should_print)
  {
    printf("Searching for ino:   %d\n", my_ino);
  }

  int i;
  for(i =0; i < 12; i++)
  {
    if(!ip->i_block[i])
    {
      return 1;
    }

    get_block(dev, ip->i_block[i], dbuf);
    while(cp < dbuf + 1024)
    {
      if(should_print)
      {
        printf("ino: %d     dp->inode:  %d\n", my_ino, dp->inode);
      }

      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;

      if(should_print)
      {
          printf("%8d%8d%8u   %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
      }
      
      if(my_ino == dp->inode)
      {
        strncpy(my_name, temp, strlen(temp));
        my_name[strlen(temp)] = 0;

        if(should_print)
        {
          printf("my_name = %s\n", my_name);
        }
        
        return 1;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }    
  }
}

int getino(char *pathname, int *tdev)
{
  int i, ino, blk, disp;
  INODE *ip;
  MINODE *mip;

  if(should_print)
  {
    printf("getino: pathname=%s\n", pathname);
  }

  if (strcmp(pathname, "/")==0)
      return 2;

  if (pathname[0]=='/')
    //mip = root;
    mip = iget(dev, 2);
  else
    mip = iget(running->cwd->dev, running->cwd->ino);
    //mip = running->cwd;
  // mip->refCount++;

  tokenize(pathname);

  for (i=0; i<n; i++){
      // printf("getino():  mip->ino=%d  name[%d]=%s   mip->cur_dev=%d and prev_fs->dev=%d\n", mip->ino, i, name[i], mip->dev, prev_fs);
      // printf("getino(): loop top: name=%s index=%d n=%d ino=%d dev=%d\n", name[i], i, n, mip->ino, *tdev);
      // printf("getino(): cur dev=%d\n", *tdev);

      // if(should_print)
      // {
      //   printf("===========================================\n");
      // }
      if (mip->INODE.i_mode != DIR_TYPE)
      {
        printf("getino(): %s is not a dir\n", name[i]);
        iput(mip);
        return 0;
      }

      // printf("getino(): name[%d]=%s   mip->cur_dev=%d and prev_fs->dev=%d\n", i, name[i], mip->dev, prev_fs);

      if(strcmp(name[i], ".."))
      {
        // printf("getino(): in the if\n");
        // printf("getino():  mip->ino=%d   name[%d]=%s   mip->cur_dev=%d and prev_fs->dev=%d\n", mip->ino, i, name[i], mip->dev, prev_fs);
        if(mip->mounted == 1)
        {
          MOUNT *mtptr;
          MINODE *mptr;

          mtptr = mip->mptr;

          prev_fs = mip->dev;

          //Getting the root inode for the filesystem
          mptr = iget(mtptr->dev, 2);
          mip = mptr; //Not sure if this should be here.
          *tdev = mptr->dev;
          root = mptr;
          // newdev = n > 1 ? mptr->dev : *tdev;
          // printf("getino(): changing FS, new dev=%d\n", *tdev);
        }
      }
      else
      {
        // printf("getino(): in the else\n");
        // printf("getino(): mip->ino=%d   name[%d]=%s   mip->cur_dev=%d and prev_fs->dev=%d\n", mip->ino, i, name[i], mip->dev, prev_fs);
        // printf("getino(): ARE WE GOING TO GO BACK UP THE TREE\n");
        if(mip->ino == 2 && mip->dev != prev_fs)
        {
          // printf("getino(): WE ARE GOING BACK UP THE TREE \n");
          MOUNT *mtptr;
          MINODE *mptr;

          int temp_preFSdev = 0;

          temp_preFSdev = prev_fs;

          // printf("getino(): before iget\n");
          // printf("mountTable[0]->dev=%d  prev_fs=%d\n", mountTable[0].dev, temp_preFSdev);

          int index;
          for(index=0; index< NFS_MOUNT && mountTable[index].mounted_inode; index++)
          {
            if(mountTable[index].dev == prev_fs)
            {
              mtptr = &(mountTable[index]);
              mip = mountTable[index].mounted_inode;
              mptr = iget(prev_fs, 2);

              // printf("mip->dev =%d mt_mptr->dev=%d  mptr->dev=%d  mptr->ino =%d mip->ino=%d \n", mip->dev, mtptr->dev, mptr->dev, mptr->ino, mip->ino);
              // if(mip == mtptr->mounted_inode)
              // {
              //   printf("mounted_inode and new inode are the same\n");
              // }
              // else if(mptr == mip)
              // {
              //   printf("mip and mptr are the same\n");
              // }

              // printf("mip->dev=%d  mtptr->dev=%d mount_name=%s  name=%s\n", mip->dev, mtptr->dev, mtptr->mount_name, mtptr->name);
              // printf("in for loop about to break;");
              break;
            }
          }
          
          // mip =  iget(temp_preFSdev, 2);
          // printf("getino(): after finding mount-table\n");

          //root = mip;
          if(mip->mounted)
          {
            prev_fs = mip->dev;
            // printf("mptr->dev=%d mip->cur_dev=%d and prev_fs->dev=%d\n", mtptr->dev, mip->dev, prev_fs);
            // printf("new pre_fs =%d\n", prev_fs);
          }

          if(mip->mounted == 1)
          {
            root = mtptr->mounted_inode;
          }

          *tdev = mptr->dev;
          // printf("mptr->dev=%d mip->cur_dev=%d and prev_fs->dev=%d\n", mtptr->dev, mip->dev, prev_fs);

          // printf("getino(): new dev=%d\n", *tdev);
          //continue;
        }
      }

      ino = search(mip, name[i]);

      if (ino==0)
      {
         iput(mip);

         if(should_print)
         {
            printf("name %s does not exist\n", name[i]);         
         }
         return 0;
      }

      // if(strcmp(name[i], ".."))
      // {
      //   if(mip->mounted == 1)
      //   {
      //     root = iget(dev)
      //   }
      // }
      // else
      // {
      //   if(mip->ino == 2 && mip->dev != mountTable[0].dev)
      //   {

      //   }
      // }     

      //iput(mip);
      mip = iget(*tdev, ino);
   }

  printf("getino(): loop top: ino=%d mip->ino=%d dev=%d\n",ino, mip->ino, *tdev);
  printf("getino(): cur dev=%d\n", *tdev);

   iput(mip);
   return ino;
}

// Same thing as getmyname function that starts at 284
// int findmyname(MINODE *parent, u32 myino, char myname[ ]) 
// {
//   // find mynio in parent data block; copy name string to myname[ ];
// }

int findino(MINODE *mip, u32 *myino) // return ino of parent and myino of .
{
  char buf[BLKSIZE], *cp;   
  DIR *dp;

  get_block(mip->dev, mip->INODE.i_block[0], buf);
  cp = buf; 
  dp = (DIR *)buf;
  *myino = dp->inode;
  cp += dp->rec_len;
  dp = (DIR *)cp;
  return dp->inode;
}

char *read_link(char *path)
{
  // int dev;

  // dev = path[0] == '/' ? root->dev : running->cwd->dev;
  printf("read_link(): pathname=%s     dev=%d\n", path, dev);

  int ino = getino(path, &dev);
  MINODE *mip = iget(dev, ino);

  if(mip->INODE.i_mode != 0xA1FF)
  {
    return 0;
  }

  return mip->INODE.i_block;
}