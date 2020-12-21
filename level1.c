/**** level1.c ****/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256];
extern char *t1, *t2;

#define OWNER  000700
#define GROUP  000070
#define OTHER  000007

int make_dir()
{
    MINODE *start, *pip;
    char *parent, *child;
    int pino;
    char temp_pathname[BLKSIZE];

    strcpy(temp_pathname, pathname);
    if(!temp_pathname)
    {
        printf("make_dir():  pathname empty\n");
        return 0;
    }

    if(temp_pathname[0] == '/')
    {
        if(should_print)
        {
            printf("pathname contains /\n");
        }

        start = root;
        dev = root->dev;
    }
    else
    {
        start = running->cwd;
        dev = running->cwd->dev;
    }

    char temp_basename[BLKSIZE];
    strcpy(temp_basename, pathname);

    parent = dirname(temp_pathname);
    child = basename(temp_basename); 

    printf("make_dir(): pathname=%s     dev=%d\n", parent, dev);
    pino = getino(parent, &dev);

    pip =  iget(dev, pino);

    if(should_print)
    {
        printf("make_dir(): child = %s\n", child);
    }

    int cino = search(pip, child);

    if(pip->INODE.i_mode != DIR_TYPE)
    {
        printf("make_dir() : parent is not a directory\n");
        return 0;
    } 

    if(cino)
    {
        printf("make_dir() : child inode number is not zero, child exists.\n"); 
        return 0;
    }
    
    mymkdir(pip, child);
    pip->INODE.i_links_count +=1;
    pip->INODE.i_atime = time(0L);
    pip->dirty = 1;

    iput(pip);
}

int mymkdir(MINODE *pip, char *name) //name is the basename
{
    printf("current cwd is: dev=%d  ino=%d\n", running->cwd->dev, running->cwd->ino);
    int ino, blk, i;
    MINODE *mip;
    char buf[BLKSIZE], *cp;

    //We first allocate an inode and disk block.
    ino = ialloc(dev);

    blk = balloc(dev);

    mip = iget(dev, ino); //Load INODE into a minode.

    INODE *ip = &mip->INODE;
    ip->i_mode = 0x41ED;
    ip->i_uid = running->uid;
    ip->i_gid = running->pid; //this is suppose to be the group id, but not sure if this is correct.
    ip->i_size = BLKSIZE;
    ip->i_links_count = 2;
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    ip->i_block[0] = blk;

    for(i = 1; i<15; i++)
    {
        mip->INODE.i_block[i] = 0;
    }

    mip->dirty = 1;
    iput(mip);

    get_block(dev, ip->i_block[0], buf);
    dp = (DIR *)buf;
    cp = buf;

    dp->inode = mip->ino;
    dp->name_len = 1;
    dp->rec_len = 12;
    strncpy(dp->name, ".", dp->name_len);

    //Going to the next block
    cp += dp->rec_len;
    dp = (DIR *)cp;

    dp->inode = pip->ino;
    dp->name_len = 2;
    dp->rec_len = 1012;
    strncpy(dp->name, "..", dp->name_len);

    //Write to disk block blk
    put_block(dev, blk, buf);

    enter_name(pip, ino, name);
    printf("current cwd is: dev=%d  ino=%d\n", running->cwd->dev, running->cwd->ino);
}

int enter_name(MINODE *pip, int myino, char *name)
{
    int i, IDEAL_LEN, remain, need_length;
    char buf[BLKSIZE], *cp;

    for(i = 0; i < 12; i++)
    {
        if(pip->INODE.i_block[i] == 0)
        {
            break; //Empty block
        }

        // Step 3
        need_length = 4*( (8 + strlen(name) + 3)/4 );  // a multiple of 4

        if(should_print)
        {
            printf("enter_name: need_length = %d\n", need_length);
        }

        //Step 1.)
        get_block(dev, pip->INODE.i_block[i], buf);
        DIR *dp = (DIR *)buf;
        cp = buf;
        
        // Step 4.)
        if(should_print)
        {
            printf("enter_name: rec_len = %d\n", dp->rec_len);
        }

        while(cp + dp->rec_len < (buf + BLKSIZE))
        {
            if(should_print)
            {
                printf("type: %d\t  rec_len:  %d\t  name_len:  %d\t  name:  %s\n", dp->file_type, dp->rec_len, dp->name_len, dp->name);
            }
            
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }

        // Step 2.)

        if(should_print)
        {
            printf("enter_name: rec_len = %d\n", dp->rec_len);
        }

        IDEAL_LEN = 4 *( (8 + dp->name_len + 3) / 4 );
        
        if(should_print)
        {
            printf("enter_name: ideal = %d\n", IDEAL_LEN);
        }
        remain = dp->rec_len - IDEAL_LEN;
        
        if(should_print)
        {
            printf("enter_name: remain = %d\n", remain);
        }


        if(remain >= need_length)
        {
            //enter the new entry as the LAST entry and
            //trim the previous entry rec_len to its ideal_length

            //Trimming to the ideal length
            dp->rec_len = IDEAL_LEN;

            //Entering the new entry as the last entry
            //Iterating to the next avaiable spot
            cp += dp->rec_len;
            dp = (DIR *)cp;

            // Setting the inode number to the one passed into the function
            dp->inode = myino;

            // Setting the new rec_len to what remained
            dp->rec_len = remain;

            //Assigning the name_len to the length of the name passed in
            dp->name_len = strlen(name);

            //Copying the name passed in as the entries name
            strncpy(dp->name, name, dp->name_len);

            //Step 6.)
            //Write data block to disk
            put_block(dev, pip->INODE.i_block[i], buf);

            return i;
        }
    }

    //Step 5.)
    // If no space in existing data blocks
    // Allocate a new data block; increment parent size by BLKSIZE
    // Enter new entry as the first entry in the new data block
    // with rec_len = BLKSIZE

    int newblock = 0;

    //Allocating a new data block
    newblock = balloc(dev);
    pip->INODE.i_block[i] = newblock;

    //Increment parent size by BLKSIZE;
    pip->INODE.i_size += BLKSIZE;

    //Need to get the next block
    get_block(dev, pip->INODE.i_block[i], buf);
    dp = (DIR *)buf;
    cp = buf;

    // Enter the new block details
    dp->inode = myino;
    dp->rec_len = BLKSIZE;
    dp->name_len = strlen(name);
    strncpy(dp->name, name, dp->name_len);

    //Step 6.)
    //Write data block to disk
    put_block(dev, pip->INODE.i_block[i], buf);
    return newblock;
}

int creat_file()
{
    MINODE *start, *pip;
    char *parent, *child;
    int pino;
    char temp_pathname[BLKSIZE];

    strcpy(temp_pathname, pathname);
    if(!temp_pathname)
    {
        printf("creat_file():  pathname empty\n");
        return 0;
    }

    if(temp_pathname[0] == '/')
    {
        if(should_print)
        {
            printf("pathname contains /\n");
        }

        start = root;
        dev = root->dev;
    }
    else
    {
        start = running->cwd;
        dev = running->cwd->dev;
    }

    char temp_basename[BLKSIZE];
    strcpy(temp_basename, pathname);

    parent = dirname(temp_pathname);
    child = basename(temp_basename); 

    printf("creat_dir(): pathname=%s     dev=%d\n", parent, dev);
    pino = getino(parent, &dev);

    pip =  iget(dev, pino);

    int cino = search(pip, child);

    if(pip->INODE.i_mode != DIR_TYPE)
    {
        printf("creat_file() : parent is not a directory file\n");
        return 0;
    }

    if(cino)
    {
        printf("creat_file() : child inode number is not zero, child/file exists.\n");
        return 0;
    }
    
    //We do not increment parent's link count when creating REG files
    int new_ino = 0;
    new_ino = my_creat(pip, child);
    pip->dirty = 1; //set the parent inode/directory to dirty.

    iput(pip);

   return new_ino;
}


int my_creat(MINODE *pip, char *name)
{
    int ino, blk, i;
    MINODE *mip;
    char buf[BLKSIZE];

    //We first allocate an inode and disk block.
    ino = ialloc(dev);

    if(should_print)
    {
        printf("my_creat(): inode number = %d\n", ino);
    }

    blk = balloc(dev);

    if(should_print)
    {
        printf("my_creat(): blk number = %d\n", blk);
    }

    mip = iget(dev, ino); //Load INODE into a minode.

    INODE *ip = &mip->INODE;
    ip->i_mode = REGULAR_FILE_TYPE; //set the permissions to rw-r-r-- for a reg file.
    ip->i_uid = running->uid;
    ip->i_gid = running->pid;
    ip->i_size = 0; //Since we do not have data blocks
    ip->i_links_count = 1;
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);
    mip->INODE.i_block[0] = blk;
    for(i = 1; i<15; i++)
    {
        mip->INODE.i_block[i] = 0;
    }

    mip->dirty = 1;
    iput(mip);

    printf("my_creat(): new file ino = %d  for filename=%s\n", ino, name);

    enter_name(pip, ino, name);

    return ino;
}

int rmdir()
{
    int ino, i, pino;
    MINODE *mip, *pip;
    char *parent, *child; 
    char temp_pathname[BLKSIZE];
    char temp_basename[BLKSIZE];

    if(!pathname || strlen(pathname) < 1)
    {
        printf("rmdir(): pathname empty\n");
        return 0;
    }

    if(pathname[0] == '/')
    {
        if(should_print)
        {
            printf("rmdir(): pathname contains /\n");
        }
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }

    strcpy(temp_basename, pathname);
    strcpy(temp_pathname, pathname);

    parent = dirname(temp_pathname);
    child = basename(temp_basename);

    if(!strcmp(parent, "."))
    {
        parent = "..";
    }

    if(should_print)
    {
        printf("parent=%s\tchild=%s\n", parent, child);
    }

    printf("rmdir(): pathname=%s     dev=%d\n", pathname, dev);
    ino = getino(pathname, &dev);

    if(!ino)
    {
        printf("rmdir(): ino number is zero.\n");
        return 0;
    }

    mip = iget(dev, ino);

    if(!mip)
    {
        printf("rmdir(): mip is null.\n");
        return 0;
    }

    // 4. check ownership 
    // super user : OK
    // not super user: uid must match
    if (running->uid == 0) // Super User (uid = 0)
    {
        if(should_print)
        {
            printf("Super User\n");
        }
    }
    else
    {
        // Need to make sure the uid matches
        if (running->uid == mip->INODE.i_uid)
        {
            printf("Not Super: There is a match...\n");
        }
        else
        {
            printf("Not Super: There is no match...\n");
            iput(mip);
            return 0;
        }
    }
    
    //verify that the INODE is a directory
    if(mip->INODE.i_mode != DIR_TYPE)
    {
        printf("rmdir(): INODE is not a directory.\n");
        iput(mip);
        return 0;
    }

    if(should_print)
    {
        printf("rmdir(): mip->refCount = %d\n", mip->refCount);
    }
    
    //Check that minode is not busy
    if(mip->refCount != 1)
    {
        printf("rmdir(): INODE is BUSY.\n");
        iput(mip);
        return 0;
    }

    if(should_print)
    {
        printf("rmdir(): mip->links_count = %d\n", mip->INODE.i_links_count);
    }
    //Verify DIR is empty
    if(mip->INODE.i_links_count > 2)
    {
        printf("rmdir(): DIR is not empty.\n");
        iput(mip);
        return 0;
    }

    if(should_print)
    {
        printf("rmdir(): right before checking files in dir.\n");
        printf("rmdir(): mip->links_count = %d\n", mip->INODE.i_links_count);
    }

    //May still need to check this.
    if(mip->INODE.i_links_count == 2)
    {
        //We are checking to see how many DIR enteries there are.
        char cp, buf[BLKSIZE], temp[BLKSIZE];
        int count_dir_entries;
        for(i = 0; i < 12; i++)
        {
            if(mip->INODE.i_block[i] == 0)
            {
                break; //Empty block   --> Might have to break here;
            }
    
            get_block(mip->dev, mip->INODE.i_block[i], buf);

            if(should_print)
            {
                printf("rmdir(): right after get block with index = %d.\n", i);
            }

            DIR* dp;
            dp = (DIR *)buf;
            cp = buf;
            
            if(should_print)
            {
                printf("cp: %d, buf: %d, dp_rec: %d\n", cp, buf, dp->rec_len);
            }

            while(cp < buf + 1024)
            {
                if(should_print)
                {
                    printf("cp: %d, buf: %d, dp_rec: %d\n", cp, buf, dp->rec_len);
                }

                strncpy(temp, dp->name, dp->name_len);

                temp[dp->name_len] = 0;

                if(strcmp(temp, ".") && strcmp(temp, ".."))
                {
                    printf("rmdir(): directory you are trying to remove is not empty.\n");
                    return 0;
                }
                count_dir_entries++;
                cp += dp->rec_len;
                dp = (DIR *)cp;
            }
        }

        if(count_dir_entries == 0)
        {
            if(should_print)
            {
                printf("rmdir(): Directory is empty, now you may delete.\n");                
            }
        }
    }

    if(should_print)
    {
        printf("rmdir(): right after checking files in dir.\n");
        printf("rmdir(): refCount = %d\n", mip->refCount);
    }

    /* get parent’s ino and inode */
    pino = search(mip, "..");
    
    if(!pino)
    {
        printf("rmdir(): parent inode number could not be found in mip with name: %s\n", parent);
        iput(mip);
        return 0;
    }

    if(should_print)
    {
        printf("rmdir(): parent ino=%d",pino);
    }
    pip = iget(mip->dev, pino);

    if(!pip)
    {
        iput(mip);
        printf("rmdir(): parent MINODE could not be retrieved.\n");
        return 0;
    }
    
    if(should_print)
    {
        printf("rmdir(): right before rm_child.\n");
        printf("rmdir(): parent ino=%d",pip->ino);
    }


    //remove child's entry from parent directory by. name should be changed to the name of the child name, in our case it should be the basename
    for (i=0; i<15; i++)
    {
        if (mip->INODE.i_block[i] == 0)
            break;
        bdealloc(dev, mip->INODE.i_block[i]);
    }
    idealloc(dev, ino);
    rm_child(pip, child);

    if(should_print)
    {
        printf("rmdir(): right after rm_child.\n");
    }

    pip->INODE.i_links_count -= 1;
    pip->INODE.i_mtime = pip->INODE.i_atime = time(0L); 
    pip->dirty = 1;

    if(should_print)
    {
        printf("rmdir(): right before deallocating.\n");
    }


    iput(pip);
    mip->dirty = 1;
    iput(mip); //clears mip->refCount = 0

    if(should_print)
    {
        printf("rmdir(): right after deallocating.\n");
    }

    return 1;
}


/*
    rm_child(): remove the entry [INO rlen nlen name] from parent's data block.
    pip->parent Minode, name = entry to remove
*/
int rm_child(MINODE *parent, char *name)
{
    int ino, i;
    char *cp, *lastcp, buf[BLKSIZE], temp[BLKSIZE];
    DIR* prev, *lastentry;

    //Search parent INODE's data block(s) for the entry of name
    if(should_print)
    {
        printf("\n");
        printf("rm_child(): child name = %s\n", name);
        printf("rm_child(): parent ino = %d\n",parent->ino);

        printf("rm_child(): Before search\n");
    }

    ino = search(parent, name);

    if(should_print)
    {
        printf("rm_child(): After search\n");
    }

    if(!ino)
    {
        printf("rm_child: child inode number is zero, could not find child.\n");
        return 0;
    }

    int add_to_last_rec = 0;
    int removed_rec_len = 0;

    if(should_print)
    {
        printf("rm_child(): Beginning block loop\n");
    }
    
    for(i = 0; i < 12; i++)
    {
        if(should_print)
        {
            printf("rm_child(): inside the loop\n");
        }
        if(parent->INODE.i_block[i] == 0)
        {
            printf("rm_child(): Inside the zero check\n");
            break; //Empty block
        }

        prev = 0;

        if(should_print)
        {
            printf("rm_child(): Before get_block\n");
        }
        get_block(dev, parent->INODE.i_block[i], buf);
        
        if(should_print)
        {
            printf("rm_child(): After get_block\n");
        }

        DIR* dp;
        dp = (DIR *)buf;
        cp = buf;
        lastcp = buf;

        while(cp < buf + BLKSIZE)
        {

            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;

            if(should_print)
            {
                printf("rm_child(): temp: %s, name: %s\n", temp, name);
            }

            if (!strcmp(temp, name)) // Need to make sure we find the name
            {
                if(should_print)
                {
                    printf("rm_child(): found the name\n");
                    printf("rm_child(): cp: %d, rec: %d, combo: %d buf: %d\n", cp, dp->rec_len, (cp+dp->rec_len), (buf + 1024));
                }

                if (prev != NULL)
                {
                    if(should_print)
                    {
                        printf("rm_child(): prev = %d\n", prev->rec_len);
                    }
                }
                if ((cp + dp->rec_len == buf + 1024) && prev == 0) // First and only entry
                {
                    if(should_print)
                    {
                        printf("rm_child(): Inside first and only\n");
                    }
                    bdealloc(dev, ino);

                    if(should_print)
                    {
                        printf("rm_child(): Before decrement\n");
                    }
                    parent->INODE.i_size -= BLKSIZE;

                    if(should_print)
                    {
                        printf("rm_child(): After decrement\n");
                    }
                    // Need to compact parent's i_block array to eliminate
                    // the deleted entry if it's between nonzero entries
                    
                    int index = i;

                    if(should_print)
                    {
                        printf("rm_child(): Reached the loop\n");
                    }

                    for (; index < 11; index++)
                    {
                        if(should_print)
                        {
                            printf("count: %d\n", index);
                        }
                        parent->INODE.i_block[index] = parent->INODE.i_block[index + 1];
                    }
                    parent->INODE.i_block[index--] = 0;
                }
                else if (cp + dp->rec_len == buf + 1024) // Last Entry in the block
                {
                    if(should_print)
                    {
                        printf("rm_child(): Last entry\n");
                    }

                    prev->rec_len += dp->rec_len;

                    if(should_print)
                    {
                        printf("lrec_len: %d, new: %d\n", dp->rec_len, prev->rec_len);
                    }
                }
                else // First but not the only
                {
                    if(should_print)
                    {
                        printf("rm_child(): somewhere in the middle\n");
                    }
                    
                    int size = (buf + 1024) - (cp + dp->rec_len);
                    removed_rec_len = dp->rec_len;
                    lastentry = (DIR *)lastcp;

                    // Traverse all the way to the end block
                    while (lastcp + lastentry->rec_len < buf + BLKSIZE)
                    {
                        if(should_print)
                        {
                            printf("lcp: %d, lent: %d, combo: %d, buf: %d\n", lastcp, lastentry->rec_len, (lastcp + lastentry->rec_len), (buf + BLKSIZE));
                        }

                        lastcp += lastentry->rec_len;
                        lastentry = (DIR *)lastcp;
                    }

                    if(should_print)
                    {
                        printf("lrec_len: %d, removed: %d\n", lastentry->rec_len, removed_rec_len);
                    }

                    lastentry->rec_len += removed_rec_len; // Assign the removed rec_len to last entry rec_len
                    
                    memmove(cp, (cp + dp->rec_len), size);
                }

                put_block(dev, parent->INODE.i_block[i], buf);
                parent->dirty = 1;
                iput(parent);
                return 1;
            }

            cp += dp->rec_len;
            prev = dp;
            dp = (DIR *)cp;
        }
    }
    return -1;
}

int link(char *old, char *new)
{
    int oino, pino;
    char *parent, *child;
    // int dev;
    char temp_pathname[BLKSIZE];
    char temp_basename[BLKSIZE];
    MINODE *omip, *pmip;

    dev = old[0] == '/' ? root->dev : running->cwd->dev;

    // 1. Verify old exists and is not a DIR
    printf("link(): pathname=%s     dev=%d\n", old, dev);    
    oino = getino(old, &dev);

    if (oino == 0)
    {
        printf("link: Old file does not exist\n");
        return -1;
    }

    omip = iget(dev, oino);

    if (!omip)
    {
        printf("link: Could not retrieve old file INODE\n");
    }

    if (omip->INODE.i_mode == DIR_TYPE)
    {
        printf("link: Old file is a DIR\n");
        iput(omip);
        return -1;
    }

    dev = new[0] == '/' ? root->dev : running->cwd->dev;

    // new_file must not exist yet
    printf("link(): pathname=%s     dev=%d\n", new, dev);
    if (getino(new, &dev) != 0)
    {
        printf("link: New file already exists\n");
        return -1;
    }

    //Create new_file with the same inode number of old_file
    strcpy(temp_basename, pathname);
    strcpy(temp_pathname, pathname);

    parent = dirname(temp_pathname);
    if(parent && !strcmp(parent, "."))
    {
        parent = "..";
    }

    child = basename(temp_basename);

    printf("link(): pathname=%s     dev=%d\n", parent, dev);
    pino = getino(parent, &dev);
    pmip = iget(dev, pino);

    //Create entry in new parent DIR with same inode number of old-file
    enter_name(pmip, oino, new);
    omip->INODE.i_links_count++;
    omip->dirty = 1;
    iput(omip);
    iput(pmip);
    return 0;
}

int unlink(char *filename)
{
    int ino, pino;
    char *parent, *child, temp_basename[BLKSIZE], temp_pathname[BLKSIZE];
    MINODE *mip, *pmip;

    printf("unlink(): pathname=%s     dev=%d\n", filename, dev);

    dev = filename[0] == '/' ? root->dev : running->cwd->dev;

    //Get filename's minode
    ino = getino(filename, &dev);
    mip = iget(dev, ino);

    if (mip->INODE.i_mode == DIR_TYPE)
    {
        printf("unlink: Filename was a DIR\n");
        iput(mip);
        return -1;
    }

    //Remove entry from parent DIR's data block
    strcpy(temp_basename, filename);
    strcpy(temp_pathname, filename);

    parent = dirname(temp_pathname);
    child = basename(temp_basename);

    printf("unlink(): pathname=%s     dev=%d\n", parent, dev);
    pino = getino(parent, &dev);
    pmip = iget(dev, pino);

    rm_child(pmip, child);
    pmip->dirty = 1;
    iput(pmip);

    //Decrement INODE'S link_count by 1
    mip->INODE.i_links_count -=1;
    if (mip->INODE.i_links_count > 0)
    {
        mip->dirty = 1;
    }
    else
    {
        int i;
        char buf[BLKSIZE];

        //deallocate all data blocks in INODE
        truncate(&(mip->INODE));

        //deallocate INODE
        idealloc(dev, mip->ino);
    }
    iput(mip);
    return 0;
}

int symlink(char *old, char *new)
{
    int oino, nino, pino;
    MINODE *omip, *nmip, *pmip;
    char pathnamecopy[BLKSIZE];
    char *parent, *child, temp_basename[BLKSIZE], temp_pathname[BLKSIZE];

    //Checking if the assumption is valid
    if (strlen(old) > 60)
    {
        printf("symlink: Old file greater than 60 char\n");
        return -1;
    }

    dev = old[0] == '/' ? root->dev : running->cwd->dev;

    printf("symlink(): pathname=%s     dev=%d\n", old, dev);
    oino = getino(old, &dev);

    //Checking if the old already exists
    if (oino == 0)
    {
        printf("symlink: Old file does not exist\n");
        return -1;
    }

    printf("symlink(): pathname=%s     dev=%d\n", new, dev);

    dev = new[0] == '/' ? root->dev : running->cwd->dev;

    //Checking that new doesnt already exist
    nino = getino(new, &dev);

    if(nino != 0)
    {
        printf("symlink: New file already exists\n");
        return -1;
    }

    //Creating a file with new
    strcpy(pathnamecopy, pathname);
    strcpy(pathname, new);
    int new_ino = 0;
    new_ino = creat_file();

    if(new_ino == 0)
    {
        printf("symlink: Could not create new file\n");
        return 0;
    }

    strcpy(pathname, pathnamecopy);
    printf("symlink(): pathname=%s     dev=%d\n", new, dev);

    dev = new[0] == '/' ? root->dev : running->cwd->dev;
    
    //Change new type to LNK
    nino = getino(new, &dev);

    if(nino != new_ino)
    {
        printf("symlink: the ino for the new file and the ino retrieved not the same.\n");
        return 0;
    }

    nmip = iget(dev, new_ino);
    omip = iget(dev, oino);

    if (!nmip)
    {
        printf("symlink: Issue retreiving the new file (Creat issue?)\n");
        return -1;
    }

    nmip->INODE.i_mode = 0xA1FF;

    //Writing old into the block
    memcpy(nmip->INODE.i_block, old, strlen(old));

    //Setting the size of new
    nmip->INODE.i_size = strlen(old);
    nmip->dirty = 1;

    iput(nmip);

    dev = new[0] == '/' ? root->dev : running->cwd->dev;

    strcpy(temp_basename, old);
    strcpy(temp_pathname, new);

    parent = dirname(temp_pathname);
    child = basename(temp_basename);

    printf("symlink(): pathname=%s     dev=%d\n", parent, dev);
    pino = getino(parent, &dev);
    if (pino == 0)
    {
        printf("symlink: Parent does not exist\n");
        return -1;
    }

    pmip = iget(dev, pino);
    if (!pmip)
    {
        printf("symlink: Issue retreiving the parent INODE\n");
        return -1;
    }

    pmip->dirty = 1;
    iput(pmip);
    return 0;
}