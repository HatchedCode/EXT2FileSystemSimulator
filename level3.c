/**** level3.c ****/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int fd, dev, newdev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256];
extern char *t1, *t2;

#define OWNER  000700
#define GROUP  000070
#define OTHER  000007


int mount(char *mount_point_pathname)    /*  Usage: mount filesys mount_point OR mount */
{
    // 1. Ask for filesys (a pathname) and mount_point (a pathname also).
    // If mount with no parameters: display current mounted filesystems.
    if(mount_point_pathname[0] ==0 || pathname[0] == 0 || mount_point_pathname == NULL || strlen(mount_point_pathname) == 0)
    {
        display_mounted_fs();
        return 0;
    }

    MOUNT *new_fs;
    // 2. Check whether filesys is already mounted: 
    int i;
    for(i = 0; i < NFS_MOUNT && mountTable[i].mounted_inode; i++)
    {
        if(!strcmp(mountTable[i].name, pathname))
        {
            printf("mount(): filesystem is already mounted.\n");
            return -1;
        }
    }

    new_fs = &(mountTable[i]);

    int new_dev = 0;

    // 3. open filesys for RW; use its fd number as the new DEV UNDER LINUX;
    //    Check whether it's an EXT2 file system: if not, reject.
    if((new_dev = open(pathname, O_RDWR)) < 0)
    {
        printf("mount(): could not open the FS %s for RW.  fd = %d.\n", pathname, fd);
        return -1;
    }

    char buf[BLKSIZE];
    get_block(new_dev, 1, buf);
    sp = (SUPER *)buf;

    /* verify it's an ext2 file system *****************/
    if (sp->s_magic != 0xEF53){
        printf("name = %s   and magic = %x is not an ext2 filesystem\n", pathname, sp->s_magic);
        close(mountTable[i].dev);
        return -1;
    } 


    // 4. For mount_point: find its ino, then get its minode:
    int ino;
    MINODE *mip;
    // int dev;
    dev = mount_point_pathname[0] == '/' ? root->dev : running->cwd->dev;
    ino  = getino(mount_point_pathname, &dev);  //to get ino:
    mip  = iget(dev, ino);    //to get its minode in memory; 

    // 5. Check mount_point is a DIR.  
    if(mip->INODE.i_mode != DIR_TYPE)
    {
        printf("mount(): mounting point is not a directory.\n");
        close(mountTable[i].dev);
        return -1;
    }

    // Check mount_point is NOT busy (e.g. can't be someone's CWD)
    if(mip->refCount != 1)
    {
        printf("mount(): INODE is BUSY.\n");
        close(mountTable[i].dev);
        return -1;
    }

    // 6. Record new DEV in the MOUNT table entry;
    new_fs->dev = new_dev;

    //   (For convenience, store the filesys name in the Mount table, and also its
    // ninodes, nblocks, bitmap blocks, inode_start block, etc. 
    //for quick reference)
    new_fs->nblocks = sp->s_blocks_count;
    new_fs->ninodes = sp->s_inodes_count;

    get_block(dev, 2, buf); 
    gp = (GD *)buf;

    new_fs->bmap = gp->bg_block_bitmap;
    new_fs->imap = gp->bg_inode_bitmap;
    new_fs->iblk = gp->bg_inode_table;

    strcpy(new_fs->mount_name, mount_point_pathname);
    strcpy(new_fs->name, pathname);


    // 7. Mark mount_point's minode as being mounted on and let it point at the
    //MOUNT table entry, which points back to the mount_point minode.
    mip->mounted = 1;

    mip->mptr = new_fs;
    
    new_fs->mounted_inode = mip;

    printf("fs %s mounted\n", pathname);

    //return 0 for SUCCESS;
    return 0;
}

int display_mounted_fs()
{
    printf("Mounted Filesystems:\n");
    printf("Mount-Name\t Device-number\t Name\n");
    printf("----------\t -------------\t ----\n");
    int i;
    for(i = 0; i < NFS_MOUNT && mountTable[i].mounted_inode; i++)
    {
        printf("-----%s-----\t -------%d------\t --%s--\n", mountTable[i].mount_name, mountTable[i].dev, mountTable[i].name);
    }
    printf("------------------------------------------------------------------------------------------------------------\n");
    return 1;
}


int umount(char *filesys)
{
    MOUNT *t_mptr;
    int found = 0;
    int i;

    // 1. Search the MOUNT table to check filesys is indeed mounted.
    for(i = 0; i < NFS_MOUNT && mountTable[i].mounted_inode; i++)
    {
        if(!strcmp(mountTable[i].name, filesys))
        {
            found = 1;
            break;
        }
    }

    if(!found)
    {
        printf("umount(): FS =%s is not mounted.\n", filesys);
        return -1;
    }

    t_mptr = &(mountTable[i]);

    // 2. Check whether any file is still active in the mounted filesys;
    //    if so, the mounted filesys is BUSY ==> cannot be umounted yet.
    //    HOW to check?      ANS: by checking all minode[].dev
    int j;
    for (j=0; j<NMINODE; j++)
    {
        if(minode[j].mounted && minode[j].dev == mountTable[i].dev)
        {
            if (minode[j].refCount > 0)
            {
                printf("umount(): The filesystem is BUSY cannot umount.  ino=%d   dev=%d\n", minode[j].ino, minode[j].dev);
                return -1;
            }
        }
    }

    // 3. Find the mount_point's inode (which should be in memory while it's mounted 
    //on).  Reset it to "not mounted"; then 
    //iput()   the minode.  (because it was iget()ed during mounting)
    MINODE *mi_ptr;
    
    mi_ptr = t_mptr->mounted_inode;

    mi_ptr->mounted = 0;

    t_mptr = 0;

    mi_ptr->mptr = 0;

    close(mountTable[i].dev);

    mountTable[i].dev = 0;
    mountTable[i].bmap = 0;
    mountTable[i].imap = 0;
    mountTable[i].mounted_inode = 0;
    mountTable[i].ninodes = 0;
    mountTable[i].nblocks = 0;
    mountTable[i].iblk = 0;
    strcpy(mountTable[i].mount_name,"\0");
    strcpy(mountTable[i].name,"\0");

    iput(mi_ptr);
    printf("fs %s has been unmounted\n", filesys);

    // 4. return 0 for SUCCESS;
    return 0;
} 