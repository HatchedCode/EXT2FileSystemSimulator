#include <stdio.h>

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

int open_file(int mode)
{
    int ino;
    // int dev;
    if(pathname[0] == '/')
    {
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }

    printf("open_file(): pathname=%s     dev=%d\n", pathname, dev);
    //getting pathname inode number
    ino = getino(pathname, &dev);

    if(!ino)
    {
        printf("open_file(): ino = 0 for the pathname:%s\n", pathname);
        return -1;
    }

    //get MINode POinter
    MINODE *mip;

    mip = iget(dev, ino);

    if(!mip)
    {
        printf("open_file(): was not able to retrieve mip for the ino = %d\n", ino);
        return -1;
    }

    //Not sure about the permissions part.
    if(mip->INODE.i_mode != REGULAR_FILE_TYPE)
    {
        printf("open_file(): file is not a regular file type.\n");
        return -1;
    }

    //Need to check if file is open with incompatible mode.
    int z=0;
    while(running->fd[z])
    {
        OFT *fd_temp = running->fd[z];
        if(fd_temp->mptr->ino == mip->ino && fd_temp->mptr->INODE.i_gid == mip->INODE.i_gid)
        {
            if(fd_temp->mode > 0)
            {
                printf("open_file(): Cannot open file that is already open for %s\n",
                        fd_temp->mode == 0 ? "READ" : fd_temp->mode == 1 ? "WRITE" : fd_temp->mode == 2 ? "READ/WRITE" : "APPEND");
                return -1;
            }
        }
        z++;
    }


    //Code here
    OFT *oftp;

    oftp = (OFT* )malloc(sizeof(OFT));
    oftp->mode = mode;
    oftp->mptr = (MINODE *)malloc(sizeof(MINODE));
    oftp->mptr = mip;
    oftp->refCount = 1;
    switch(mode)
    {
        case 0:
            oftp->offset = 0;     // R: offset = 0
            break;
        case 1:
            truncateLevel2(mip);        // W: truncate file to 0 size
            oftp->offset = 0;
            break;
        case 2:
            oftp->offset = 0;     // RW: do NOT truncate file
            break;
        case 3:
            oftp->offset =  mip->INODE.i_size;  // APPEND mode
            break;
        default:
            printf("invalid mode\n");
            return -1;
    }

    int index = 0;
    while(running->fd[index])
    {
        index++;
    }

    if(index >= 8)
    {
        printf("open_file(): all oft spaces are full. error trying to allocate a FREE OpenFileTable (OFT).\n");
        return -1;
    }

    running->fd[index] = oftp;

    if(mode > 0)
    {
        mip->INODE.i_atime = time(0L);
    }
    else
    {
        mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
    }

    mip->dirty = 1;

    // iput(mip); this will change where the oftp is point at by putting the minode

    return index;
//   1. ask for a pathname and mode to open:
//          You may use mode = 0|1|2|3 for R|W|RW|APPEND

//   2. get pathname's inumber:
//          if (pathname[0]=='/') dev = root->dev;          // root INODE's dev
//          else                  dev = running->cwd->dev;  
//          ino = getino(pathname); 

//   3. get its Minode pointer
//          mip = iget(dev, ino);  

//   4. check mip->INODE.i_mode to verify it's a REGULAR file and permission OK.
      
//      Check whether the file is ALREADY opened with INCOMPATIBLE mode:
//            If it's already opened for W, RW, APPEND : reject.
//            (that is, only multiple R are OK)

//   5. allocate a FREE OpenFileTable (OFT) and fill in values:
 
//          oftp->mode = mode;      // mode = 0|1|2|3 for R|W|RW|APPEND 
//          oftp->refCount = 1;
//          oftp->minodePtr = mip;  // point at the file's minode[]

//   6. Depending on the open mode 0|1|2|3, set the OFT's offset accordingly:

//       switch(mode){
//          case 0 : oftp->offset = 0;     // R: offset = 0
//                   break;
//          case 1 : truncate(mip);        // W: truncate file to 0 size
//                   oftp->offset = 0;
//                   break;
//          case 2 : oftp->offset = 0;     // RW: do NOT truncate file
//                   break;
//          case 3 : oftp->offset =  mip->INODE.i_size;  // APPEND mode
//                   break;
//          default: printf("invalid mode\n");
//                   return(-1);
//       }

//    7. find the SMALLEST i in running PROC's fd[ ] such that fd[i] is NULL
//       Let running->fd[i] point at the OFT entry

//    8. update INODE's time field
//          for R: touch atime. 
//          for W|RW|APPEND mode : touch atime and mtime
//       mark Minode[ ] dirty

//    9. return i as the file descriptor
}

int truncateLevel2(MINODE *mip)
{
    //release direct blocks
    int i;
    for (i=0; i<12; i++)
    {
        if (mip->INODE.i_block[i] == 0)
        {
            break;
        }

        mip->INODE.i_block[i] = 0;
    }    

    //release indirect blocks ( block 13, index 12 )
    int blk, index;
    blk = mip->INODE.i_block[i];
    char buf[BLKSIZE];
    u32 *up;

    if(blk)
    {
        get_block(dev, blk, buf);
        up = (u32 *)buf;
        for(index =0; index < 256; index++, up++)
        {
            if(*up)
            {
               *up = 0;
            }
        }

        put_block(dev, blk, buf);
    }

    i++;
    int innerIndex;

    //release double indirect blocks ( block 14, index 13 )
    if(mip->INODE.i_block[i])
    {
        blk = mip->INODE.i_block[i];
        get_block(dev, blk, buf);
        up = (u32 *)buf;
        for(index =0; index < 256; index++, up++)
        {
            if(*up)
            {
                char tempBuf[BLKSIZE];
                u32 *t_up;
                get_block(dev, *up, tempBuf);
                t_up = (u32 *)tempBuf;
                for(innerIndex = 0; innerIndex < 256; innerIndex++, t_up++)
                {
                    if(*t_up)
                    {
                        *t_up = 0;
                    }
                }

                put_block(dev, *up, tempBuf);
            }
        }

        put_block(dev, blk, buf);
    }

    i++;

    //release triple indirect blocks ( block 15, index 14 )
    if(mip->INODE.i_block[i])
    {
        blk = mip->INODE.i_block[i];
        get_block(dev, blk, buf);
        up = (u32 *)buf;
        for(index =0; index < 256; index++, up++)
        {
            if(*up)
            {
                char tempBuf[BLKSIZE];
                u32 *t_up;
                get_block(dev, *up, tempBuf);
                t_up = (u32 *)tempBuf;
                for(innerIndex = 0; innerIndex < 256; innerIndex++, t_up++)
                {
                    char tempBuf[BLKSIZE];
                    u32 *t_up;
                    get_block(dev, *t_up, tempBuf);
                    t_up = (u32 *)tempBuf;                    
                    if(*t_up)
                    {
                        int innerInnerIndex;
                        char tempBuf2[BLKSIZE];
                        u32 *tt_up;
                        get_block(dev, *tt_up, tempBuf2);
                        tt_up = (u32 *)tempBuf2;                           
                        for(innerInnerIndex = 0; innerInnerIndex < 256; innerInnerIndex++, tt_up++)
                        {
                            if(*tt_up)
                            {
                                *tt_up = 0;
                            }
                        }

                        put_block(dev, *t_up, tempBuf2);                        
                    }
                }

                put_block(dev, *up, tempBuf);
            }
        }

        put_block(dev, blk, buf);
    }

    //
    mip->INODE.i_atime = mip->INODE.i_mtime = time(0L); //I think we just update all the times, but not sure. rn I just updating mtime and atime.
    mip->INODE.i_size = 0;
    mip->dirty = 1;
    mip->INODE.i_blocks = 0;

    iput(mip);
//     // 1. release mip->INODE's data blocks;
//     //  a file may have 12 direct blocks, 256 indirect blocks and 256*256
//     //  double indirect data blocks. release them all.
//     // 2. update INODE's time field

//     // 3. set INODE's size to 0 and mark Minode[ ] dirty
    return 1;
}

int close_file(int fd)
{
    if(fd < 0 || fd > 7)
    {
        printf("close_file(): the file descriptor is not within the correct range ( 0 <= fd <= 7). Given fd = %d\n", fd);
        return -1;
    }

    int i;
    OFT *oftp;

    if(!(oftp = running->fd[fd]))
    {
        printf("close_file(): The given file descriptor does not point to a valid OFT entry.\n");
        return -1;
    }

    running->fd[fd] = 0;
    oftp->refCount--;
    if(oftp->refCount > 0)
    {
        printf("close_file(): the refCount for the OFT is not zero.\n");
        return 0;
    }

    MINODE *mip;

    //Since we are the last users of this OFT entry we want to dispose of the inode ( minode )
    mip = oftp->mptr;
    iput(mip);

    return 0;


    // 1. verify fd is within range.

    // 2. verify running->fd[fd] is pointing at a OFT entry

    // 3. The following code segments should be fairly obvious:
    //  oftp = running->fd[fd];
    //  running->fd[fd] = 0;
    //  oftp->refCount--;
    //  if (oftp->refCount > 0) return 0;

    //  // last user of this OFT entry ==> dispose of the Minode[]
    //  mip = oftp->inodeptr;
    //  iput(mip);

    //  return 0; 
}

int lseek_level2(int fd, int position)
{
    if(fd < 0 || fd > 7)
    {
        printf("lseek_level2(): the file descriptor is not within the correct range ( 0 <= fd <= 7). Given fd = %d\n", fd);
        return -1;
    }

    int i;
    OFT *oftp;

    if(!(oftp = running->fd[fd]))
    {
        printf("lseek_level2(): The given file descriptor does not point to a valid OFT entry.\n");
        return -1;
    }

    if(position < 0 || position > (oftp->mptr->INODE.i_size - 1)) //either end of the file.  --> NOT SURE ABOUT THIS COMMENT. I think this if statement is violating this comment.
    {
        printf("lseek_level2(): the requested position is not within the file size bounds ( 0 <= position <= %d ). position = %d\n", (oftp->mptr->INODE.i_size - 1), position);
        return -1;
    }

    //We are assuming that position starts from the beginning of the file, not relative to the last position.

    int original_position = oftp->offset;
    oftp->offset = position;
    
    return original_position;
    // From fd, find the OFT entry. 

    // change OFT entry's offset to position but make sure NOT to over run either end
    // of the file.

    // return originalPosition
}

int pfd()
{
    printf("pfd:\n");
    printf("fd\t\tmode\t\toffset\t\tINODE\n");
    printf("---\t\t-----\t\t-------\t\t------\n");
    int i;
    for (i = 0; running->fd[i]; i++)
    {
        printf("-%d-\t\t%s----\t\t%d------\t\t[%d , %d]-----\n",
        i,
        running->fd[i]->mode == 0 ? "READ" : running->fd[i]->mode == 1 ? "WRITE" : running->fd[i]->mode == 2 ? "READ/WRITE" : "APPEND",
        running->fd[i]->offset,
        running->fd[i]->mptr->dev,
        running->fd[i]->mptr->ino
        );
    }
    // This function displays the currently opened files as follows:

    //     fd     mode    offset    INODE
    //    ----    ----    ------   --------
    //      0     READ    1234   [dev, ino]  
    //      1     WRITE      0   [dev, ino]
    //   --------------------------------------
    // to help the user know what files has been opened
}

// As you already know, these (Next two functions) are needed for I/O redirections.

dup_level2(int fd)
{
    if(fd < 0 || fd > 7)
    {
        printf("dup_level2(): the file descriptor is not within the correct range ( 0 <= fd <= 7). Given fd = %d\n", fd);
        return -1;
    }

    OFT *oftp;

    if(!(oftp = running->fd[fd]))
    {
        printf("dup_level2(): The given file descriptor does not point to a valid OFT entry.\n");
        return -1;
    }

    int index = 0;
    while(index < 8 && running->fd[index])
    {
        index++;
    }
    
    if(index > 7)
    {
        printf("dup_level2(): There are no more empty OFT entries to copy fd = %d.\n", fd);
        return -1;
    }
    
    running->fd[index] = (OFT *)malloc(sizeof(OFT));
    running->fd[index] = oftp;
    running->fd[index]->refCount++;

    return index;
//   verify fd is an opened descriptor;
//   duplicates (copy) fd[fd] into FIRST empty fd[ ] slot;
//   increment OFT's refCount by 1;
}

dup2_level2(int fd, int gd)
{
    if(fd < 0 || fd > 7)
    {
        printf("dup2_level2(): the file descriptor is not within the correct range ( 0 <= fd <= 7). Given fd = %d\n", fd);
        return -1;
    }

    OFT *oftp_fd;

    if(!(oftp_fd = running->fd[fd]))
    {
        printf("dup2_level2(): The first (fd) given file descriptor does not point to a valid OFT entry.\n");
        return -1;
    }


    if(gd < 0 || gd > 7)
    {
        printf("dup2_level2(): the file descriptor is not within the correct range ( 0 <= gd <= 7). Given gd = %d\n", gd);
        return -1;
    }

    OFT *oftp_gd;

    //This below looks good to me, but if something below does not work then it is because of the below code, but I believe it works right.(ps, I didn't actually test it yet (11/11/2019))

    if(!(oftp_gd = running->fd[gd]))
    {
        oftp_gd = oftp_fd;
        // printf("dup2_level2(): The second (gd) given file descriptor does not point to a valid OFT entry.\n");
        // return -1;
    }
    else
    {
        oftp_gd = 0;
        oftp_gd = oftp_fd;
    }
    
    return gd;


//   CLOSE gd first if it's already opened;
//   duplicates fd[fd] into fd[gd]; 
}



int read_file()
{
    char line[256];
    int fd, nbytes;
    //We are assuming that the file is open for R or RW
    do
    {
        printf("Enter the fd (or type: exit to return to main menu): ");
        fgets(line, 256, stdin);
        line[strlen(line)-1] = 0;
        if (line[0]==0)
            continue;

        if(!strcmp(line, "exit"))
        {
            return -1;
        }

        char * endptr;
        long result = strtol(line, &endptr, 10);
        if (endptr > line)
        {
            fd = (int)result;
            break;
        }
        else
        {
            printf("read_file(): Error, Invalid file descriptor.\n");
            continue;
        }
           
    } while (1);

    do
    {
        printf("Enter the number of bytes to read from the file: ");
        fgets(line, 256, stdin);
        line[strlen(line)-1] = 0;
        if (line[0]==0)
            continue;

        if(!strcmp(line, "exit"))
        {
            return -1;
        }

        char * endptr;
        long result = strtol(line, &endptr, 10);
        if (endptr > line)
        {
            nbytes = (int)result;
            break;
        }
        else
        {
            printf("read_file(): Error, Invalid number of bytes to read.\n");
            continue;
        }
           
    } while (1);


    if(fd < 0 || fd > 7)
    {
        printf("read_file(): the file descriptor is not within the correct range ( 0 <= fd <= 7). Given fd = %d\n", fd);
        return -1;
    }

    OFT *oftp;

    if(!(oftp = running->fd[fd]))
    {
        printf("read_file(): The first (fd) given file descriptor does not point to a valid OFT entry.\n");
        return -1;
    }

    if(oftp->mode == 1 || oftp == 3)
    {
        printf("read_file(): file descriptor is not open for RD or READ-WRITE.\n");
        return -1;
    }

    char buf[BLKSIZE];

    return (myread(fd, buf, nbytes));
//   Preparations: 
//     ASSUME: file is opened for RD or RW;
//     ask for a fd  and  nbytes to read;
//     verify that fd is indeed opened for RD or RW;
//     return(myread(fd, buf, nbytes));
}


int myread(int fd, char buf[], int nbytes)
{
    int count, lblk, offset, startByte, remain, avil, file_size, blk;
    OFT *oftp;
    MINODE *mip;
    char readbuf[nbytes];
    
    oftp = running->fd[fd];

    mip = oftp->mptr;

    file_size = oftp->mptr->INODE.i_size;

    count = 0;
    avil = file_size - oftp->offset;
    char *cq = buf;

    while(nbytes && avil)
    {
        //Compute LOGICAL BLOCK number blk and startByte in that block from offset
        lblk = oftp->offset / BLKSIZE;
        startByte = oftp->offset % BLKSIZE;
        char buf1[BLKSIZE];

        if(lblk < 12) //read direct blocks
        {
            blk = mip->INODE.i_block[lblk];
        }
        else if(lblk >= 12 && lblk < 256 + 12) //read indirect blocks
        {
            get_block(mip->dev, mip->INODE.i_block[12], buf1);
            if(buf1)
            {
                blk = buf1[lblk - 12];
            }        
        }
        else //read double indirect blocks
        {
            char buf2[BLKSIZE];

            // If the double indirect block i_block [13] does not exist
            // it must be allocated and initialized to 0.
            // if (mip->INODE.i_block[13] == 0)
            // {
            //     printf("myread(): Not able to read anything from block[13] double indirect block.\n");
            //     break;
            // }

            // Entry in the double indirect block
            get_block(dev, mip->INODE.i_block[13], buf1);
            
            // if (buf1[((lblk - (12 + 256)) / 256)] == 0)
            // {
            //     printf("myread(): Not able to read anything from double indirect block.\n");
            //     break;
            // }

            // Double indirect block
            get_block(dev, buf1[((lblk - (12 + 256)) / 256)], buf2);
            // if (buf2[(lblk - (12 + 256)) % 256] == 0)
            // {
            //     printf("myread(): Not able to read anything from double indirect block.\n");
            //     break;
            // }

            blk = buf2[(lblk - (12 + 256)) % 256];
        }
        
        /* get the data block into readbuf[BLKSIZE] */
        get_block(mip->dev, blk, readbuf);

        /* copy from startByte to buf[ ], at most remain bytes in this block */
        char *cp = readbuf + startByte;
        remain = BLKSIZE - startByte;

        if(avil < remain)
        {
            remain = avil;
        }

        if (remain > nbytes)
        {
            memcpy(cq, cp, nbytes);
            count += nbytes;
            oftp->offset += nbytes;
            remain -= nbytes;
            avil -= nbytes;
            nbytes -= nbytes;
        }
        else
        {
            memcpy(cq, cp, remain);
            count += remain;
            oftp->offset += remain;
            nbytes -= remain;
            avil -= remain;
            remain -= remain;
        }

        // /*This while loop needs to be optimized, but will complete it later.*/
        // while(remain > 0)
        // {
        //     *cq++ = cp++; //copy byte from readbuf[] into buf[]
        //     oftp->offset++;  //Advance the offset
        //     count++;    //Increment count as a number of bytes read.
        //     avil--;
        //     nbytes--;
        //     remain--;
        //     if(nbytes <= 0 || avil <= 0)
        //     {
        //         break;
        //     }
        // }
        // //Loop out to outer while loop since one data block is not enough, loop back to outer whole loop for more.
    }

    //printf("myread(): read %d char from file descriptor %d\n", count, fd);
    return count;

    //Logical block: lblk = offset / BLKSIZE
    

    //the place to start read is defined as: start = offset % BLKSIZE


    //the remaining number of bytes to read is defined as: remain = BLKSIZE - start


    //The amount of bytes that the file has avaliable to read is: avil = file_size - offset

}

// int myread(int fd, char buf[ ], nbytes) behaves EXACTLY the same as the
// read() system call in Unix/Linux. 
// The algorithm of myread() can be best explained in terms of the following 
// diagram.

// (1).  PROC              (2).                          | 
//      =======   |--> OFT oft[ ]                        |
//      | pid |   |   ============                       |
//      | cwd |   |   |mode=flags|                       | 
//      | . ..|   |   |minodePtr ------->  minode[ ]     |      BlockDevice
//      | fd[]|   |   |refCount=1|       =============   |   ==================
//  fd: | .------>|   |offset    |       |  INODE    |   |   | INODE -> blocks|
//      |     |       |===|======|       |-----------|   |   ==================
//      =======           |              |  dev,ino  |   |
//                        |              =============   |
//                        |
//                        |<------- avil ------->|
//     -------------------|-----------------------
//     |    |    | ...  |lbk  |   |  ...| .......|
//     -------------------|---|------------------|-
// lbk   0    1 .....     |rem|                   |
//                      start                   fsize  
                        
// ------------------------------------------------------------------------------
//                  Data structures for reading file

// (1). Assume that fd is opened for READ. 
// (2). The offset in the OFT points to the current byte position in the file from
//      where we wish to read nbytes. 
// (3). To the kernel, a file is just a sequence of contiguous bytes, numbered from
//      0 to file_size - 1. As the figure shows, the current byte position, offset
//      falls in a LOGICAL block (lbk), which is 

//              lbk = offset / BLKSIZE 

//      the byte to start read in that logical block is 

//              start = offset % BLKSIZE 

//      and the number of bytes remaining in the logical block is 

//              remain = BLKSIZE - start. 

//      At this moment, the file has 

//              avil = file_size - offset 

//      bytes available for read. 

//      These numbers are used in the read algorithm.


// (4). myread() behaves exactly the same as the read(fd, buf, nbytes) syscall of 
//      Unix/Linux. It tries to read nbytes from fd to buf[ ], and returns the 
//      actual number of bytes read.

// (5). ============ Algorithm and pseudo-code of myread() =======================

// int myread(int fd, char *buf, int nbytes)
// {
//  1. int count = 0;
//     avil = fileSize - OFT's offset // number of bytes still available in file.
//     char *cq = buf;                // cq points at buf[ ]

//  2. while (nbytes && avil){

//        Compute LOGICAL BLOCK number lbk and startByte in that block from offset;

//              lbk       = oftp->offset / BLKSIZE;
//              startByte = oftp->offset % BLKSIZE;
     
//        // I only show how to read DIRECT BLOCKS. YOU do INDIRECT and D_INDIRECT
 
//        if (lbk < 12){                     // lbk is a direct block
//            blk = mip->INODE.i_block[lbk]; // map LOGICAL lbk to PHYSICAL blk
//        }
//        else if (lbk >= 12 && lbk < 256 + 12) { 
//             //  indirect blocks 
//        }
//        else{ 
//             //  double indirect blocks
//        } 

//        /* get the data block into readbuf[BLKSIZE] */
//        get_block(mip->dev, blk, readbuf);

//        /* copy from startByte to buf[ ], at most remain bytes in this block */
//        char *cp = readbuf + startByte;   
//        remain = BLKSIZE - startByte;   // number of bytes remain in readbuf[]

//        while (remain > 0){
//             *cq++ = *cp++;             // copy byte from readbuf[] into buf[]
//              oftp->offset++;           // advance offset 
//              count++;                  // inc count as number of bytes read
//              avil--; nbytes--;  remain--;
//              if (nbytes <= 0 || avil <= 0) 
//                  break;
//        }
 
//        // if one data block is not enough, loop back to OUTER while for more ...

//    }
//    printf("myread: read %d char from file descriptor %d\n", count, fd);  
//    return count;   // count is the actual number of bytes read
// }

//                   OPTMIAZATION OF THE READ CODE:

// Instead of reading one byte at a time and updating the counters on each byte,
// TRY to calculate the maximum number of bytes available in a data block and
// the number of bytes still needed to read. Take the minimum of the two, and read
// that many bytes in one operation. Then adjust the counters accordingly. This 
// would make the read loops more efficient. 

// REQUIRED: optimize the read algorithm in your project.

// ==========================  HOW TO cat ======================================
// cat filename:

//    char mybuf[1024], dummy = 0;  // a null char at end of mybuf[ ]
//    int n;

// 1. int fd = open filename for READ;
// 2. while( n = read(fd, mybuf[1024], 1024)){
//        mybuf[n] = 0;             // as a null terminated string
//        // printf("%s", mybuf);   <=== THIS works but not good
//        spit out chars from mybuf[ ] but handle \n properly;
//    } 
// 3. close(fd);

int cat_file()
{
    char mybuf[BLKSIZE];
    int n;

    int fd = open_file(0/*file mode = READ*/);

    if(fd == -1)
    {
        printf("cat_file(): the file descriptor is 0.\n");
        return -1;
    }

    printf("\n\n");
    while(n = myread(fd, mybuf, 1024))
    {
        mybuf[n] = 0; //Null terminating string.
        //DO NOT TRY TO PRINT the whole mybuf one time.
        int index=0;  
        while(index < n)
        {
            putchar(mybuf[index]);
            if(mybuf[index] == '\n')
            {
                putchar('\r');
            }
            index++;
        }
    }
    printf("\n\r");
    putchar('\n'); //Print newline.
    close_file(fd);
}


int write_file()
{
    int fd = 0;
    char line[256];
    do
    {
        printf("Enter the file descriptor (0 <= fd <= 7) or exit to return to main menu: ");
        fgets(line, 128, stdin);
        line[strlen(line)-1] = 0;
        if (line[0]==0)
            continue;

        if(!strcmp(line, "exit"))
        {
            return -1;
        }

        char * endptr;
        long result = strtol(line, &endptr, 10);
        if (endptr > line)
        {
            if(fd < 0 || fd > 7)
            {
                continue;
            }

            fd = (int)result;
            break;
        }
        else
        {
            printf("write_file(): Error, Invalid file descriptor.\n");
            continue;
        }
        printf("write_file(): Invalid file descriptor.\n");
    } while (1);

    char new_text[BLKSIZE];
    do
    {
        printf("Enter text:\n");
        fgets(line, 128, stdin);
        line[strlen(line)-1] = 0;
        if (line[0]==0)
            continue;

        strcpy(new_text, line);
        break;
    } while (1);

    OFT *oftp;
    if(!(oftp = running->fd[fd]))
    {
        printf("write_file(): Error, the file descriptor does not contain a valid file entry in the file descriptors.\n");
        return -1;
    }

    if(oftp->mode == 0)
    {
        printf("write_file(): Error, file is not open for WRITE nor READ-WRITE.\n");
        return -1;
    }

    if(oftp->mode == 1)
    {
        printf("write_file(): mode = WRITE,  right before truncate\n");
        
        int x;
        for(x=0; x < 12; x++)
        {
            printf("Block[%d] = %d\n", x, oftp->mptr->INODE.i_block[x]);
        }
        //truncateLevel2(oftp->mptr);
        printf("write_file(): mode = WRITE,  right AFTER truncate\n");
        // int i;
        // for(i=0; i< 12 && oftp->mptr->INODE.i_block[i]; i++)
        // {
        //     bdealloc(oftp->mptr->dev, oftp->mptr->INODE.i_block[i]);
        // }
    }

    int nbytes;
    char tbuf[BLKSIZE];
    memcpy(tbuf, new_text, strlen(new_text));
    nbytes = strlen(tbuf);

    return (mywrite(fd, tbuf, nbytes));
//   1. Preprations:
//      ask for a fd   and   a text string to write;

//   2. verify fd is indeed opened for WR or RW or APPEND mode

//   3. copy the text string into a buf[] and get its length as nbytes.

//      return(mywrite(fd, buf, nbytes));
}

int mywrite(int fd, char buf[], int nbytes)
{
    int count=0;
    int lbk, startByte, blk, remain;
    char wbuf[BLKSIZE];
    OFT *oftp;
    oftp = running->fd[fd];
    MINODE *mip = oftp->mptr;
    char *cq;

    cq = buf;

    while(nbytes > 0)
    {
        lbk = oftp->offset / BLKSIZE;
        startByte = oftp->offset % BLKSIZE;

        //Write to the blocks (direct, indirect, and double indirect, if neccessary)
        char ibuf[256];

        if(lbk < 12)//direct blocks
        {
            if (mip->INODE.i_block[lbk] == 0)
            {
                mip->INODE.i_block[lbk] = balloc(mip->dev);
            }
            blk = mip->INODE.i_block[lbk];
        }        // if(oftp->mode == 1)
        // {
        //     memset(wbuf,0, 1024*sizeof(char));
        // }
        else if(lbk >= 12 && lbk < 256 + 12) // Indirect Blocks
        {
            if (mip->INODE.i_block[12] == 0)
            {
                mip->INODE.i_block[12] = balloc(mip->dev);
                int index;
                get_block(oftp->mptr->dev, mip->INODE.i_block[12], ibuf);
                for(index =0; index < BLKSIZE; index++)
                {
                    ibuf[index] = 0;
                }
                put_block(mip->dev, mip->INODE.i_block[12], ibuf);
            }

            get_block(oftp->mptr->dev, mip->INODE.i_block[12], ibuf);
            blk = ibuf[lbk - 12];

            if(blk == 0)
            {
                // I think this is what it is asking for
                blk = balloc(mip->dev);
                put_block(dev, mip->INODE.i_block[12], blk);
            }
        }
        else
        {
            char buf1[256], buf2[256];

            // If the double indirect block i_block [13] does not exist
            // it must be allocated and initialized to 0.
            if (mip->INODE.i_block[13] == 0)
            {
                //Allocate and Initialize to zero
                mip->INODE.i_block[13] = balloc(mip->dev);

                //Initialize blocks to zero.
                int index;
                get_block(oftp->mptr->dev, mip->INODE.i_block[13], buf1);
                for(index =0; index < BLKSIZE; index++)
                {
                    ibuf[index] = 0;
                }
                put_block(mip->dev, mip->INODE.i_block[13], buf1);
                // Need to initialize
                // mip->INODE.i_block[13] = 0;
                //memset(mip->INODE.i_block[13], 0 , 256*sizeof(char));
            }
        // if(oftp->mode == 1)
        // {
        //     memset(wbuf,0, 1024*sizeof(char));
        // }
            // Entry in the double indirect block
            get_block(dev, mip->INODE.i_block[13], buf1);
            if (buf1[((lbk - 12) + 256) / 256] == 0)
            {
                //Allocate and Initialize to zero
                buf1[((lbk - 12) + 256) / 256] = balloc(mip->dev);

                //Initialize blocks to zero.
                int index;
                get_block(oftp->mptr->dev, buf1[((lbk - 12) + 256) / 256], buf1);
                for(index =0; index < BLKSIZE; index++)
                {
                    ibuf[index] = 0;
                }
                put_block(mip->dev, buf1[((lbk - 12) + 256) / 256], buf1);

                // Need to initialize
                // buf1[((lbk - 12) + 256) / 256] = 0;
                //memset(buf1[((lbk - 12) + 256) / 256], 0 , 256*sizeof(char));
            }

            // Double indirect block
            get_block(dev, buf1[((lbk - 12) + 256) / 256], buf2);
            if (buf2[((lbk - 12) + 256) % 256] == 0)
            {
                //Allocate and Initialize to zero
                buf2[((lbk - 12) + 256) % 256] = balloc(mip->dev);

                //Initialize blocks to zero.
                int index;
                get_block(oftp->mptr->dev, buf2[((lbk - 12) + 256) % 256], buf2);
                for(index =0; index < BLKSIZE; index++)
                {
                    ibuf[index] = 0;
                }
                put_block(mip->dev, buf2[((lbk - 12) + 256) % 256], buf2);

                // // Need to put back
                // put_block(dev, buf1[((lbk - 12) + 256) % 256], buf2);
            }
        }

        get_block(mip->dev, blk, wbuf);

        char *cp = wbuf + startByte;
        remain = BLKSIZE - startByte;

        // while(remain > 0)
        // {
        //     *cp++ = *cq++;
        //     nbytes--;
        //     remain--;
        //     oftp->offset--;
        //     if(oftp->offset > mip->INODE.i_size)
        //     {
        //         mip->INODE.i_size++;
        //     }

        //     if(nbytes <= 0 )
        //     {
        //         break;
        //     }
        // }

        // Approved by KCW as being the right approach
        if (remain > nbytes)
        {
            memcpy(cp, cq, nbytes);
            count += nbytes;
            oftp->offset += nbytes;

            if(oftp->offset > mip->INODE.i_size)
            {
                mip->INODE.i_size += nbytes;
            }        // if(oftp->mode == 1)
        // {
        //     memset(wbuf,0, 1024*sizeof(char));
        // }

            remain -= nbytes;
            nbytes -= nbytes;
        }
        else
        {
            memcpy(cp, cq, remain);
            count += remain;
            oftp->offset += remain;
            
            if(oftp->offset > mip->INODE.i_size)
            {
                mip->INODE.i_size += remain;
            }

            nbytes -= remain;
            remain -= remain;
        }

        put_block(mip->dev, blk, wbuf);
    }

    mip->dirty = 1;
    printf("wrote %d char into file descriptor fd=%d\n", count, fd);
    return count;
}

// The algorithm of write_file() can also be explained in terms of the following
// figure.

// (1).  PROC              (2).                          | 
//      =======   |--> OFT oft[ ]                        |
//      | pid |   |   ============                       |
//      | cwd |   |   |mode=flags|                       | 
//      | . ..|   |   |minodePtr ------->  minode[ ]     |      BlockDevice
//      | fd[]|   |   |refCount=1|       =============   |   ==================
//  fd: | .------>|   |offset    |       |  INODE    |   |   | INODE -> blocks|
//      |     |       |===|======|       |-----------|   |   ==================
//      =======           |              | dev,inode |   | 
//                        |              |  dirty    |   |
//                        |              =============   |
//                        |
//     -------------------|-----------------
//     |    |    | ...  |lbk  | ............
//     -------------------|-----------------
// lbk   0    1 .....     |rem|            |
//                      start           fileSize (in INODE)  
                        
// ------------------------------------------------------------------------------
//                Data Structures for write()

        // if(oftp->mode == 1)
        // {
        //     memset(wbuf,0, 1024*sizeof(char));
        // }
// mywrite behaves exactly the same as Unix's write(fd, buf, nbytes) syscall.
// It writes nbytes from buf[ ] to the file descriptor fd, extending the file 
// size as needed.

// int mywrite(int fd, char buf[ ], int nbytes) 
// {
//   while (nbytes > 0 ){

//      compute LOGICAL BLOCK (lbk) and the startByte in that lbk:

//           lbk       = oftp->offset / BLKSIZE;
//           startByte = oftp->offset % BLKSIZE;

//     // I only show how to write DIRECT data blocks, you figure out how to 
//     // write indirect and double-indirect blocks.

//      if (lbk < 12){                         // direct block
//         if (ip->INODE.i_block[lbk] == 0){   // if no data block yet

//            mip->INODE.i_block[lbk] = balloc(mip->dev);// MUST ALLOCATE a block
//         }
//         blk = mip->INODE.i_block[lbk];      // blk should be a disk block now
//      }
//      else if (lbk >= 12 && lbk < 256 + 12){ // INDIRECT blocks 
//               // HELP INFO:
//               if (i_block[12] == 0){
//                   allocate a block for it;
//                   zero out the block on disk !!!!
//               }
//               get i_block[12] into an int ibuf[256];
//               blk = ibuf[lbk - 12];
//               if (blk==0){
//                  allocate a disk block;
//                  record it in i_block[12];
//               }
//               .......
//      }
//      else{
//             // double indirect blocks */
//      }

//      /* all cases come to here : write to the data block */
//      get_block(mip->dev, blk, wbuf);   // read disk block into wbuf[ ]  
//      char *cp = wbuf + startByte;      // cp points at startByte in wbuf[]
//      remain = BLKSIZE - startByte;     // number of BYTEs remain in this block

//      while (remain > 0){               // write as much as remain allows  
//            *cp++ = *cq++;              // cq points at buf[ ]
//            nbytes--; remain--;         // dec counts
//            oftp->offset++;             // advance offset
//            if (offset > INODE.i_size)  // especially for RW|APPEND mode
//                mip->INODE.i_size++;    // inc file size (if offset > fileSize)
//            if (nbytes <= 0) break;     // if already nbytes, break
//      }
//      put_block(mip->dev, blk, wbuf);   // write wbuf[ ] to disk
     
//      // loop back to outer while to write more .... until nbytes are written
//   }

//   mip->dirty = 1;       // mark mip dirty for iput() 
//   printf("wrote %d char into file descriptor fd=%d\n", nbytes, fd);           
//   return nbytes;
// }


//                 OPTIMIZATION OF write Code

// As in read(), the above inner while(remain > 0) loop can be optimized:
// Instead of copying one byte at a time and update the control variables on each 
// byte, TRY to copy only ONCE and adjust the control variables accordingly.

// REQUIRED: Optimize the write() code in your project.

// =============================================================================

//                       HOW TO cp ONE file:

// cp src dest:

// 1. fd = open src for READ;

// 2. gd = open dst for WR|CREAT; 

//    NOTE:In the project, you may have to creat the dst file first, then open it 
//         for WR, OR  if open fails due to no file yet, creat it and then open it
//         for WR.

// 3. while( n=read(fd, buf[ ], BLKSIZE) ){
//        write(gd, buf, n);  // notice the n in write()
//    }

// ==============================================================================

//                     HOW TO mv (rename)
// mv src dest:

// 1. verify src exists; get its INODE in ==> you already know its dev
// 2. check whether src is on the same dev as src

//               CASE 1: same dev:
// 3. Hard link dst with src (i.e. same INODE number)
// 4. unlink src (i.e. rm src name from its parent directory and reduce INODE's
//                link count by 1).
                
//               CASE 2: not the same dev:
// 3. cp src to dst
// 4. unlink src

int cp_level2(char *src, char *dest)
{
    int fd, gd, n;
    char buf[BLKSIZE];

    fd = open_file(0/*file mode = READ*/); 
    if(fd == -1)
    {
        printf("cp(): src could not found\n");
        return -1;
    }

    char temp_pathname[BLKSIZE];
    strcpy(temp_pathname, pathname);
    strcpy(pathname, dest);

    //Open the src for read and open dest for write/create
    gd = open_file(1/*file mode = WRITE or CREAT if we have that.*/); 
    if(gd == -1)
    {
        printf("cp(): dest could not found\n");
        printf("cp(): creating dest file.\n");
        creat_file();
        printf("cp(): new dest file has been created.\n");
        gd = open_file(1/*file mode = WRITE or CREAT if we have that.*/);
        if(gd == -1)
        {
            printf("cp(): dest could not be found after creation\n");
            return -1;
        }
    }

    strcpy(pathname, temp_pathname);
    if(running->fd[fd]->mode != 0)
    {
        printf("cp(): src is not open for read; fd = %d   mode = %d\n", fd, running->fd[fd]->mode);
        return -1;
    }

    if(running->fd[gd]->mode != 1)
    {
        printf("cp(): dest is not open for W or CREAT; gd = %d   mode = %d\n",gd , running->fd[gd]->mode);
        return -1;
    }

    // The loop given in the instructions
    while(n = myread(fd, buf, BLKSIZE))
    {
        mywrite(gd, buf, n);
    }

    // Closing the files
    close_file(fd);
    close_file(gd);
}

int mv_level2(char *src, char *dest)
{
    int ino;
    // int dev;
    MINODE *mip;

    dev = src[0] == '/' ? root->dev : running->cwd->dev;
    printf("mv_level2(): pathname=%s     dev=%d\n", src, dev);
    // Verifying src exists
    ino = getino(src, &dev);

    if (ino == 0)
    {
        printf("mv_level2: src does not exist\n");
    }

    //Check whehter src is on the same dev as src? what...
    mip = iget(dev, ino);

    // Need to finish this peice right here
    if (mip->dev == running->cwd->dev)
    {
        // 3. Hard link dst with src (i.e. same INODE number)
        link(src, dest);

        // 4. unlink src (i.e. rm src name from its parent directory and reduce INODE's
        //    link count by 1).
        unlink(src);
    }
    else
    {
        // 3. cp src to dst
        cp_level2(src, dest);

        // 4. unlink src
        unlink(src);
    }

    iput(mip);
}
