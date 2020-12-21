#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>

#define BLKSIZE 1024
#define NUM_INDIRECT_BLOCKS 256

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ext2_group_desc GD;
typedef struct ext2_super_block SUPER;
typedef struct ext2_inode INODE;
typedef struct ext2_dir_entry_2 DIR;

// define shorter TYPES, save typing efforts
// typedef struct ext2_group_desc  GD;
// typedef struct ext2_super_block SUPER;
// typedef struct ext2_inode       INODE;
// typedef struct ext2_dir_entry_2 DIR;    // need this for new version of e2fs


char* dev = "mydisk";
char* path2;

char buf[BLKSIZE], dbuf[BLKSIZE], sbuf[256], path[1024];

int fd;
int bmap, imap, iblock;
int rootblock;

GD *gp;
SUPER *sp;
INODE *ip;
DIR *dp; 

char *name[BLKSIZE];

int search(INODE *ip, char* name);
int print_extendedblocks(u32 *iblknum);
int print_ipInformation(INODE *ip);
int mailmanAlgorithm(INODE *ip);
int tokenize();
int show_dir(INODE *ip);
int dir(char *dev);
int get_block(int fd, char *buf, int blk);


int main(int argc, char* argv[], char* env[])
{

    if(argc > 2)
    {
        dev = argv[1];
        path2 = argv[2];
    }
    else
    {
        printf("Not enough args\n");
        exit(1);
    }

    dir(dev);

    return 1;
}

int dir(char *dev)
{
    int i;
    char* cp;


    fd = open(dev, O_RDONLY);

    if(fd < 0)
    {
        printf("Open %s failed\n", dev);
        exit(1);
    }

    //Read SUPER block at offset 1024
    get_block(fd, buf, 1);
    sp = (SUPER *)buf;

    printf("Check EXT2 FS : %s\n", dev);

    if(sp->s_magic != 0XEF53)
    {
        printf("%s is not an EXT2 FS\n", dev);
        exit(2);
    }
    else
    {
        printf("EXT2 FS OK\n");

        //read GD block in block #2
        get_block(fd, buf, 2);
        gp = (GD *)buf;

        printf("GD info: \nblock bitmap: %d  \ninode bitmap: %d  \ninode table: %d  \nfree blocks count: %d  \ninodes count: %d  \nused directories count: %d\n",
        gp->bg_block_bitmap,
        gp->bg_inode_bitmap,
        gp->bg_inode_table,//bg_block_table,
        gp->bg_free_blocks_count,
        gp->bg_free_inodes_count,
        gp->bg_used_dirs_count);

        bmap = gp->bg_block_bitmap;
        imap = gp->bg_inode_bitmap;
        iblock = gp->bg_inode_table;//bg_block_table;
        printf("bmap = %d   imap = %d   iblock = %d\n",bmap, imap, iblock);
    }

    // Read first INODE block to get root inode #2
    get_block(fd, buf, iblock);
    ip = (INODE *)buf;
    ip++;


    printf("*****************  root inode info  *****************\n");
    printf("mode = %4x    \n", ip->i_mode);
    printf("uid = %d    gid = %d\n", ip->i_uid, ip->i_gid);
    printf("size = %d\n", ip->i_size);
    printf("time = %d\n", ctime(&ip->i_ctime));
    printf("link = %d\n", ip->i_links_count);
    printf("i_block[0] = %d\n", ip->i_block[0]);
    rootblock = ip->i_block[0];
    printf("*****************************************************\n");

    printf("\n\n\n");

    getchar();
    show_dir(ip);
    mailmanAlgorithm(ip);
}


int get_block(int fd, char *buf, int blk)
{
    int n;
    lseek(fd, blk*BLKSIZE, SEEK_SET);
    n = read(fd, buf, BLKSIZE);
    if(n < 0)
    {
        printf("get_block error %d\n", blk);
        return -1;
    }
    return n;
}

int show_dir(INODE *ip)
{
    printf("\n");
    char temp[256];
    char *cp;
    int i;

    for (i=0; i < 12; i++){  // assume DIR at most 12 direct blocks
        if (ip->i_block[i] == 0)
            break;


        // YOU SHOULD print i_block[i] number here
        get_block(fd, sbuf, ip->i_block[i]);

        dp = (DIR *)sbuf;
        cp = sbuf;

        printf("\t \ti_number\t rec_len\t name_len\t name\n");
        while(cp < sbuf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("\t \t%4d\t \t %4d\t \t %4d\t \t %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
}


/*
    searches the DIrectory's data blocks for a name string; 
    return its inode number if found; return 0 if not.
*/
int search(INODE *ip, char *name)
{
    printf("\n");
    char sbuf[BLKSIZE], temp[256];
    DIR *dp;
    char *cp;
    int i;

    for(i = 0; i < 12; i++)
    {
        if(!(ip->i_block[i]))
        {
            break;
        }

        get_block(fd, dbuf, ip->i_block[i]);
        printf("search for %s in %d\n", name, *ip);
        printf("i = %d   i_block[i] = %d\n", i, ip->i_block[i]);

        dp = (DIR *)dbuf;
        cp = dbuf;
        printf("\t \ti_number\t rec_len\t name_len\t name\n");

        while(cp < dbuf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("\t \t%4d\t \t %4d\t \t %4d\t \t %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
            if(!strcmp(temp, name))
            {
                return dp->inode;
            }

            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }


    return 0;
}

/*
    Tokenizes the path into it's seperate componenets using the '/' as a delimeter.
*/
int tokenize()
{
    char *s; 
    char *tempBuf = (char*)malloc(strlen(path2) * sizeof(char)); 
    strcpy(tempBuf, path2);
    s = strtok(tempBuf, "/");
    int i = 0;
    while(s)
    {
        char *temp = (char*)malloc((strlen(s) + 1) * sizeof(char));
        name[i] = (char*)malloc((strlen(s) + 1) * sizeof(char));
        temp = s; 
        strcpy(name[i], temp);
        i++;
        s = strtok(NULL, "/");
    }

    free(tempBuf);

    return i;
}


/*
    In this function we are:
        -taking the pathname that was entered by the users and tokenizing it so that we have all the seperate directories and file (if applicable)
        -We go through each directory until we reach the final file or directory
            -we search for the inode number for the directory that we are searching for.
            -Once we have found the searched directory or file, we have found the inode number
                we then set the current inode to the new inode since all inodes could have 
                the same name, but every inode number is unique.
            -We continue to do this until we go through all the names in the path.
*/
int mailmanAlgorithm(INODE *ip)
{
    int ino, blk, offset, i;
    char ibuf[BLKSIZE];

    printf("tokenize %s\n", path2);
    int n = tokenize();

    for (i=0; i < n; i++){
        ino = search(ip, name[i]);
        
        if (ino==0){
            printf("can't find %s\n", name[i]);
            free(name[i]); 
            exit(1);
        }

        printf("found %s : ino = %d\n", name[i], ino);

        // Mailman's algorithm: Convert (dev, ino) to INODE pointer
        blk    = (ino - 1) / 8 + iblock; //
        offset = (ino - 1) % 8;     //   
        get_block(fd, ibuf, blk); //
        ip = (INODE *)ibuf + offset;   // ip -> new INODE
    }

    print_ipInformation(ip);
}

int print_ipInformation(INODE *ip)
{
    printf("\n");
    char buf2[BLKSIZE], buf3[BLKSIZE];
    u32 *up;
    int i=0,j=0;
    printf("****************** Printing Direct Blocks ******************\n");
    up = (u32 *)buf;
    for(i = 0; i < 12; i++)
    {
        printf("i_block[%d] = %d\n", i, ip->i_block[i]);
    }
    printf("****************** Finished Printing Direct Blocks ******************\n");
    printf("\n");

    printf("\n");
    printf("****************** Printing Indirect Blocks:  ******************\n");
    if(ip->i_block[12])
    {
        get_block(fd, buf, ip->i_block[12]);
        up = (u32 *)buf;
        print_extendedblocks(up);
    }
    else{
        printf("i_block[12] = 0\n");
    }
    printf("****************** Finished Printing Indirect Blocks ******************\n");
    printf("\n");

    printf("\n");
    printf("****************** Printing Double Indirect Blocks ******************\n");
    if(ip->i_block[13])
    {
        get_block(fd, buf, ip->i_block[13]);
        for(i = 0; i < NUM_INDIRECT_BLOCKS; i++)
        {
            printf("block[%d]->", i);
            get_block(fd, buf2, buf[i]);
            up = (u32 *)buf2;
            print_extendedblocks(up);
        }
    }
    else
    {
        printf("i_block[13] = 0\n");
    }
    
    printf("****************** Finished Printing Double Indirect Blocks ******************\n");
    printf("\n");

    printf("\n");
    printf("****************** Printing Triple Indirect Blocks ******************\n");
    if(ip->i_block[14])
    {
        get_block(fd, buf, ip->i_block[13]);
        for(i = 0; i < NUM_INDIRECT_BLOCKS; i++)
        {
            get_block(fd, buf2, buf[i]);
            for(j = 0; j < NUM_INDIRECT_BLOCKS; j++)
            {
                printf("block[%d]->block[%d]->", i, j);
                get_block(fd, buf3, buf2[i]);
                up = (u32 *)buf3;
                print_extendedblocks(up);
            }
        }
    }
    else
    {
        printf("i_block[14] = 0\n");
    }
    printf("****************** Finished Printing Triple Indirect Blocks ******************\n");
    printf("\n");
}

int print_extendedblocks(u32 *iblknum)
{
    int i;
    for(i = 0; i < NUM_INDIRECT_BLOCKS; i++)
    {
        if(*iblknum)
        {
            printf("block[%d]: %d\n", i, *iblknum);
        }

        iblknum++;
    }
}