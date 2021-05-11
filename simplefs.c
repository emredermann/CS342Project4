#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"

#define BLOCKSIZE 4096 // bytes
#define FILENAMESIZE 110
#define ENTRY_PER_BLOCK 32
#define MAXFILES 32
#define ENTRYBLOCKS 4
#define MAX_FILE_SIZE 128
#define FCB_SIZE 128
#define ROOT_BLOCK_NUMBER 4
#define BITMAP_BLOCK_NUMBER 4

//contains the pointers to all blocks occupied by the file.
struct inode
{
    int indexNodeNo;
    int usedStatus;
};

struct directoryEntry
{
    char fileName[FILENAMESIZE];
    //File serial number for open table 
    int iNodeNo;
    int size;
    char filler [128 - sizeof(int) -FILENAMESIZE];
};

struct directoryblock
{
    struct directoryEntry entryList[ENTRY_PER_BLOCK];
};
//The size of the directory Entry declared as 128 in the assingment.
// Contains the address of the index block(inode)

struct index_block{
    //  1kb / 4bytes
    int  addresses [1024];
};

struct bitmap_block{
    //Check the size calculation (Lecture 38 --> 09:00)
    int bit_block[4];
 
};

struct FCB_block{
    struct inode inodes[32];
};

struct superBlock
{
    int blockCount;
    // int blocksPerGroup;   
    int freeFCB[MAX_FILE_SIZE];
    int totalNumberOfBlocks;

};


//MODE_READ 0
//MODE_APPEND 1
int modes[MAX_FILE_SIZE];
int available_location_openFileTable[MAX_FILE_SIZE];
struct directoryEntry open_FileTable[MAX_FILE_SIZE];
int last_position[MAX_FILE_SIZE];
int entry_position[MAX_FILE_SIZE];

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
    superBlock_ptr->blockCount = size / BLOCKSIZE;
    for (int i = 0; i < MAX_FILE_SIZE; ++i)
    {
        superBlock_ptr->freeFCB[i] = 0;
    }
    write_block(superBlock_ptr,0);
    free(superBlock_ptr);

    bitmap_block_init();


    directory_entry_block_init();


    
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



// Done 
// RECHECK AGAIN
int sfs_create(char *filename)
{
    //Target location in FCB
    int targetlocation = emptyDirectoryFounder(filename);
    if(targetlocation == -1){return -1;}
    
    /*
    struct bitmap_block * spr_ptr;
    spr_ptr = (struct bitmap_block *) malloc (sizeof(struct bitmap_block));
    read_block(spr_ptr,1);
    */
    
    struct superBlock * spr_ptr;
    spr_ptr = (struct superBlock *) malloc (sizeof(struct superBlock));
    read_block(spr_ptr,0);
   

    struct directoryblock * directoryBlock_ptr;

    directoryBlock_ptr = (struct directoryblock *) malloc (sizeof(struct directoryblock));
    
    for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)
    {
        read_block(directoryBlock_ptr,i+5);
        for (int j = 0; i < 32; j++)
        {
            // Bitmap location calculation ?? i * 
            //if(spr_ptr->freeFCB[i*32+j])}
            
        if((spr_ptr->freeFCB[i*32+j] == 1) && (strcmp(filename, directoryBlock_ptr->entryList[j].fileName) == 0))
            {
                printf("File already exist.");
                return -1;
            }
    }
    }
    //set_bitmap_block(targetlocation);
    set_superblock_FCB(targetlocation);
    struct directoryEntry * directoryEntry_ptr;
    directoryEntry_ptr = (struct directoryEntry *) malloc (sizeof(struct directoryEntry));
    directoryEntry_ptr->size = 0;
    strcpy(directoryEntry_ptr->fileName,filename);

   
    
    directory_entry_add(targetlocation,directoryBlock_ptr);

    return (0);
}


// DONE
int sfs_open(char *file, int mode)
{
    struct directoryEntry* tmp;
    tmp = (struct directoryEntry *)malloc(sizeof(struct directoryEntry));
    int entryLocation =  directory_entry_finder(file,tmp) ;
    if(entryLocation == -1){ printf("Directory entry finder could not find the entry ! \n");return -1;}
    int locationHolder = -1 ;
    for (int i = 0; i < MAX_FILE_SIZE; i++)
    {
        if(available_location_openFileTable[i] == 0){
            locationHolder = i;
            break;
        }
    }
    if(locationHolder == -1){return -1;} 
    strcpy(open_FileTable[locationHolder].fileName,tmp->fileName);
    open_FileTable[locationHolder].iNodeNo = tmp->iNodeNo;
    open_FileTable[locationHolder].size = tmp->size;
    modes[locationHolder] = mode;
    available_location_openFileTable[locationHolder] = 1;
    entry_position[locationHolder] = entryLocation;
    return locationHolder; 
}


// DONE
int sfs_close(int fd){
    if(fd >= 0){
        available_location_openFileTable[fd] = 0;
        return (0); 
    }else
        return -1;
}

// DONE
int sfs_getsize (int  fd)
{
    open_FileTable[fd].size;
    return (0); 
}



//FCB
//index table
//offset logical address 5000
//5000 / (1024 * 4096)(4MB) ==> outer table entry
// 5000 / 4096 = 1 ==> inner block entry       (LOGICAL BLOCK,              OFFSET)
// 5000 % 4096 = 904 ==> block displacement (inner blcok akrşılığı 1 in ,offset(904))
//(lecture 37 - 40:00)
int sfs_read(int fd, void *buf, int n){

    if(fd > MAX_FILE_SIZE || modes[fd] == 0)
        return -1;
    if(open_FileTable[fd].size < (last_position[fd]+n) )
        return -1;

    //GET Block From Position
    



}

// Allocate the index data num when needed.
int sfs_append(int fd, void *buf, int n)
{
    return (0); 
}

int sfs_delete(char *filename)
{
    return (0); 
}

// Add print disk method to check the status.



void bitmap_block_init(){

    struct bitmap_block * current_bitmap_block;
    current_bitmap_block = (struct bitmap_block *) malloc ( sizeof ( struct bitmap_block ) );
    for (int i = 0; i < BITMAP_BLOCK_NUMBER; i++)
    {
        current_bitmap_block->bit_block[i] = 1;   
    }
    //current_bitmap_block->bit_block = (uint *)calloc((BITMAP_BLOCK_NUMBER/32) + ((BITMAP_BLOCK_NUMBER % 4) != 0) , 4);

    write_block(current_bitmap_block,1);
    current_bitmap_block->bit_block[0] = 1;
    write_block(current_bitmap_block,2);

    for (int i = 0; i < BITMAP_BLOCK_NUMBER; i++)
    {
        current_bitmap_block->bit_block[i] = 0;   
    }
    for (int i = 2; i < BITMAP_BLOCK_NUMBER; i++)
    {
        // Since the bitmap blocks are from 1 to 5 (not included.)
        write_block(current_bitmap_block,i+1);
    }
    free(current_bitmap_block);   
}



int bitmap_is_allocated(int index){
    struct bitmap_block * current_bitmap_block;
    current_bitmap_block = (struct bitmap_block *) malloc ( sizeof ( struct bitmap_block ) );
    
    read_block(current_bitmap_block,(index /4));
    
    return (current_bitmap_block->bitmap[index/32] & (1 << (index % 4))) != 0;


}


/*
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
*/

//************************************************
//Directory Entry methods

void directory_entry_block_init(){
    struct directoryblock * current_entry_block;
    current_entry_block = (struct directoryblock *) malloc ( sizeof ( struct directoryblock ) );
    //the blocks 5,6,7,8 contains the root directory
    for (int i = 0; i < BLOCKSIZE / sizeof(struct directoryEntry); i++)
    {
        current_entry_block->entryList[i].fileName[0] = '\0';
        current_entry_block->entryList[i].iNodeNo = 0;
        current_entry_block->entryList[i].size = 0;
    }
    for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)
    {
        // Since the root directories are from 5 to 9 (not included.)
        write_block(current_entry_block,i+5);
    }
    free(current_entry_block);   
    
}

int directory_entry_add(int n, struct directoryEntry * ent){
    int directoryBlockNumber = n / 32; 
    int ofset = n-directoryBlockNumber * 32;
    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));

    if(read_block(dir_block,directoryBlockNumber+1) != 1){
        return -1;
    }

    strcpy(dir_block->entryList[ofset].fileName, ent->fileName);
    dir_block->entryList[ofset].iNodeNo = ent->iNodeNo;
    dir_block->entryList[ofset].size = ent->size;
    // real part 
    if(write_block(dir_block,directoryBlockNumber+1) != 1){
        return -1;
    }
    
    return 1;
}

int directory_entry_finder(char * name, struct directoryEntry * ent ){

    struct superBlock * ptr;
    ptr = (struct superBlock *) malloc(sizeof(struct superBlock));

    read_block(ptr,0);
    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));
    for (int i = 0; i < 4; i++)
    {
        read_block(dir_block,i+5);
        for(int j = 0;j<32;j++){
            if(ptr->freeFCB[(i*32)+j] == 1 && (strcmp(name,dir_block->entryList[j].fileName) == 0)){
                    ent->iNodeNo = dir_block->entryList[j].iNodeNo;
                    strcpy(ent->fileName,dir_block->entryList[j].fileName); 
            // Block size / sizeof(struct directory entry) = 32
                    return (i*32)+j;
            }
        }
    }
    return -1;
}


//Checks the directory if is it free or not According to the Bitmap.

int emptyDirectoryFounder(){

    struct bitmap_block * bitmap_ptr;
    bitmap_ptr = (struct bitmap_block *)malloc(sizeof(struct bitmap_block));
    read_block(bitmap_ptr,0);
    for(int i = 0;i <MAX_FILE_SIZE;i++){
        // 1 Means FCB is free
        if(bitmap_ptr->bitmap[i] == 1){
            return i;
        }
    }
}

// Sets the superblocks FCB item.
int set_superblock_FCB(int location){
    struct superBlock * super_ptr;
    super_ptr = (struct superBlock *)malloc(sizeof(struct superBlock));
    read_block(super_ptr,0);
    super_ptr->freeFCB[location] = 1;
    write_block(super_ptr,0);
}


int set_bitmap_block(int location){
    struct bitmap_block * super_ptr;
    super_ptr = (struct bitmap_block *)malloc(sizeof(struct bitmap_block));
    read_block(super_ptr,0);
    super_ptr->bitmap[location] = 1;
    write_block(super_ptr,0);
}


int directory_entry_getter(int n, struct  directoryEntry * output){
     int directoryBlockNumber = n / 32; 
    int ofset = n-directoryBlockNumber * 32;
    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));

    if(read_block(dir_block,directoryBlockNumber+1) != 1){
        return -1;
    }

    strcpy(dir_block->entryList[ofset].fileName, output->fileName);
    dir_block->entryList[ofset].iNodeNo = output->iNodeNo;
    return 1;


}