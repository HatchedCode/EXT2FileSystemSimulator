# EXT2 File System Simulator
This program is suppose to simulate the EXT2 File system with the commands below.

# Files and Functions
type.h, util.c\
main.c         : initilization, mountroot, command processing loop\
cd_ls_pwd.c    : ls, cd, pwd \
mkdir_creat.c  : mkdir, creat, enter_name\
rmdir.c        : rmdir, rm_child \
link_unlink.c  : link, unlink\
symlink.c      : symlink, readlink\
open_close.c   : open, close, lseek, pfd\
read_cat.c     : read, cat \
write_cp.c     : write, cp \
mount_umount.c : mount, umount\
getino() and pwd() for cross mounting points\
rmdir() and unlink() to check permission
