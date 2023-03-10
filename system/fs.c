#include <xinu.h>
#include <kernel.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef FS
#include <fs.h>

static fsystem_t fsd;
int dev0_numblocks;
int dev0_blocksize;
char *dev0_blocks;

extern int dev0;

char block_cache[512];

int _fs_get_inode_by_num(int dev, int inode_number, inode_t *out);
int _fs_put_inode_by_num(int dev, int inode_number, inode_t *in);

#define SB_BLK 0 // Superblock
#define BM_BLK 1 // Bitmapblock

#define NUM_FD 16

filetable_t oft[NUM_FD]; // open file table
#define isbadfd(fd) (fd < 0 || fd >= NUM_FD || oft[fd].in.id == EMPTY)

#define INODES_PER_BLOCK (fsd.blocksz / sizeof(inode_t))
#define NUM_INODE_BLOCKS (( (fsd.ninodes % INODES_PER_BLOCK) == 0) ? fsd.ninodes / INODES_PER_BLOCK : (fsd.ninodes / INODES_PER_BLOCK) + 1)
#define FIRST_INODE_BLOCK 2

/**
 * Helper functions
 */
int _fs_fileblock_to_diskblock(int dev, int fd, int fileblock) {
  int diskblock;

  if (fileblock >= INODEDIRECTBLOCKS) {
    errormsg("No indirect block support! (%d >= %d)\n", fileblock, INODEBLOCKS - 2);
    return SYSERR;
  }

  // Get the logical block address
  diskblock = oft[fd].in.blocks[fileblock];

  return diskblock;
}

/**
 * Filesystem functions
 */
int _fs_get_inode_by_num(int dev, int inode_number, inode_t *out) {
  int bl, inn;
  int inode_off;

  if (dev != dev0) {
    errormsg("Unsupported device: %d\n", dev);
    return SYSERR;
  }
  if (inode_number >= fsd.ninodes) {
    errormsg("inode %d out of range (> %s)\n", inode_number, fsd.ninodes);
    return SYSERR;
  }

  bl  = inode_number / INODES_PER_BLOCK;
  inn = inode_number % INODES_PER_BLOCK;
  bl += FIRST_INODE_BLOCK;

  inode_off = inn * sizeof(inode_t);

  bs_bread(dev0, bl, 0, &block_cache[0], fsd.blocksz);
  memcpy(out, &block_cache[inode_off], sizeof(inode_t));

  return OK;

}

int _fs_put_inode_by_num(int dev, int inode_number, inode_t *in) {
  int bl, inn;

  if (dev != dev0) {
    errormsg("Unsupported device: %d\n", dev);
    return SYSERR;
  }
  if (inode_number >= fsd.ninodes) {
    errormsg("inode %d out of range (> %d)\n", inode_number, fsd.ninodes);
    return SYSERR;
  }

  bl = inode_number / INODES_PER_BLOCK;
  inn = inode_number % INODES_PER_BLOCK;
  bl += FIRST_INODE_BLOCK;

  bs_bread(dev0, bl, 0, block_cache, fsd.blocksz);
  memcpy(&block_cache[(inn*sizeof(inode_t))], in, sizeof(inode_t));
  bs_bwrite(dev0, bl, 0, block_cache, fsd.blocksz);

  return OK;
}

int fs_mkfs(int dev, int num_inodes) {
  int i;

  if (dev == dev0) {
    fsd.nblocks = dev0_numblocks;
    fsd.blocksz = dev0_blocksize;
  } else {
    errormsg("Unsupported device: %d\n", dev);
    return SYSERR;
  }

  if (num_inodes < 1) {
    fsd.ninodes = DEFAULT_NUM_INODES;
  } else {
    fsd.ninodes = num_inodes;
  }

  i = fsd.nblocks;
  while ( (i % 8) != 0) { i++; }
  fsd.freemaskbytes = i / 8;

  if ((fsd.freemask = getmem(fsd.freemaskbytes)) == (void *) SYSERR) {
    errormsg("fs_mkfs memget failed\n");
    return SYSERR;
  }

  /* zero the free mask */
  for(i = 0; i < fsd.freemaskbytes; i++) {
    fsd.freemask[i] = '\0';
  }

  fsd.inodes_used = 0;

  /* write the fsystem block to SB_BLK, mark block used */
  fs_setmaskbit(SB_BLK);
  bs_bwrite(dev0, SB_BLK, 0, &fsd, sizeof(fsystem_t));

  /* write the free block bitmask in BM_BLK, mark block used */
  fs_setmaskbit(BM_BLK);
  bs_bwrite(dev0, BM_BLK, 0, fsd.freemask, fsd.freemaskbytes);

  // Initialize all inode IDs to EMPTY
  inode_t tmp_in;
  for (i = 0; i < fsd.ninodes; i++) {
    _fs_get_inode_by_num(dev0, i, &tmp_in);
    tmp_in.id = EMPTY;
    _fs_put_inode_by_num(dev0, i, &tmp_in);
  }
  fsd.root_dir.numentries = 0;
  for (i = 0; i < DIRECTORY_SIZE; i++) {
    fsd.root_dir.entry[i].inode_num = EMPTY;
    memset(fsd.root_dir.entry[i].name, 0, FILENAMELEN);
  }

  for (i = 0; i < NUM_FD; i++) {
    oft[i].state     = 0;
    oft[i].fileptr   = 0;
    oft[i].de        = NULL;
    oft[i].in.id     = EMPTY;
    oft[i].in.type   = 0;
    oft[i].in.nlink  = 0;
    oft[i].in.device = 0;
    oft[i].in.size   = 0;
    memset(oft[i].in.blocks, 0, sizeof(oft[i].in.blocks));
    oft[i].flag      = 0;
  }

  return OK;
}

int fs_freefs(int dev) {
  if (freemem(fsd.freemask, fsd.freemaskbytes) == SYSERR) {
    return SYSERR;
  }

  return OK;
}

/**
 * Debugging functions
 */
void fs_print_oft(void) {
  int i;

  printf ("\n\033[35moft[]\033[39m\n");
  printf ("%3s  %5s  %7s  %8s  %6s  %5s  %4s  %s\n", "Num", "state", "fileptr", "de", "de.num", "in.id", "flag", "de.name");
  for (i = 0; i < NUM_FD; i++) {
    if (oft[i].de != NULL) printf ("%3d  %5d  %7d  %8d  %6d  %5d  %4d  %s\n", i, oft[i].state, oft[i].fileptr, oft[i].de, oft[i].de->inode_num, oft[i].in.id, oft[i].flag, oft[i].de->name);
  }

  printf ("\n\033[35mfsd.root_dir.entry[] (numentries: %d)\033[39m\n", fsd.root_dir.numentries);
  printf ("%3s  %3s  %s\n", "ID", "id", "filename");
  for (i = 0; i < DIRECTORY_SIZE; i++) {
    if (fsd.root_dir.entry[i].inode_num != EMPTY) printf("%3d  %3d  %s\n", i, fsd.root_dir.entry[i].inode_num, fsd.root_dir.entry[i].name);
  }
  printf("\n");
}

void fs_print_inode(int fd) {
  int i;

  printf("\n\033[35mInode FS=%d\033[39m\n", fd);
  printf("Name:    %s\n", oft[fd].de->name);
  printf("State:   %d\n", oft[fd].state);
  printf("Flag:    %d\n", oft[fd].flag);
  printf("Fileptr: %d\n", oft[fd].fileptr);
  printf("Type:    %d\n", oft[fd].in.type);
  printf("nlink:   %d\n", oft[fd].in.nlink);
  printf("device:  %d\n", oft[fd].in.device);
  printf("size:    %d\n", oft[fd].in.size);
  printf("blocks: ");
  for (i = 0; i < INODEBLOCKS; i++) {
    printf(" %d", oft[fd].in.blocks[i]);
  }
  printf("\n");
  return;
}

void fs_print_fsd(void) {
  int i;

  printf("\033[35mfsystem_t fsd\033[39m\n");
  printf("fsd.nblocks:       %d\n", fsd.nblocks);
  printf("fsd.blocksz:       %d\n", fsd.blocksz);
  printf("fsd.ninodes:       %d\n", fsd.ninodes);
  printf("fsd.inodes_used:   %d\n", fsd.inodes_used);
  printf("fsd.freemaskbytes  %d\n", fsd.freemaskbytes);
  printf("sizeof(inode_t):   %d\n", sizeof(inode_t));
  printf("INODES_PER_BLOCK:  %d\n", INODES_PER_BLOCK);
  printf("NUM_INODE_BLOCKS:  %d\n", NUM_INODE_BLOCKS);

  inode_t tmp_in;
  printf ("\n\033[35mBlocks\033[39m\n");
  printf ("%3s  %3s  %4s  %4s  %3s  %4s\n", "Num", "id", "type", "nlnk", "dev", "size");
  for (i = 0; i < NUM_FD; i++) {
    _fs_get_inode_by_num(dev0, i, &tmp_in);
    if (tmp_in.id != EMPTY) printf("%3d  %3d  %4d  %4d  %3d  %4d\n", i, tmp_in.id, tmp_in.type, tmp_in.nlink, tmp_in.device, tmp_in.size);
  }
  for (i = NUM_FD; i < fsd.ninodes; i++) {
    _fs_get_inode_by_num(dev0, i, &tmp_in);
    if (tmp_in.id != EMPTY) {
      printf("%3d:", i);
      int j;
      for (j = 0; j < 64; j++) {
        printf(" %3d", *(((char *) &tmp_in) + j));
      }
      printf("\n");
    }
  }
  printf("\n");
}

void fs_print_dir(void) {
  int i;

  printf("%22s  %9s  %s\n", "DirectoryEntry", "inode_num", "name");
  for (i = 0; i < DIRECTORY_SIZE; i++) {
    printf("fsd.root_dir.entry[%2d]  %9d  %s\n", i, fsd.root_dir.entry[i].inode_num, fsd.root_dir.entry[i].name);
  }
}

int fs_setmaskbit(int b) {
  int mbyte, mbit;
  mbyte = b / 8;
  mbit = b % 8;

  fsd.freemask[mbyte] |= (0x80 >> mbit);
  return OK;
}

int fs_getmaskbit(int b) {
  int mbyte, mbit;
  mbyte = b / 8;
  mbit = b % 8;

  return( ( (fsd.freemask[mbyte] << mbit) & 0x80 ) >> 7);
}

int fs_clearmaskbit(int b) {
  int mbyte, mbit, invb;
  mbyte = b / 8;
  mbit = b % 8;

  invb = ~(0x80 >> mbit);
  invb &= 0xFF;

  fsd.freemask[mbyte] &= invb;
  return OK;
}

/**
 * This is maybe a little overcomplicated since the lowest-numbered
 * block is indicated in the high-order bit.  Shift the byte by j
 * positions to make the match in bit7 (the 8th bit) and then shift
 * that value 7 times to the low-order bit to print.  Yes, it could be
 * the other way...
 */
void fs_printfreemask(void) { // print block bitmask
  int i, j;

  for (i = 0; i < fsd.freemaskbytes; i++) {
    for (j = 0; j < 8; j++) {
      printf("%d", ((fsd.freemask[i] << j) & 0x80) >> 7);
    }
    printf(" ");
    if ( (i % 8) == 7) {
      printf("\n");
    }
  }
  printf("\n");
}


/**
 * TODO: implement the functions below
 */
int fs_open(char *filename, int flags) {
    if ((flags != O_WRONLY && flags != O_RDONLY  && flags != O_RDWR) || fsd.root_dir.numentries <= 0) {
        return SYSERR;
    }
    int i;
    struct inode nd;
    int fdindex=-1;
    for(i=0;i<NUM_FD;i++){
      if(oft[i].state==FSTATE_CLOSED){
        fdindex=i;
        break;
      }
    }
    if(fdindex==-1){
      return SYSERR;
    }

    for (i = 0; i < DIRECTORY_SIZE; i++) {
        if (strcmp(fsd.root_dir.entry[i].name, filename) == 0) {

          int tp = fsd.root_dir.entry[i].inode_num;
          int geterr;
          geterr=_fs_get_inode_by_num(0, tp, &nd);
          if(geterr==SYSERR){
            return SYSERR;
          }
          oft[fdindex].state = FSTATE_OPEN;
          oft[fdindex].flag = flags;
          oft[fdindex].fileptr = 0;
          oft[fdindex].in = nd;
          oft[fdindex].de = &fsd.root_dir.entry[i];
          _fs_put_inode_by_num(0, tp, &oft[i].in);
          return fdindex;
        }
    }
    return SYSERR;
}

int fs_close(int fd) {
  if (oft[fd].state == FSTATE_CLOSED || fd < 0 || fd >= NUM_FD)  {
    return SYSERR;
  }
    // oft[fd].fileptr = 0;
    oft[fd].state = FSTATE_CLOSED;
    return OK; 
}

int fs_create(char *filename, int mode) {
    if(mode != O_CREAT){
      return SYSERR;
    }
    if(fsd.root_dir.numentries < DIRECTORY_SIZE){
      int i;
      if(strlen(filename)>FILENAMELEN){
      return SYSERR;
      }
      for(i=0; i < fsd.root_dir.numentries; i++) {
            if (strcmp(filename, fsd.root_dir.entry[i].name) == 0) {
                return SYSERR;
            }
      }
      int found=0;
      int nextfreeentry;
      for(nextfreeentry=0; nextfreeentry < DIRECTORY_SIZE; nextfreeentry++) {
          if(fsd.root_dir.entry[nextfreeentry].inode_num==EMPTY){
            found=1;
            break;
          }
      }
      if(!found){
        return SYSERR;
      }

      struct inode tt;
      found=0;
      for (i = 1; i < fsd.ninodes; i++) {
        _fs_get_inode_by_num(dev0, i, &tt);
        if(tt.id == EMPTY){
          found=1;
          break;
        }
      }

      if(!found){
        return SYSERR;
      }
      
      // int stts = _fs_get_inode_by_num(0, fsd.inodes_used, &tt);
      // if (stts == SYSERR) {
      //   return SYSERR;
      // }

      tt.size = 0;
      tt.nlink = 1;
      tt.id = i;
      tt.type = INODE_TYPE_FILE;
      tt.device = dev0;

      fsd.root_dir.entry[nextfreeentry].inode_num = tt.id;
      strcpy(fsd.root_dir.entry[nextfreeentry].name, filename);
      fsd.root_dir.numentries++;

      int ptsts = _fs_put_inode_by_num(0, tt.id, &tt);
      if (ptsts == SYSERR) {
        return SYSERR;
      }

      fsd.inodes_used++;
      int retfd=fs_open(filename, O_RDWR);
      if(retfd==SYSERR){
        return SYSERR;
      }
      return retfd;
    }
    return SYSERR;

}

int fs_seek(int fd, int offset) {
  if(fd > NUM_FD){
    return SYSERR;
  }
  else if(fd < 0){
    return SYSERR; 
  }
  else if(oft[fd].state != FSTATE_OPEN){
    return SYSERR; 
  }
  else if(offset > oft[fd].in.size){
    return SYSERR; 
  }
  else if(offset < 0){
    return SYSERR; 
  }

  oft[fd].fileptr = offset;

  return OK;
}

int fs_read(int fd, void *buf, int nbytes) {
  if(fd > NUM_FD){
    return SYSERR;
  }
  else if(fd < 0){
    return SYSERR; 
  }
  else if(oft[fd].state != FSTATE_OPEN){
    return SYSERR; 
  }
  else if(nbytes < 0){
    return SYSERR; 
  }
  else if(oft[fd].flag == O_WRONLY){
    return SYSERR; 
  }
  else if(oft[fd].state != FSTATE_OPEN){
    return SYSERR; 
  }

  
  if(oft[fd].in.size - oft[fd].fileptr<nbytes){
    nbytes = (oft[fd].in.size - oft[fd].fileptr);
  }
  if(oft[fd].in.size * INODEDIRECTBLOCKS<nbytes){
    nbytes = oft[fd].in.size * INODEDIRECTBLOCKS;
  }
  int of=oft[fd].fileptr % fsd.blocksz;
  int nbtcp = nbytes,n_count=fsd.blocksz-of,bk=oft[fd].fileptr / fsd.blocksz;
  char* buffptr = buf;
  
  while(1==1){
    if(nbytes <= 0){
      break;
    }
    else if(n_count>0){
      int rd = (nbytes>n_count) ? n_count:nbytes;
      bs_bread(dev0, oft[fd].in.blocks[bk], of, buffptr, rd);
      of = (of + rd) % fsd.blocksz;
      nbytes -= rd;
      buffptr += rd;
    }
    n_count = fsd.blocksz;
    bk++;
  }
  oft[fd].fileptr += nbtcp;
  return nbtcp;
}

int fs_write(int fd, void *buf, int nbytes) {
  if (fd < 0 || fd > NUM_FD || oft[fd].flag == O_RDONLY || oft[fd].state != FSTATE_OPEN || nbytes < 0){
    return SYSERR;
  }
 
  nbytes = (nbytes>5120 - oft[fd].fileptr)?5120 - oft[fd].fileptr:nbytes;

  if(buf == NULL){
    return 0;
  }
  if(nbytes==0){
    return 0;
  }

  int t=0;
  while(1==1){
    if(nbytes<=0){
      break;
    }
    int wrtcont=0,bk=oft[fd].fileptr / fsd.blocksz,hd=oft[fd].fileptr % fsd.blocksz;
    int amt=fsd.blocksz - (hd % fsd.blocksz);
    wrtcont = (nbytes <= amt)?nbytes:amt;

    if(oft[fd].in.blocks[bk] == 0){
      int extr=0;
      int i;
      for(i=18; i < fsd.nblocks; i++){
        if (fs_getmaskbit(i) == 0) {
          fs_setmaskbit(i);
          oft[fd].in.blocks[bk] = i;
          inode_t tt;
          _fs_get_inode_by_num(dev0, oft[fd].in.id, &tt);
          tt.blocks[bk] = i;
          _fs_put_inode_by_num(dev0, oft[fd].in.id, &tt);

          if (bs_bwrite(dev0, oft[fd].in.blocks[bk], hd, buf, wrtcont) == SYSERR) {
            return SYSERR;
          } 
          extr+=1;
          break;
        }
      }
      if(!extr){
        return SYSERR;
      }
    }
      else if (bs_bwrite(dev0, oft[fd].in.blocks[bk], hd, buf, wrtcont) == SYSERR){
        return SYSERR;
      }

      oft[fd].fileptr += wrtcont;
      buf += wrtcont;
      t+=wrtcont;
      nbytes -= wrtcont;
  }
      if(oft[fd].in.size < oft[fd].fileptr) {
        // reset size
        oft[fd].in.size = oft[fd].fileptr;
        inode_t tt;
        _fs_get_inode_by_num(dev0, oft[fd].in.id, &tt);
        tt.size = oft[fd].fileptr;
        _fs_put_inode_by_num(dev0, oft[fd].in.id, &tt);
      }

return t;
    
}

int fs_link(char *src_filename, char* dst_filename) {

    int i;
    if(strlen(src_filename)>FILENAMELEN || strlen(dst_filename)>FILENAMELEN){
      return SYSERR;
    }
    // check for duplicate
    if (strcmp(src_filename, dst_filename) == 0) {
        return SYSERR;
    }
    for(i=0; i < DIRECTORY_SIZE; i++) {
        if (strcmp(dst_filename, fsd.root_dir.entry[i].name) == 0) {
            return SYSERR;
        }
    }
    

    if(fsd.root_dir.numentries==DIRECTORY_SIZE){
        return SYSERR;
    }
    int nextfreeentry=-1;
    for(nextfreeentry=0; nextfreeentry < DIRECTORY_SIZE; nextfreeentry++) {
        if(fsd.root_dir.entry[nextfreeentry].inode_num==EMPTY){
          break;
        }
    }
    if(nextfreeentry==-1){
      return SYSERR;
    }
    for(i=0; i < DIRECTORY_SIZE; i++) {
        if(strcmp(src_filename, fsd.root_dir.entry[i].name) == 0) {
          strcpy(fsd.root_dir.entry[nextfreeentry].name, dst_filename);
          fsd.root_dir.entry[nextfreeentry].inode_num = fsd.root_dir.entry[i].inode_num;
          fsd.root_dir.numentries+=1;
          inode_t tempnode;
          int geterr;
          geterr=_fs_get_inode_by_num(dev0, fsd.root_dir.entry[i].inode_num, &tempnode);
          if(geterr==SYSERR){
              return SYSERR;
          }   
          tempnode.nlink+=1;
           geterr=_fs_put_inode_by_num(dev0, fsd.root_dir.entry[i].inode_num, &tempnode);
           if(geterr==SYSERR){
              return SYSERR;
          }  
          return OK;
        }
    }


    return SYSERR;
  
}

int fs_unlink(char *filename) {
    int i;
    if(strlen(filename)>FILENAMELEN){
      return SYSERR;
    }
    int found=0;
    for(i=0; i < DIRECTORY_SIZE; i++) {
        if (strcmp(filename, fsd.root_dir.entry[i].name) == 0) {
            found=1;
            break;
        }
    }
    if(!found){
      return SYSERR;
    }
    // now our filename is found and exists at index i

    inode_t tempnode;
    int inodeid=fsd.root_dir.entry[i].inode_num,geterr;
    geterr=_fs_get_inode_by_num(dev0, inodeid, &tempnode);
    if(geterr==SYSERR){
      return SYSERR;
    }

    if(tempnode.nlink==1){
        tempnode.id=EMPTY;
        tempnode.nlink=0;
        memset(tempnode.blocks, 0, sizeof(tempnode.blocks));
        tempnode.type   = 0;
        tempnode.device = 0;
        tempnode.size   = 0;
        fsd.inodes_used -=1;
        geterr=_fs_put_inode_by_num(dev0, inodeid, &tempnode);


        memset(fsd.root_dir.entry[i].name, 0, FILENAMELEN);
        fsd.root_dir.entry[i].inode_num=EMPTY;
        if(geterr==SYSERR){
                    return SYSERR;
        }
        fsd.root_dir.numentries-=1;
        
        int j;
        for (j = 0; j < NUM_FD; j++) {
          if(inodeid==oft[j].in.id){
            oft[j].state     = 0;
            oft[j].fileptr   = 0;
            oft[j].de        = NULL;
            oft[j].in.type   = 0;
            oft[j].in.nlink  = 0;
            oft[j].in.device = 0;
            oft[j].in.size   = 0;
            oft[j].flag = 0;
            memset(oft[j].in.blocks, 0, sizeof(oft[j].in.blocks));
            }
        }
        
        
        
        return OK;
    }

    if(tempnode.nlink>1){
      memset(fsd.root_dir.entry[i].name, 0, FILENAMELEN);
      fsd.root_dir.entry[i].inode_num=EMPTY;
      tempnode.nlink-=1;
      
      geterr=_fs_put_inode_by_num(dev0, inodeid, &tempnode);
      if(geterr==SYSERR){
            return SYSERR;
        }
      fsd.root_dir.numentries-=1;
      int j;
      for (j = 0; j < NUM_FD; j++) {
        if(inodeid==oft[j].in.id){
          oft[j].state     = 0;
          oft[j].fileptr   = 0;
          oft[j].de        = NULL;
          oft[j].in.type   = 0;
          oft[j].in.nlink  = 0;
          oft[j].in.device = 0;
          oft[j].in.size   = 0;
          oft[j].flag = 0;
          memset(oft[j].in.blocks, 0, sizeof(oft[j].in.blocks));
          }
        }
      return OK;
    }
  return SYSERR;
}

#endif /* FS */