/************* cd_ls_pwd.c file **************/

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
extern int prev_fs;

#define OWNER  000700
#define GROUP  000070
#define OTHER  000007

change_dir()
{
  if(!pathname || strlen(pathname) < 1) // Check to make sure the pathname is not empty
  {
    running->cwd = root; // If empty, cwd is the root
    return 1;
  }

  // local variable definitions
  int ino, r = 0;
  MINODE *mip;

  printf("current cwd is: dev=%d  ino=%d\n", running->cwd->dev, running->cwd->ino);
  dev = pathname[0] == '/' ? root->dev : running->cwd->dev;
  printf("change_dir(): pathname=%s     dev=%d\n", pathname, dev);

  ino = getino(pathname, &dev); // Gather the ino of the pathname

  if(!ino) // Check to make sure ino is valid
  {
    printf("Could not find the inode\n");
    return 0;
  }

  mip = iget(dev, ino); // Return a pointer to the in-memory containing the INODE

  if(!mip) // Check to make sure the INODE is valid
  {
    printf("inode does not exit!\n");
    return 0;
  }

  //verify that mip->inode is a DIR
  if(should_print)
  {
    printf("i_mode = %d\n\n", mip->INODE.i_mode);
  }
  
  if(mip->INODE.i_mode != DIR_TYPE) // Verifying the mode is DIR
  {
    printf("The pathname is not a dir\n");
    return 0;
  }

  r = iput(running->cwd); // Release the used minode pointed by mip

  running->cwd = mip; // assign the cwd to minode that contains the INODE
  printf("new cwd is: dev=%d  ino=%d\n", running->cwd->dev, running->cwd->ino);

  return 1;
}

int list_file() // Creating a temporary to pass into the ls function
{
  printf("\n\n");
  char temp[BLKSIZE];
  strncpy(temp, pathname, strlen(pathname));
  temp[strlen(pathname)] = 0;
  
  if(should_print)
  {
    printf("temp-pathname = %s\n", temp);
  }
  ls(temp);
}

int ls(char *pathname)
{
  int r, ino;
	char* filename, path[1024], cwd[256];
	filename = "./"; //default to cwd

  if(pathname[0] == '/') // If the pathname is a root, then assign it to the original root
  {
    if(should_print)
    {
      printf("path is root\n");
    }
    dev = root->dev; // Reassign the dev to the root's dev
  }
  else
  {
    if(should_print)
    {
      printf("path is not root\n");
    }
    dev = running->cwd->dev; // Reassign the dev to the cwd's dev
  }
  
	if(strlen(pathname) >= 1) // Check if the pathname is populated
	{
    filename = (char*)malloc(strlen(pathname + 1) * sizeof(char));
    strcpy(filename, pathname);
    ino = getino(filename, &dev); // Gather the ino of the filename
	}
	else
  {
		ino = running->cwd->ino; // ino becomes the cwd's ino
	}

  MINODE *mip = iget(dev, ino); // Return a pointer to the in-memory containing the INODE

	if(mip->INODE.i_size == 0) // If the size of the inode is 0, no file exists
	{
		printf("No such file %s: \n", filename);
    return 0;
	}
	strcpy(path, filename); // Create a copy of the file name

	if(mip->INODE.i_mode == 0x41ED) // Check that the mode is a directory
	{	
    if(should_print)
    {
      printf("offical pathname into ls_dir() = %s\n\n", path);
    }

		ls_dir(path); //call ls_dir if parent is a directory.
	}
	else // This is a file
  {
    if(should_print)
    {
      printf("offical pathname into ls_file() = %s\n\n", path);
    }

		ls_file(ino, path); //call ls_file if parent is a file.
	}
  iput(mip); // Release the used minode pointed by mip
	return 1;
}

int ls_file(int ino, char *name)
{
  int i;
  char ftime[64];
  MINODE *mip = iget(dev, ino); // get the pointer that contains the INODE of ino
  INODE *ip = &(mip->INODE);

  // print relevant information regarding the mode of the file/dir
  if ((ip->i_mode & 0xF000) == 0x8000)
  {
      printf("%c", '-');
  }
  if ((ip->i_mode & 0xF000) == 0x4000)
  {
      printf("%c", 'd');
  }
  if ((ip->i_mode & 0xF000) == 0xA000)
  {
      printf("%c", 'l');
  }

  // Get the permission of the file/dir
  for (i = 8; i >= 0; i--)
  {
    if (ip->i_mode & (1 << i))
    {
      printf("%c", t1[i]);
    }
    else
    {
      printf("%c", t2[i]);
    }
  }

  // Print the useful information of the file
  printf("%4d ", ip->i_links_count);
  printf("%4d ", ip->i_gid);
  printf("%4d ", ip->i_uid);
  printf("%8d ", ip->i_size);


  // Print the time the file was last modified
  strcpy(ftime, ctime(&ip->i_ctime));
  ftime[strlen(ftime) - 1] = 0;
  printf("%s  ", ftime);

  // If there is more contents, display a link
  printf("%s", basename(name));

  if ((ip->i_mode & 0xF000) == 0xA000)
  {
      printf(" -> %s", read_link(name));
  }
  printf("\n");
  iput(mip); // Release the used minode pointed by mip
}

int ls_dir(char *dirname)
{
  int ino;
  MINODE *mip;
  // int dev;
  char buf[BLKSIZE];

  dev = dirname[0] == '/' ? root->dev : running->cwd->dev;
  printf("ls_dir(): pathname=%s     dev=%d\n", dirname, dev);

  ino = getino(dirname, &dev); //gets the inode number for the directory

  if(!ino)
  {
    printf("ls_dir could not open %s\n", dirname);
    return 0;
  }

  // get the actual inode (well minode) for the directory.
  mip = iget(dev, ino);

  if(!mip) // Check that the mip is valid
  {
    printf("ls_dir could not get inode given the inode number : %d\n", ino);
    return 0;
  }

  int i;
  for(i=0; i < 12 && mip->INODE.i_block[i]; i++)
  {
    get_block(dev, mip->INODE.i_block[i], buf); //get the first data block

    char *cp, temp[256];
    cp = buf;

    DIR* dp;
    dp = (DIR *)buf;

    //Check through the first directory entry and call ls_file for it.
    while(cp < buf + 1024)
    {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0; // remove newline

      if(strcmp(temp, ".") && strcmp(temp, "..")) // if not "." and "..", continue
      {
        ls_file(dp->inode, temp); // call ls_file to print contents
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }

    iput(mip); // Release the used minode pointed by mip
  }
}

int pwd(MINODE *wd)
{
  MOUNT* r_mtptr = root->mptr;
  char temp[256];
  if(wd == root) // If the wd is the root
  {
      printf("%s\n", r_mtptr->mount_name);
  }
  else // Otherwise, pass into a recursive pwd function
  {
    rpwd(wd, temp);
    printf("%s\n", temp);
  }
}

 int rpwd(MINODE *wd, char *temp)
{
  printf("print test\n");
  char myname[256];
  printf("\n\n");

  printf("rpwd(): wd->dev=%d  temp=%s\n", wd->dev, temp);
  if (wd == root) // If root, return 0
  {
    int index;
    for(index = 0; index < NFS_MOUNT && mountTable[index].mounted_inode; index++)
    {
      if(root->dev == mountTable[index].dev)
      {
        break;
      }
    }

    printf("root->mount_name\n");
    if(strcmp(mountTable[index].mount_name, "/"))
    {
      printf("Not root\n");
      printf("%s", mountTable[index].mount_name);
    }
    // printf("/");
    return 0;
  }

  int myino, parentino, r =0;
  MINODE *pip;

  if(should_print)
  {
    printf("getting myino\n");
  }
  
  myino = getino(".", &dev); // get my ino by searching for "." in wd

  if(!myino) // Check the myino is valid
  {
    printf("rpwd(): could not find %s\n", ".");
    return 0;
  }

  if(should_print)
  {
    printf("getting parentino\n");
  }

  parentino = getino("..", &dev);// get parentino by searching for ".." in wd

  if(!parentino) // Check that parentino is valid
  {
    printf("rpwd(): could not find parent of %s\n", "..");
    return 0;
  }

  if(should_print)
  {
    printf("getting pip\n");
  }
  pip = iget(dev, parentino); // get the pointer that contains the INODE of parentino

  if(should_print)
  {
    printf("getting my_name\n");
  }
  r = get_myname(pip, myino, myname); // returns the name string of a dir_entry identified by myino
  
  printf("rpwd(): parent->dev=%d  parent_name=%s\n", pip->dev, myname);
  rpwd(pip, temp);  // recursive call rpwd() with pip

  strcat(temp, "/");
  strcat(temp, myname);
}