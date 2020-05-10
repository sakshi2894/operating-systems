#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define stat xv6_stat      // avoid clash with host struct stat
#define dirent xv6_dirent  // avoid clash with host struct stat
#include "fs.h"
#include "stat.h"
#include "types.h"
#undef stat
#undef dirent


// Utility functions

void *GetAddressOfBlock(void *img_ptr, uint block_num) {
  return (img_ptr + (block_num * BSIZE));
}

void MarkBlockInUse(int block_num, char *my_bitblock) {
  char mask = 0x1 << (block_num % 8);
  my_bitblock[block_num / 8] = my_bitblock[block_num / 8] | mask;
}


int IsDataBlockAddressConsistent(uint addr, uint datablock_start, int total_blocks) {
  if (addr == 0 || (datablock_start <= addr && addr < total_blocks)) {  //Check 3
    return 1;
  }
  return 0;
}

/**
 * Utility function to clear the visited nodes.
 * @param visited
 * @param num_inodes
 */
void ClearVisited(int *visited, unsigned long num_inodes) {
  for (int i = 0; i < num_inodes; i++) {
    visited[i] = 0;
  }
}

void ClearInUseNumLinks(int * used_in_directory, short* num_links, unsigned long num_inodes) {
  for (int i = 0; i < num_inodes; i++) {
    used_in_directory[i] = 0;
    num_links[i] = 0;
  }
  num_links[1] = 1;

}

/**
 * Populates "used_in_directory[]" and "links[]" DS for checks 9, 10, 11, 12
 * used_in_directory:
 * used_in_directory[inode_num] = 1 if inode_num is being pointed to by one of the directories in the system.
 * Ignore '.' entries (not '..') based on post https://piazza.com/class/k4bkdfbqqziw5?cid=1510
 *
 * num_links:
 * num_links[inode_num] = count of the number of times inode_num has been referenced by other directories in the system.
 * Ignore '.' and '..' entries.
 * @param img_ptr
 * @param data_block_addr
 * @param used_in_directory
 * @param num_links
 */
void MarkUsedInDirectory(void *img_ptr, uint data_block_addr, int* used_in_directory, short* num_links) {
  struct xv6_dirent *entry = (struct xv6_dirent *) GetAddressOfBlock(img_ptr, data_block_addr);
  int dirent_num = BSIZE / sizeof(struct xv6_dirent);
  for (int i = 0; i < dirent_num; i++) {
    if (strcmp(entry[i].name, ".") != 0) {
      used_in_directory[entry[i].inum] = 1;
      if (strcmp(entry[i].name, "..") != 0) {
        //TODO: Add a check to see if inum is 0?
        num_links[entry[i].inum] = num_links[entry[i].inum] + 1;
        //printf("increased num link by count 1 for inode %d\n", entry[i].inum);
      }
    }
  }
}

void MarkAllUsedInDirectory(void *img_ptr, struct dinode *inode_ptr, unsigned long num_inodes,
                            int* used_in_directory, short* num_links) {
  for (int i = 0; i < num_inodes; i++) {
    if (inode_ptr[i].type == T_DIR) {
      // Iterate over all direct address blocks.
      for (int j = 0; j < NDIRECT; j++) {
        uint data_block_addr = inode_ptr[i].addrs[j];
        if (data_block_addr != 0) {
          MarkUsedInDirectory(img_ptr, data_block_addr, used_in_directory, num_links);
        }
      }

      uint i_data_block_addr = inode_ptr[i].addrs[NDIRECT];
      // Iterate over all indirect address blocks.
      if (i_data_block_addr != 0) {
        uint *indirect = (uint *)GetAddressOfBlock(img_ptr, i_data_block_addr);
        int i_datanum = BSIZE/ sizeof(uint);
        for (int j = 0; j < i_datanum; j++) {
          if (indirect[j] != 0) {
            MarkUsedInDirectory(img_ptr, indirect[j], used_in_directory, num_links);
          }
        }
      }
    }
  }

}

int GetInodeNum(void *img_ptr, uint data_block_addr, char * file_name) {
  struct xv6_dirent *entry = (struct xv6_dirent *) GetAddressOfBlock(img_ptr, data_block_addr);
  int dirent_num = BSIZE / sizeof(struct xv6_dirent);
  for (int i = 0; i < dirent_num; i++) {
    if (strcmp(entry[i].name, file_name) == 0) {
      return entry[i].inum;
    }
  }
}

void SetParentDirectory(void *img_ptr, uint data_block_addr, int new_inode) {
  struct xv6_dirent *entry = (struct xv6_dirent *) GetAddressOfBlock(img_ptr, data_block_addr);
  int dirent_num = BSIZE / sizeof(struct xv6_dirent);
  for (int i = 0; i < dirent_num; i++) {
    if (strcmp(entry[i].name, "..") == 0) {
      //printf("Changing entry[i].num from %d to %d\n", entry[i].inum, new_inode);
      entry[i].inum = new_inode;
    }
  }
}

int CheckIfCurrentNodeExists(void *img_ptr, uint data_block_addr, int inode_num) {
  struct xv6_dirent *entry = (struct xv6_dirent *) GetAddressOfBlock(img_ptr, data_block_addr);
  int dirent_num = BSIZE / sizeof(struct xv6_dirent);
  for (int i = 0; i < dirent_num; i++) {
    //printf("entry inum %d entry name %s for inode %d\n", entry[i].inum, entry[i].name, inode_num);
    // Avoiding '..' entries
    //TODO: Check if '.' entries should also be avoided?
    if (entry[i].inum == inode_num && strcmp(entry[i].name, "..") != 0) {
      return 1;
    }
  }
  return 0;
}
// Verification functions

/**
 * Check 1: For the metadata in the super block, the file-system size is larger than
 *          the number of blocks used by the super-block, inodes, bitmaps and data.
 * @param total_blocks
 * @param num_inodeblocks
 * @param num_bitblocks
 * @param num_datablocks
 */
//TODO: Check if two unused blocks should be considered - (1 from first unused and 2nd from one unused in bit blocks)
//Verified with Prof. He said its okay to add +2 while performing this check.
void VerifySuperBlockMetadata(int total_blocks, unsigned long num_inodeblocks, int num_bitblocks, int num_datablocks) {
  if (total_blocks < (3 + num_inodeblocks + num_bitblocks + num_datablocks)) {  //Check 1
    fprintf(stderr, "ERROR: superblock is corrupted.\n");
    exit(1);
  }
}


/**
 * Check 2: Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV).
 * @param inode
 */
void VerifyInodeType(struct dinode *inode) {
  if (inode->type != 0 && inode->type != T_DIR &&
      inode->type != T_FILE && inode->type != T_DEV) {  //Check 2
    fprintf(stderr, "ERROR: bad inode.\n");
    exit(1);
  }
}

/**
 * Check 7: For in-use inodes, each direct address in use is only used once.
 * While marking an in-use datanode, if we come across same data block number multiple times,
 * check 7 is marked as failed.
 * @param used_by_inode
 * @param inode_num
 * @param datablock_num
 */
void MarkInodeForDatablock(int *used_by_inode, int inode_num, int datablock_num) {
  if (used_by_inode[datablock_num] != 0) {  //Check 7
    fprintf(stderr, "ERROR: direct address used more than once.\n");
    exit(1);
  }
  used_by_inode[datablock_num] = inode_num;
}

/**
 * Check 3: For in-use inodes, each address that is used by inode is valid (points to a valid datablock address within the image).
 * @param img_ptr
 * @param inode
 * @param datablock_start
 * @param total_blocks
 * @param my_bitblock
 * @param inode_num
 * @param used_by_inode
 */
void VerifyInodeDataPointers(void *img_ptr, struct dinode *inode, uint datablock_start,
                             int total_blocks, char *my_bitblock, int inode_num, int *used_by_inode) {
  int i;
  for (i = 0; i < NDIRECT; i++) {
    if (!IsDataBlockAddressConsistent(inode->addrs[i], datablock_start, total_blocks)) {
      fprintf(stderr, "ERROR: bad direct address in inode.\n");
      exit(1);
    }
    if (inode->addrs[i] != 0) {
      MarkBlockInUse(inode->addrs[i], my_bitblock);
      MarkInodeForDatablock(used_by_inode, inode_num, inode->addrs[i]);
    }
  }

  if (!IsDataBlockAddressConsistent(inode->addrs[NDIRECT], datablock_start, total_blocks)) {
    fprintf(stderr, "ERROR: bad indirect address in inode.\n");
    exit(1);
  }

  //TODO: Check 7: Why not mark inode for datablocks pointed by indirect blocks and the indirect data block itself?
  //Not required. https://piazza.com/class/k4bkdfbqqziw5?cid=1415 - Adding for consistency anyway.
  if (inode->addrs[NDIRECT] != 0) {
    uint *indirect = (uint *) GetAddressOfBlock(img_ptr, inode->addrs[NDIRECT]);
    MarkBlockInUse(inode->addrs[NDIRECT], my_bitblock);
    MarkInodeForDatablock(used_by_inode, inode_num, inode->addrs[NDIRECT]);
    // TODO: Verify
    int i_datanum = BSIZE / sizeof(uint);
    for (i = 0; i < i_datanum; i++) {
      if (!IsDataBlockAddressConsistent(indirect[i], datablock_start, total_blocks)) {
        fprintf(stderr, "ERROR: bad indirect address in inode.\n");
        exit(1);
      }
      if (indirect[i] != 0) {
        MarkBlockInUse(indirect[i], my_bitblock);
        MarkInodeForDatablock(used_by_inode, inode_num, indirect[i]);
      }
    }
  }
}


/**
 * Check 4: Each directory contains . and .. entries, and the . entry points to the directory itself.
 * Checks first two entries in data block of inode being checked.
 *  Mentioned nowhere that for root we should verify .. also.
 * @param img_ptr: pointer to image
 * @param inode : pointer to inode being verifed.
 * @param inode_num : id/number of inode in consideration.
 */
void VerifyDotEntries(void *img_ptr, struct dinode *inode, int inode_num) {
  int verified_count = 0;
  struct xv6_dirent *de;

  if (inode->addrs[0] != 0) {
    de = (struct xv6_dirent *)GetAddressOfBlock(img_ptr, inode->addrs[0]);
    if (strcmp(de->name, ".") == 0 && de->inum == inode_num) {  //Check 4
      verified_count++;
    }

    de++;
    if (strcmp(de->name, "..") == 0) {  //Check 4
      verified_count++;
    }
  }
  if (verified_count != 2) {
    fprintf(stderr, "ERROR: directory not properly formatted.\n");
    exit(1);
  }
}


/**
 * Check 5: For in-use inodes, each address in use is also marked in-use in the bitmap.
 * Check 6: For blocks marked in-use in bitmap, the block should actually be in-use in
 *          an inode or indirect block somewhere.
 * @param bb_addr
 * @param my_bitblock
 * @param datablock_start
 */
void VerifyBitmap(char *bb_addr, char *my_bitblock, int datablock_start) {
  int i, mask;
  //BPB: bits per block: BSIZE * 8
  for (i = datablock_start; i < BPB; i++) {
    mask = 0x1 << (i % 8);
    if ((bb_addr[i / 8] & mask) == 0 && (my_bitblock[i / 8] & mask) != 0) {  //Check 5
      fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
      exit(1);
    } else if ((bb_addr[i / 8] & mask) != 0 && (my_bitblock[i / 8] & mask) == 0) {  //Check 6
      fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
      exit(1);
    }
  }
}


/**
 * Check 8: For in-use inodes, the file size stored must be within the actual number of blocks used for storage.
 * @param inode_ptr
 * @param num_inodes
 * @param num_links
 */
//TODO: Should we include one block for INDIRECT node?
void VerifyInodeFileSize(void *img_ptr, struct dinode *inode, int inode_num) {

  int dfsize = inode -> size;
  int count = 0;
  int i;
  for (i = 0; i < NDIRECT; i++) {
    if (inode->addrs[i] != 0) {
      count++;
    }
  }

  if (inode->addrs[NDIRECT] != 0) {
    uint *indirect = (uint *)GetAddressOfBlock(img_ptr, inode->addrs[NDIRECT]);
    int i_datanum = BSIZE / sizeof(uint);
    for (i = 0; i < i_datanum; i++) {
      if (indirect[i] != 0) {
        count++;
      }
    }
  }

  if (dfsize > (count - 1) * BSIZE && dfsize <= count * BSIZE ) return;
  fprintf(stderr, "ERROR: incorrect file size in inode.\n");
  exit(1);
}

/**
 * Check 9: For all inodes marked in use, each must be referred to in at least one directory.
 * @param inode_ptr
 * @param num_inodes
 * @param used_in_directory
 */
// TODO: Unused same as unallocated: https://piazza.com/class/k4bkdfbqqziw5?cid=1409
// For checking inode marked in use, verify that the type is not 0
int VerifyInodeDirectory(struct dinode *inode_ptr, unsigned long num_inodes, int* used_in_directory, int repair) {
  for (int i = 1; i < num_inodes; i++) {
    if (inode_ptr[i].type != 0 && used_in_directory[i] == 0) {
      if (repair == 0) {
        fprintf(stderr, "ERROR: inode marked used but not found in a directory.\n");
        exit(1);
      } else {
        return 1;
      }
    }
  }
  return 0;
}

/**
 * Check 10: For each inode number that is referred to in a valid directory, it is actually marked in use.
 * @param inode_ptr
 * @param num_inodes
 * @param used_in_directory
 */
void VerifyReferredInodeMarkedUsed(struct dinode *inode_ptr, unsigned long num_inodes, int* used_in_directory) {
  for (int i = 1; i < num_inodes; i++) {
    if (used_in_directory[i] == 1 && inode_ptr[i].type == 0) {
      fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
      exit(1);
    }
  }
}

/**
 * Check 11: Reference counts (number of links) for regular files match the number of times
 *           file is referred to in directories (i.e., hard links work correctly).
 * @param inode_ptr
 * @param num_inodes
 * @param num_links
 */
int VerifyFileLinks(struct dinode *inode_ptr, unsigned long num_inodes, short* num_links, int repair) {
  for (int i = 1; i < num_inodes; i++) {
    if (inode_ptr[i].type == T_FILE && num_links[i] != inode_ptr[i].nlink) {
      if (repair == 0) {
        //printf("Num links should be %d for inode %d, but found %d\n", num_links[i], i, inode_ptr[i].nlink);
        fprintf(stderr, "ERROR: bad reference count for file.\n");
        exit(1);
      } else {
        return 1;
      }
    }
  }
  return 0;
}

/**
 * Check 12: No extra links allowed for directories (each directory only appears in one other directory).
 * @param inode_ptr
 * @param num_inodes
 * @param num_links
 */
void VerifyDirectoryLinks(struct dinode *inode_ptr, unsigned long num_inodes, short* num_links) {
  for (int i = 1; i < num_inodes; i++) {
    if (inode_ptr[i].type == T_DIR && num_links[i] > 1) {
      fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
      exit(1);
    }
  }
}

/**
 * Extra credit #1: Each .. entry in a directory refers to the proper parent inode and parent inode points back to it.
 * @param img_ptr
 * @param inode_ptr
 * @param num_inodes
 */
void VerifyParentInode(void *img_ptr, struct dinode *inode_ptr, unsigned long num_inodes) {
  for (int i = 1; i < num_inodes; i++) {
    int found = 0;
    if (inode_ptr[i].type == T_DIR) {
      // '..' should exist in the first data dir itself - according to this Piazza post: https://piazza.com/class/k4bkdfbqqziw5?cid=1510
      int parent_inum = GetInodeNum(img_ptr, inode_ptr[i].addrs[0], "..");

      //printf("parent i_num for inode %d is %d\n", parent_inum, i);
      struct dinode parent_inode = inode_ptr[parent_inum];
      if (parent_inode.type != T_DIR) {
        fprintf(stderr, "ERROR: parent directory mismatch.\n");
        exit(1);
      }
      // Check all entries in parent data dir to check if current node exists.
      for (int j = 0; j < NDIRECT; j++) {
        if (parent_inode.addrs[j] != 0) {
          if (CheckIfCurrentNodeExists(img_ptr, parent_inode.addrs[j], i) == 1) {
            found = 1;
            break;
          }
        }
      }
      if (found == 1) continue;
      //printf("Starting indirect blocks check\n");
      if (parent_inode.addrs[NDIRECT] != 0) {
        uint *indirect = (uint *)GetAddressOfBlock(img_ptr, parent_inode.addrs[NDIRECT]);
        int i_datanum = BSIZE/ sizeof(uint);
        for (int j = 0; j < i_datanum; j++) {
          if (indirect[j] != 0) {
            if (CheckIfCurrentNodeExists(img_ptr, indirect[j], i) == 1) {
              found = 1;
              break;
            }
          }
        }
      }
      if (found == 1) continue;

      fprintf(stderr, "ERROR: parent directory mismatch.\n");
      exit(1);

    }
  }
};



/**
 * Extra credit #2: Every directory traces back to the root directory (i.e no loops in the directory tree)
 * @param img_ptr
 * @param inode_ptr
 * @param num_inodes
 */
void VerifyAccessibilityFromRoot(void *img_ptr, struct dinode *inode_ptr, unsigned long num_inodes) {
  int visited[num_inodes];
  for (int i = 1; i < num_inodes; i++) {
    if (inode_ptr[i].type == T_DIR) {
      ClearVisited(visited, num_inodes);
      // Start visiting
      visited[i] = 1;
      int parent_inum = GetInodeNum(img_ptr, inode_ptr[i].addrs[0], "..");

      while (parent_inum != 1) {
        if (parent_inum <= 0 || visited[parent_inum] == 1) {  //Loop detected.
          fprintf(stderr, "ERROR: inaccessible directory exists.\n");
          exit(1);
        }
        visited[parent_inum] = 1;
        parent_inum = GetInodeNum(img_ptr, inode_ptr[parent_inum].addrs[0], "..");
      }
    }

  }
}

/***
 * Extra credit #3:
 * Repair the "inode marked used but not found in a directory" error (Check 9).
 * An xv6 image that has a number of in-use files and directories that are not linked by any directory.
 * An example of such image is available: /u/c/s/cs537-1/tests/p5/images/Repair.
 * This method collects these nodes and put them in lost_found directory.
 * The lost_found directory is already present in the root directory of the provided image.
 * @param repair_img_ptr
 * @param inode_ptr
 * @param num_inodes
 * @param used_in_directory
 */
void RepairInodeDirectory(void* repair_img_ptr, struct dinode *inode_ptr, unsigned long num_inodes, int *used_in_directory) {

  int lost_inodes[num_inodes];
  int lost_found_inum = GetInodeNum(repair_img_ptr, inode_ptr[1].addrs[0], "lost_found");
  struct dinode lost_found_inode = inode_ptr[lost_found_inum];

  int lost_num = 0;
  for (int i = 1; i < num_inodes; i++) {
    if (inode_ptr[i].type != 0 && used_in_directory[i] == 0) {
      //printf("Repairing inode %d marked used but not found in directory\n", i);
      // Change '..' entry in data dir of inode i to lost_found_inum
      inode_ptr[i].nlink = 1;
      if (inode_ptr[i].type == T_DIR) {
        SetParentDirectory(repair_img_ptr, inode_ptr[i].addrs[0], lost_found_inum);
      }
      //Add this inode entry to directory data directory.
      lost_inodes[lost_num] = i;
      lost_num++;
    }
  }

  //printf("lost num is %d\n", lost_num);
  int current = 0;
  for (int i = 0; i < NDIRECT; i++) {
    // Check if space in this address.
    //TODO: What if data address is 0
    struct xv6_dirent *entry = (struct xv6_dirent *) GetAddressOfBlock(repair_img_ptr, lost_found_inode.addrs[i]);
    int dirent_num = BSIZE / sizeof(struct xv6_dirent);
    for (int j = 0; j < dirent_num; j++) {
      if (entry[j].inum == 0) {
        strcpy(entry[j].name, "dummy");
        //printf("inode added %d\n", lost_inodes[current]);
        entry[j].inum = lost_inodes[current];
        current++;
        if (current == lost_num) {
          return;
        }
      }
    }
  }
  if (current == lost_num) return;

  //printf("Starting indirect blocks check\n");
  if (lost_found_inode.addrs[NDIRECT] != 0) {
    uint *indirect = (uint *)GetAddressOfBlock(repair_img_ptr, lost_found_inode.addrs[NDIRECT]);
    int i_datanum = BSIZE/ sizeof(uint);
    for (int i = 0; i < i_datanum; i++) {

      struct xv6_dirent *entry = (struct xv6_dirent *) GetAddressOfBlock(repair_img_ptr, indirect[i]);
      int dirent_num = BSIZE / sizeof(struct xv6_dirent);
      for (int j = 0; j < dirent_num; j++) {
        if (entry[j].inum == 0) {
          strcpy(entry[j].name, "dummy");
          entry[j].inum = lost_inodes[current];
          current++;
          if (current == lost_num) {
            break;
          }
        }
      }

    }
  }
}

/**
 * Extra credit #4: Repair the incorrect reference counts (Point 11) for any inodes with the actual number of links.
 * @param inode_ptr
 * @param num_inodes
 * @param num_links
 */
void RepairInodeNumLink(struct dinode *inode_ptr, unsigned long num_inodes, short* num_links) {
  for (int i = 1; i < num_inodes; i++) {
    if (inode_ptr[i].type == T_FILE && num_links[i] != inode_ptr[i].nlink) {
      inode_ptr[i].nlink = num_links[i];
    }
  }
}
// FS layout:
// unused | superblock | inodes - 25 blocks | unused | bitmap | data
// 01234....2627<28>
// 1 + 1 + 25 + 1 + 1 + 995 = 1024 blocks
int main(int argc, char *argv[]) {

  // Data Structures Initialization
  int fd;
  int repair = 0;
  struct stat sbuf;

  void *img_ptr;
  if (argc == 2) {
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "image not found.\n");
      exit(1);
    }
    fstat(fd, &sbuf);
    img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (img_ptr == MAP_FAILED) {
      exit(1);
      //TODO: Check what to print and return here.
    }

  } else if (argc == 3) {
    if (strcmp(argv[1], "-r") == 0) {
      fd = open(argv[2], O_RDWR);
      repair = 1;
      if (fd < 0) {
        fprintf(stderr, "image not found.\n");
        exit(1);
      }
      fstat(fd, &sbuf);
      img_ptr = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (img_ptr == MAP_FAILED) {
        exit(1);
        //TODO: Check what to print and return here.
      }
    } else {
      fprintf(stderr, "Usage: xfsck -r <file_system_image>\n");
      exit(1);
    }
  } else {
    fprintf(stderr, "Usage: xfsck <file_system_image>\n");
    exit(1);
  }



  struct superblock *sb = (struct superblock *)(img_ptr + BSIZE);
  int total_blocks = sb->size;
  //TODO: Why divide by 8?
  //Took this formula from mkfs.c:77. In a block (512 * 8) bits. 1 bit per block.
  int num_bitblocks = total_blocks / (BSIZE * 8) + 1;
  int num_datablocks = sb->nblocks;
  unsigned long num_inodes = sb->ninodes;
  unsigned long num_inodeblocks = num_inodes / IPB;

  struct dinode *inode_ptr = (struct dinode *)(img_ptr + 2 * BSIZE);

  uint datablock_start = num_inodeblocks + 3 + num_bitblocks;
  uint bitblock_num = num_inodeblocks + 3;
  char my_bitblock[BSIZE] = {0};
  int used_by_inode[total_blocks];
  int used_in_directory[num_inodes];
  short num_links[num_inodes];

  for (int i = 0; i < total_blocks; i++) {
    used_by_inode[i] = 0;
  }

  ClearInUseNumLinks(used_in_directory, num_links, num_inodes);

  char *bb_addr = (char *) GetAddressOfBlock(img_ptr, bitblock_num);

  if (repair == 1) {

    /**
    If you were creating a new repaired image:
    int repair_fd = open("new_repaired.img", O_RDWR | O_CREAT, 0666);
    ftruncate(repair_fd, sbuf.st_size);
    void *repair_img_ptr = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, repair_fd, 0);
    memcpy (repair_img_ptr, img_ptr, sbuf.st_size);

    struct dinode *repair_inode_ptr = (struct dinode *)(repair_img_ptr + 2 * BSIZE);

     **/

    MarkAllUsedInDirectory(img_ptr, inode_ptr, num_inodes, used_in_directory, num_links);   // Extra credits #3
    if (VerifyInodeDirectory(inode_ptr, num_inodes, used_in_directory, 1) == 1) {
      RepairInodeDirectory(img_ptr, inode_ptr, num_inodes, used_in_directory);
    }

    ClearInUseNumLinks(used_in_directory, num_links, num_inodes);                           // Extra credits #4
    MarkAllUsedInDirectory(img_ptr, inode_ptr, num_inodes, used_in_directory, num_links);
    if (VerifyFileLinks(img_ptr, num_inodes, num_links, 1) == 1) {
      RepairInodeNumLink(inode_ptr, num_inodes, num_links);
    }

    munmap(img_ptr, sbuf.st_size);
    close(fd);
    exit(1);
  }

  // Verification process starts
  VerifySuperBlockMetadata(total_blocks, num_inodeblocks, num_bitblocks, num_datablocks); // Check 1
  for (int i = 0; i < num_inodes; i++) {
    VerifyInodeType(&inode_ptr[i]);                                                       // Check 2
    if (inode_ptr[i].type != 0) {
      VerifyInodeDataPointers(img_ptr, &inode_ptr[i], datablock_start, total_blocks,
                              my_bitblock, i, used_by_inode);                             // Check 3
      if (inode_ptr[i].type == T_DIR) {
        VerifyDotEntries(img_ptr, &inode_ptr[i], i);                                      // Check 4
      }
      if (inode_ptr[i].type == T_FILE) {
        VerifyInodeFileSize(img_ptr, &inode_ptr[i], i);                                   // Check 8
      }
    }

  }

  VerifyBitmap(bb_addr, my_bitblock, datablock_start);                                    // Check 5, 6
  MarkAllUsedInDirectory(img_ptr, inode_ptr, num_inodes, used_in_directory, num_links);
  VerifyInodeDirectory(inode_ptr, num_inodes, used_in_directory, 0);               // Check 9
  VerifyReferredInodeMarkedUsed(inode_ptr, num_inodes, used_in_directory);                // Check 10
  VerifyFileLinks(inode_ptr, num_inodes, num_links, 0);                            // Check 11
  VerifyDirectoryLinks(inode_ptr, num_inodes, num_links);                                 // Check 12
  VerifyParentInode(img_ptr, inode_ptr, num_inodes);                                      // Extra credits #1
  VerifyAccessibilityFromRoot(img_ptr, inode_ptr, num_inodes);                            // Extra credits #2

  return 0;
}
