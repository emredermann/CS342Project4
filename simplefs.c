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
#define SHIFT 3             //1 byte = 8 bits Which means the FCB size. 
#define MASK 0x7

/*
** SHIFT used the index to locate the 128 bits FCB's
** MASK used the index to locate the offset in the 128 bits block.
*/


#define SECTOR_SIZE 128
//contains the pointers to all blocks occupied by the file.
struct inode
{
    int inodeNo;
    int indexNodeNo;
    int usedStatus;
    char filler [128 - (2 * sizeof(int)) - sizeof(struct index_node *)];
};

struct directoryEntry
{
    char fileName[FILENAMESIZE];
    //File serial number for open table 
    int iNodeNo;
    int size;
    int usedStatus;
    char filler [128 - sizeof(int) - sizeof(char) * FILENAMESIZE];
};

struct directoryblock
{
    struct directoryEntry entryList[ENTRY_PER_BLOCK];
};
//The size of the directory Entry declared as 128 in the assingment.
// Contains the address of the index block(inode)


// The initialized block number of the index_block must be set to the appropriate inode.
struct index_block{
    //  4kb / 4bytes
    // block no of the each file data blocks
    int block_numbers [1024];
};


struct bitmap_block{
    //Check the size calculation (Lecture 38 --> 09:00)
   unsigned char bitmap[4096];
    
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
    int blockSize;
    int current_available_block;
};


void fcb_block_init(){
    struct FCB_block  * fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
    for (int i = 0; i < 32; i++)
    {
        fcb_block_ptr->inodes[i].usedStatus = 0;
    }
    for (int i = 0; i < 4; i++)
    {
        write_block(fcb_block_ptr,i+9);
    }
}
// if -1 means no empty location founded
int get_empty_inodeNo_in_fcb(){
    struct FCB_block  * fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
    for (int i = 0; i < 4; i++)
    {
        read_block(fcb_block_ptr,i+9);
        for(int j = 0;j < 32;j++){
            if(fcb_block_ptr->inodes[j].usedStatus == 0){
                //inode number of fcb.
                return (i * 32) + j;
            }
        }
    }
    return -1;
    
}

// if -1 means fcb could not founded
int get_indexTable_locaiton_of_specific_file(int inodeNo){
     struct FCB_block  * fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
     int block_location = (inodeNo / 32)+9;
     int offset = inodeNo % 32;

    read_block(fcb_block_ptr,block_location);
   if(fcb_block_ptr->inodes[offset].usedStatus == 1){
        return fcb_block_ptr->inodes[offset].indexNodeNo;
    }return -1;

}

// if -1 means fcb already allocated.
void set_fcb_block(int inodeNo){
    struct FCB_block  * fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
     int block_location = (inodeNo / 32)+9;
     int offset = inodeNo % 32;

    read_block(fcb_block_ptr,block_location);
   if(fcb_block_ptr->inodes[offset].usedStatus == 1){
        return -1;
    }
    fcb_block_ptr->inodes[offset].usedStatus = 1;
    write_block(fcb_block_ptr,block_location);

    return 0;

}

// (Number of bits per word) * (number of 0 values words)+  offset of first 1 in the non zero word.
void bitmap_block_init(){
    struct bitmap_block  * bitmap =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
    
    for (int i = 0; i < 4096; i++)
    {
        bitmap->bitmap[i] = '0';
    }
    
    for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)

    {
        // Since the bitmap are from 1 to 5 (not included.)
        write_block(bitmap,i+1);
    }
    
/*
       struct bitmap_block  * bitmap =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
       size_t bytes = (BLOCKSIZE >> SHIFT) + ((BLOCKSIZE & MASK) ? 1 : 1);
       void *map = malloc(bytes);

       for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)
    {
        // Since the root directories are from 5 to 9 (not included.)
        write_block(bitmap,i+1);
    }
    */
}

void print_bitmap(){
    struct bitmap_block  * bitmap =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
    for (int  i = 0; i < 4; i++)
    {
         read_block(bitmap,i+1);
         for (int j = 0; j < 4096; j++)
         {
             printf("{s} ",bitmap->bitmap[j]);
         }
         printf("\n");                 
    }
    
}

// index is the inodeNo
int bitmap_block_set(int index){

    struct bitmap_block  * bitmap_block =(struct bitmap_block *)malloc(sizeof(struct bitmap_block)); 
     //Decide which block is the bitmap going to read.
        read_block(bitmap_block,(index / (4096 * 8))+1);
        int word = index >> SHIFT;
        int position =index & MASK;
        bitmap_block->bitmap[word] |= 1 << position;  
    /*
        int word = index >> SHIFT;
        int position = index & MASK;
        bitmap_block->bitmap[word] |=  1 << position; 
    */
        write_block(bitmap_block,(index / (4096 * 8))+1);
}

// index is the inodeNo
int bitmap_block_clear(int index){

    struct bitmap_block  * bitmap_block =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
      //Decide which blocok is the bitmap going to read.
        read_block(bitmap_block,(index /(4096 * 8))+1);
        int word = index >> SHIFT;
        int position = index & MASK;
        bitmap_block->bitmap[word] &=  ~(1 << position); 
        write_block(bitmap_block,(4096 * 8));
}
// index is the inodeNo

int bitmap_read(int index){
    struct bitmap_block  * bitmap_block =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
     //Decide which block is the bitmap going to read.
        read_block(bitmap_block,(index / (4096 * 8))+1);
        int word = index >> SHIFT;
        int position = index & MASK;
       return (bitmap_block->bitmap[word]>> position) &1; 
}


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

    // Super block init***************************************
    struct superBlock * superBlock_ptr = (struct superBlock *) malloc(sizeof(struct superBlock));
    superBlock_ptr->blockCount = size / BLOCKSIZE;
    superBlock_ptr->blockSize = BLOCKSIZE;

    for (int i = 0; i < MAX_FILE_SIZE; ++i)
    {
        superBlock_ptr->freeFCB[i] = 0;
    }
    superBlock_ptr->current_available_block = 13;
    superBlock_ptr->totalNumberOfBlocks = 12;
    write_block(superBlock_ptr,0);
    //****************************************
    bitmap_block_init();
    
    for(int i = 0;i<5;i++){bitmap_block_set(i);}
    
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
    //Target location in directoryEntry
    int targetlocation = emptyDirectoryFounder();
    if(targetlocation == -1){return -1;}
    
    struct directoryblock * directoryBlock_ptr;

    directoryBlock_ptr = (struct directoryblock *) malloc (sizeof(struct directoryblock));
    
    for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)
    {
        read_block(directoryBlock_ptr,i+5);
        for (int j = 0; i < 32; j++)
        {
            // Checks the status of the fcb according to the data stored in the superblock.
        if((strcmp(filename, directoryBlock_ptr->entryList[j].fileName) == 0))
            {
                printf("File already exist.");
                return -1;
            }
    }
    }
    set_fcb_block(targetlocation);
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




//(lecture 37 - 40:00)
int sfs_read(int fd, void *buf, int n){

    if(fd > MAX_FILE_SIZE || modes[fd] == 0)
        return -1;
    if(open_FileTable[fd].size < (last_position[fd]+n) )
        return -1;

    //GET Block From Position
    int starting_pos = last_position[fd];
    int remained_blocks = (starting_pos +n -1) / BLOCKSIZE-starting_pos/BLOCKSIZE+1;    

    //last_position[fd] (buf_position)
    int b_position =0;
//**********************************************************************************
    //open_FileTable[fd] fbc
    int current_block = open_FileTable[fd].iNodeNo;

    struct index_block * tmp;
        tmp = (struct index_block *)malloc (sizeof(struct index_block *));


    for(int i = 0; i < b_position / BLOCKSIZE; i++){
        get_index_block_entry(current_block,tmp);
        current_block = tmp->block_numbers;
    }
//***********************************************************************************?

    int current_offset;
    char* data = malloc(BLOCKSIZE);

    read_block(data,current_block);
    for (int i = 0; i < remained_blocks; i++)
    {
        // For the last block
        if(i == remained_blocks - 1){
            int l_ofset = (starting_pos + n) % BLOCKSIZE;
            if(l_ofset == 0){
                l_ofset = BLOCKSIZE;
            }
            for(current_offset = last_position[fd] % BLOCKSIZE; current_offset < l_ofset; current_offset++){
                ((char*)buf)[b_position] = data[current_offset];
                b_position++;
            }
           // fd
           int tmp_size = l_ofset-(last_position[fd]%BLOCKSIZE);
            if(open_FileTable[fd].size >= last_position[fd]+tmp_size){
                last_position[fd] += tmp_size;
            }
        }else{
            for(current_offset = last_position[fd] % BLOCKSIZE; current_offset < BLOCKSIZE; current_offset++){
                ((char*)buf)[b_position] = data[current_offset];
                b_position++;
            }
            int tmp_size = BLOCKSIZE-(last_position[fd]%BLOCKSIZE);
            if(open_FileTable[fd].size >= last_position[fd]+tmp_size){
                last_position[fd] += tmp_size;
            }
//*******************************************************************************!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*************
           // Read the next indexed block number 
           //current_block = ;
            read_block(data,current_block);
//****************************************************************************************!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!***********
    }
    //number of bytes readed succesfully.
    return n;
}
}

int get_index_block_entry(int n, struct index_block *input) {

    int index_block_num = n / (BLOCKSIZE/sizeof(struct index_block *));
    int ofset = n - index_block_num *(BLOCKSIZE /sizeof(struct index_block *));

    struct index_block * tmp;
    tmp = (struct index_block *)malloc (sizeof(struct index_block));

    read_block(tmp,index_block_num+MAX_FILE_SIZE * sizeof(struct directoryEntry) / BLOCKSIZE);
    for (int i = 0; i < 128; i++)
    {
         
        input->block_numbers[i] = tmp->block_numbers[i];
    }
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


/*
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
    
    return (current_bitmap_block->bit_block[index/4] & (1 << (index % 4))) != 0;


}
*/

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

int set_bitmap_block(int location){
    struct bitmap_block * super_ptr;
    super_ptr = (struct bitmap_block *)malloc(sizeof(struct bitmap_block));
    read_block(super_ptr,0);
    super_ptr->bitmap[location] = 1;
    write_block(super_ptr,0);
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
        current_entry_block->entryList[i].usedStatus = 0;
        current_entry_block->entryList[i].size = 0;
    }
    for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)
    {
        // Since the root directories are from 5 to 9 (not included.)
        write_block(current_entry_block,i+5);
    }
    free(current_entry_block);   
    
}


// created directory entry added to the specific position.
int directory_entry_add(int directory_enrty_no, struct directoryEntry * ent){
    
    int directoryBlockNumber = directory_enrty_no / 32; 
    int ofset = (directory_enrty_no % 32);
    
    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));

    if(read_block(dir_block,directoryBlockNumber + 1) != 1){
        return -1;
    }

    strcpy(dir_block->entryList[ofset].fileName, ent->fileName);
    dir_block->entryList[ofset].iNodeNo = ent->iNodeNo;
    dir_block->entryList[ofset].usedStatus = 1;
    dir_block->entryList[ofset].size = ent->size;
    
    // real part 
    if(write_block(dir_block,directoryBlockNumber+1) != 1){
        return -1;
    }
    return 1;
}


//find_dir_entry
int directory_entry_location_finder_byName(char * name, struct directoryEntry * ent ){

    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));
    for (int i = 0; i < 4; i++)
    {
        read_block(dir_block,i+5);
        for(int j = 0;j<32;j++){
            if((strcmp(name,dir_block->entryList[j].fileName) == 0)){
                    ent->iNodeNo = dir_block->entryList[j].iNodeNo;
                    strcpy(ent->fileName,dir_block->entryList[j].fileName); 
                    ent->size = dir_block->entryList[j].size;
            // Block size / sizeof(struct directory entry) = 32
                    return (i*32)+j;
            }
        }
    }
    return -1;
}


//Checks the free directory entry position according to the superblock
//-1 means no empty location
int empty_Directory_location_Founder(){
    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));
    for (int i = 0; i < 4; i++)
    {
        read_block(dir_block,i+5);
        for(int j = 0;j<32;j++){
            if(dir_block->entryList[j].usedStatus == 0){
                return (i*32)+j;
            }
        }
    
    }
    return -1;
}
