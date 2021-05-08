#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"

#define BITMAP_BLOCK_NO 4
#define BLOCKSIZE 4096 // bytes
#define FILENAMESIZE 110
#define ENTRY_PER_BLOCK 32
#define MAXFILES 32
#define ENTRYBLOCKS 4
#define MAX_FILE_SIZE 128
#define ROOT_BLOCKS 4
struct inode
{
    int nodeID;
   // int dataBlockNumber;
    int dataBlockNumbers[];

};
struct directoryblock
{
    struct directoryEntry entryList[ENTRY_PER_BLOCK];
};
//The size of the directory Entry declared as 128 in the assingment.
struct directoryEntry
{
    char fileName[FILENAMESIZE];
    int iNodeNo;
    char filler [128 - sizeof(int) -FILENAMESIZE];

};
struct bitmap_block{
    int bitmap[4];

};
struct FCB_block{
    struct inode inodes[32];
};

struct superBlock
{
    int iNodeCount;
    int blocksCount;
    int reservedBlocksCount;
    int freeFileBlockCount[MAX_FILE_SIZE];
    int freeiNodeCount;
    int firstDataBlock;
    int blockSize;
   // int blocksPerGroup;   
    struct directoryEntry dir_Entry [MAX_FILE_SIZE];
    int freeFCB[MAX_FILE_SIZE];
};

struct superBlock super_block;
struct block block;
struct directoryEntry dir_Entry;


void inode_init(){}

// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly. 
// ========================================================


int get_vdisk_fd(){
    return vdisk_fd;
}

// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk. 
int read_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
        printf ("read error\n");
        return -1;
    }
    return (0); 
}

// write block k into the virtual disk. 
int write_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	    printf ("write error\n");
	    return (-1);
    }
    return 0; 
}


/**********************************************************************
   The following functions are to be called by applications directly. 
***********************************************************************/


// this function is partially implemented.
int create_format_vdisk (char *vdiskname, unsigned int m)
{
    char command[1000];
    int size;
    int num = 1;
    int count;
    size  = num << m;
    count = size / BLOCKSIZE;
    //    printf ("%d %d", m, size);
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, count);
    //printf ("executing command = %s\n", command);
    system (command);
    
    // now write the code to format the disk below.
    // .. your code...
    


    sfs_mount(vdiskname);
    struct superBlock * superBlock_ptr = (struct superBlock *) malloc(sizeof(struct superBlock));
    superBlock_ptr-> blocksCount = size / BLOCKSIZE;
    for (int i = 0; i < MAX_FILE_SIZE; ++i)
    {
        superBlock_ptr->freeFCB[i] = 0;
    }
    write_block(superBlock_ptr,0);
    free(superBlock_ptr);

    bitmap_block_init();
    directory_entry_block_init();
    fcb_block_init();
    sfs_umount();
    return (0); 
}


// already implemented
int sfs_mount (char *vdiskname)
{
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it. 
    vdisk_fd = open(vdiskname, O_RDWR); 
    return(0);
}


// already implemented
int sfs_umount ()
{
    fsync (vdisk_fd); // copy everything in memory to disk
    close (vdisk_fd);
    return (0); 
}


int sfs_create(char *filename)
{
    return (0);
}


int sfs_open(char *file, int mode)
{
    return (0); 
}

int sfs_close(int fd){
    return (0); 
}

int sfs_getsize (int  fd)
{
    return (0); 
}

int sfs_read(int fd, void *buf, int n){
    return (0); 
}


int sfs_append(int fd, void *buf, int n)
{
    return (0); 
}

int sfs_delete(char *filename)
{
    return (0); 
}

void bitmap_block_init(){
    struct bitmap_block * current_bitmap_block;
    current_bitmap_block = (struct bitmap_block *) malloc ( sizeof ( struct bitmap_block ) );
    
    for (int i = 0; i < BLOCKSIZE / sizeof(struct bitmap_block); i++)
    {
        for (int j = 0; j < 4; j++)
        {
            // Which 0 means free space.
            current_bitmap_block->bitmap[j] = 0;
        }
        
    }
    for (int i = 0; i < BITMAP_BLOCK_NO-1; i++)
    {
        // Since the root directories are from 5 to 9 (not included.)
        write_block(current_bitmap_block,i+1);
    }
    free(current_bitmap_block);   

}


void fcb_block_init(){
    struct FCB_block * current_fcb_block;
    current_fcb_block=  (struct FCB_block *) malloc ( sizeof ( struct FCB_block ) );
    for (int i = 0; i < BLOCKSIZE / sizeof(struct directoryEntry); i++)
    {
        // FCB initializaiton hakkında soru işaretleri maili bekle.
    }
    for (int i = 0; i < ROOT_BLOCKS; i++)
    {
        // Since the root directories are from 5 to 9 (not included.)
        write_block(current_fcb_block,i+5);
    }
    free(current_fcb_block);   
}


void directory_entry_block_init(){
    struct directoryblock * current_entry_block;
    current_entry_block = (struct directoryblock *) malloc ( sizeof ( struct directoryblock ) );
    //the blocks 5,6,7,8 contains the root directory
    for (int i = 0; i < BLOCKSIZE / sizeof(struct directoryEntry); i++)
    {
        current_entry_block->entryList[i].fileName[0] = '\0';
        current_entry_block->entryList[i].iNodeNo = 0;
    }
    for (int i = 0; i < ROOT_BLOCKS; i++)
    {
        // Since the root directories are from 5 to 9 (not included.)
        write_block(current_entry_block,i+5);
    }
    free(current_entry_block);   
    
}