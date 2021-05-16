#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"

#define SECTOR_SIZE 128
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

//contains the pointers to all blocks occupied by the file.
struct inode
{
    int inodeNo;
    int indexNodeNo;
    int location_pointer;
    int mode;
    int usedStatus;
    char filler [128 - (5 * sizeof(int))];
};

struct directoryEntry
{
    char fileName[FILENAMESIZE];
    //File serial number for open table 
    int iNodeNo;
    int size;
    
    char filler [128 - 2*sizeof(int) - sizeof(char) * FILENAMESIZE];
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
   unsigned int block_numbers [1024];
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
    int totalNumberOfBlocks;
};

void fcb_block_init(){
    struct FCB_block  * fcb_block_ptr;
    fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
  
     for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            fcb_block_ptr->inodes[j].usedStatus = 0;
            fcb_block_ptr->inodes[j].indexNodeNo = -1;
            fcb_block_ptr->inodes[j].inodeNo = (i*32)+j;
            fcb_block_ptr->inodes[j].location_pointer = 0;
            fcb_block_ptr->inodes[j].mode = 0;
        
        }
        write_block(fcb_block_ptr,i+9);
    }
}

int get_empty_inodeNo_in_fcb(){
    struct FCB_block  * fcb_block_ptr;
     fcb_block_ptr = (struct FCB_block *)malloc(sizeof(struct FCB_block));
     for (int i = 0; i < 4; i++)
    {
        read_block(fcb_block_ptr,i+9);
        for (int j = 0; j < 32; j++)
        {
            if((fcb_block_ptr->inodes[j].usedStatus) == 0){

                fcb_block_ptr->inodes[j].usedStatus = 1;
                 write_block(fcb_block_ptr,i+9);
                return (j * 32)+i;
            }
        }
        write_block(fcb_block_ptr,i+9);
    }
    return -1;
}


// if -1 means fcb already allocated.
void set_fcb_block(int inodeNo,int indexNode, int locationPointer, int mode, int usedStatus){
     struct FCB_block  * fcb_block_ptr;
     fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
     int block_location = (inodeNo / 32)+9;
     int offset = inodeNo % 32;

    read_block(fcb_block_ptr,block_location);
   if(fcb_block_ptr->inodes[offset].usedStatus == 1){
        return -1;
    }
    fcb_block_ptr->inodes[offset].usedStatus = 1;
    fcb_block_ptr->inodes[offset].indexNodeNo = indexNode;
    fcb_block_ptr->inodes[offset].inodeNo = inodeNo;
    fcb_block_ptr->inodes[offset].location_pointer = 0;

    write_block(fcb_block_ptr,block_location);
    return 0;
}


void add_fcb(int inodeNo,struct inode * output_ptr){
 struct FCB_block  * fcb_block_ptr;
 fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
    
     int block_location = (inodeNo / 32)+9;
     int offset = inodeNo % 32;

    read_block(fcb_block_ptr,block_location);
    fcb_block_ptr->inodes[offset].indexNodeNo = output_ptr->indexNodeNo;
    fcb_block_ptr->inodes[offset].inodeNo = output_ptr->inodeNo;
    fcb_block_ptr->inodes[offset].usedStatus = output_ptr->usedStatus;
    fcb_block_ptr->inodes[offset].mode = output_ptr->mode;
    fcb_block_ptr->inodes[offset].location_pointer = output_ptr->location_pointer;

    write_block(fcb_block_ptr,block_location);
}

// According to the inodeNo

void get_inode_node(int inodeNo,struct inode * output_ptr){
    struct FCB_block  * fcb_block_ptr;
    fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
     int block_location = (inodeNo / 32)+9;
     int offset = inodeNo % 32;
     read_block(fcb_block_ptr,block_location);
     output_ptr->inodeNo = fcb_block_ptr->inodes[offset].inodeNo;
     output_ptr->indexNodeNo = fcb_block_ptr->inodes[offset].indexNodeNo;
     output_ptr->usedStatus = fcb_block_ptr->inodes[offset].usedStatus;
     output_ptr->mode = fcb_block_ptr->inodes[offset].mode;
     output_ptr->location_pointer = fcb_block_ptr->inodes[offset].location_pointer;
}

int  get_inode_node_status(int inodeNo){
     struct FCB_block  * fcb_block_ptr;
    fcb_block_ptr =(struct FCB_block *)malloc(sizeof(struct FCB_block));
    int block_location = (inodeNo / 32)+9;
    int offset = inodeNo % 32;
    read_block(fcb_block_ptr,block_location);
    return fcb_block_ptr->inodes[offset].usedStatus;
}
// 1 means there is available location
// -1 means there are no available location
int check_enough_blocks_available_bitmap(int requiredBlock){
    int counter = 0;
    struct bitmap_block  * bitmap_block_ptr =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
    for (int i = 0; i < 4; i++)
    {
        read_block(bitmap_block_ptr,i+1);
        for(int j = 0;j < 4096;j++){
            if(bitmap_read(i) == 0){
                counter++;
            }
        }
    }
    if(counter >= requiredBlock){
        return 1;
    }else{
        return -1;
    }
}

// (Number of bits per word) * (number of 0 values words)+  offset of first 1 in the non zero word.
void bitmap_block_init(){
    struct bitmap_block  * bitmap;
    bitmap =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
    bitmap->bitmap[4096];
    for (int i = 0; i < 4096; i++)
    {
        bitmap->bitmap[i] = '0';
    }
    
    for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)
    {
        // Since the bitmap are from 1 to 5 (not included.)
        write_block(bitmap,i+1);
    }
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
   
        write_block(bitmap_block,(index / (4096 * 8))+1);
}

// index is the inodeNo
int bitmap_block_clear(int index){

    struct bitmap_block  * bitmap_block =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
      //Decide which block is the bitmap going to read.
        
        read_block(bitmap_block,(index /(4096 * 8))+1);
        int word = index >> SHIFT;
        int position = index & MASK;
        bitmap_block->bitmap[word] &=  ~(1 << position); 
        write_block(bitmap_block,(4096 * 8)+1);
}

// index is the inodeNo
int bitmap_read(int index){
    struct bitmap_block  * bitmap_block =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
     //Decide which block is the bitmap going to read.
        read_block(bitmap_block,(index / (4096 * 8))+1);
        int word = index >> SHIFT;
        int position = index & MASK;
       return (bitmap_block->bitmap[word]>> position) & 1; 
}


int get_free_block_in_Bitmap(){
     struct bitmap_block  * bitmap_block =(struct bitmap_block *)malloc(sizeof(struct bitmap_block));
      //Decide which blocok is the bitmap going to read.
      for (int i = 0; i < 4; i++)
      {
        read_block(bitmap_block,i+1);
        for(int index = 0;index < (4096 * 8); index++){
            int word = index >> SHIFT;
            int position = index & MASK;
            if( bitmap_read((i*4096*8) + index) == 0){
                bitmap_block_set((i*4096*8) + index);
                return (i*4096*8) + index;
            } 
    }

   }     
   return -1;
}


//MODE_READ 0
//MODE_APPEND 1
int available_location_openFileTable[MAX_FILE_SIZE];
struct directoryEntry open_FileTable[MAX_FILE_SIZE];
int last_position[MAX_FILE_SIZE];
int entry_position[MAX_FILE_SIZE];
int modes[MAX_FILE_SIZE];


// Global Variables =======================================
int vdisk_fd; 

int get_vdisk_fd(){
    return vdisk_fd;
}

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
        printf ("%d %d", m, size);
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, count);
     
    system (command);
   
    sfs_mount(vdiskname);

    // Super block init***************************************
    struct superBlock * superBlock_ptr;
    superBlock_ptr = (struct superBlock *) malloc(sizeof(struct superBlock));
    
    superBlock_ptr->totalNumberOfBlocks = count;
    
    write_block(superBlock_ptr,0);
   
    //****************************************
    fcb_block_init();

    directory_block_init();
    bitmap_block_init();
    for(int i = 0;i<13;i++){bitmap_block_set(i);}
    
    sfs_umount();
    return 0; 
}


// already implemented
int sfs_mount (char *vdiskname)
{
    
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
  
    int targetlocation = get_empty_inodeNo_in_fcb;
    printf("target Location is{%d}",targetlocation);
    if(targetlocation == -1){return -1;}
    printf("inside create");
    struct inode * inode_ptr;
    inode_ptr = (struct inode *) malloc (sizeof(struct inode));
    //**********************************************************************
    struct directoryblock * directoryBlock_ptr;
    directoryBlock_ptr = (struct directoryblock *) malloc (sizeof(struct directoryblock));

    //To check is the file already in the directory entry
    for (int i = 0; i < ROOT_BLOCK_NUMBER; i++)
    {
        if(read_block(directoryBlock_ptr,i+5)==-1);
        for (int j = 0; i < 32; j++)
        {
            get_inode_node(i*32+j,inode_ptr);
        if( inode_ptr->usedStatus == 1 && (strcmp(filename, directoryBlock_ptr->entryList[j].fileName) == 0))
            {
                printf("File already exist.");
                return -1;
            }
        }
    }   
    //**********************************************************************
    //Target location in directoryEntry
    struct directoryEntry * directoryEntry_ptr;
    directoryEntry_ptr = (struct directoryEntry *) malloc (sizeof(struct directoryEntry));
    //inode no of the file initalized.
    directoryEntry_ptr->iNodeNo = -1;
    directoryEntry_ptr->size = 0;
   
    strcpy(directoryEntry_ptr->fileName,filename);

    // Update the directory entry and fcb block
    //directory_entry_add(targetlocation,directoryBlock_ptr);
    int inode_no = get_empty_inodeNo_in_fcb();

    int directoryBlockNumber = inode_no / 32; 
    int offset = (inode_no % 32);
    
    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));

    if(read_block(dir_block,directoryBlockNumber + 1) != 1){
        return -1;
    }

    strcpy(dir_block->entryList[offset].fileName, directoryEntry_ptr->fileName);
    dir_block->entryList[offset].iNodeNo = directoryEntry_ptr->iNodeNo;
   
  //  dir_block->entryList[ofset].size = ent->size;
    
    // real part 
    if(write_block(dir_block,directoryBlockNumber + 1) != 1){
        return -1;
    }
    return 1;
}


int sfs_open(char *file, int mode)
{
    struct directoryEntry* tmp;
    tmp = (struct directoryEntry *)malloc(sizeof(struct directoryEntry));
    
    int entryLocation =  directory_entry_location_finder_byName(file,tmp) ;

    if(entryLocation == -1){ printf("Directory entry finder could not find the entry ! \n");return -1;}
   
    int locationHolder = -1 ;
    int a = 0;
    for (int i = 0; i < MAX_FILE_SIZE && a == 0; i++)
    {
        if(available_location_openFileTable[i] == 0){
            locationHolder = i;
            a = 1;
        }
    }
    if(locationHolder == -1){printf("No empty place in openfile table");return -1;} 

    strcpy(open_FileTable[locationHolder].fileName,file);
    open_FileTable[locationHolder].iNodeNo = tmp->iNodeNo;
    open_FileTable[locationHolder].size = tmp->size;

    modes[locationHolder] = mode;
    available_location_openFileTable[locationHolder] = 1;
    entry_position[locationHolder] = entryLocation;

    return locationHolder; 
}


int sfs_close(int fd){
    if(fd < 0 || fd >= 16){
        available_location_openFileTable[fd] = 0;
        return (0); 
    }else
        return -1;
}


int sfs_getsize (int  fd)
{
    return open_FileTable[fd].size;    
}

void increase_lastPosition(int fd,int n){
    last_position[fd] += n;
}

int sfs_read(int fd, void *buf, int n){

    if(fd > MAX_FILE_SIZE || modes[fd] == 0)
        return -1;
    if(open_FileTable[fd].size < (last_position[fd]+n) == 0 )
        return -1;

    //GET Block From Position
    int starting_pos = last_position[fd];
    
    int remained_blocks = (starting_pos + n -1) / BLOCKSIZE - starting_pos/BLOCKSIZE+1;    

    //last_position[fd] (buf_position)
    int buffer_position =0;

    //open_FileTable[fd] fbc
    int current_block_inode = open_FileTable[fd].iNodeNo;

    struct inode * target_inode;
    target_inode = (struct inode *)malloc (sizeof(struct inode *));
    get_inode_node(current_block_inode,target_inode);

    // target_inode is the file is going to read.
    
    struct index_block * tmp;
    tmp = (struct index_block *)malloc (sizeof(struct index_block *));


    read_block(target_inode->indexNodeNo,tmp);

    char * data = (char *) malloc(BLOCKSIZE);
    char * result_data = (char *) malloc(BLOCKSIZE);

    int flag = 0;
    for(int i = 0; i < remained_blocks && flag == 0; i++){

        if(tmp->block_numbers[i] == -1){flag = 1;}
        // For the last block
        if(i == remained_blocks -1){

            int last_offset = (starting_pos + n) % BLOCKSIZE;
            if(last_offset == 0) {last_offset = BLOCKSIZE;}
            for (int  i = last_position[fd] % BLOCKSIZE; i < last_offset; i++)
            {
                // Byte by byte reading
                ((char *)buf)[buffer_position++] = data[i];
            }
            increase_lastPosition(fd,last_offset -last_position[fd]%BLOCKSIZE);
            
        }// Not in the last block
        else{
            for (int  i = last_position[fd] % BLOCKSIZE; i < BLOCKSIZE; i++)
            {
                // Byte by byte reading
                ((char *)buf)[buffer_position++] = data[i];
            }
            increase_lastPosition(fd,BLOCKSIZE -last_position[fd]%BLOCKSIZE);
            read_block(data,tmp->block_numbers[i]);
        }
    }
    return n;
}

int get_index_block_entry(int n, struct index_block *input) {

    int index_block_num = n / (BLOCKSIZE / sizeof(struct index_block *));
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
 //Might throw error.
int sfs_append(int fd, void *buf, int n)
{
  if(fd >= 128 || modes[fd] != MODE_APPEND) {return -1;}

    int current_block_no = open_FileTable[fd].size / BLOCKSIZE;

    if((open_FileTable[fd].size % BLOCKSIZE) > 0){
        current_block_no ++; 
    }

    int new_block_no = (open_FileTable[fd].size +n) /BLOCKSIZE;

    if(((open_FileTable[fd].size +n)% BLOCKSIZE) > 0){
        new_block_no ++; 
    }
    int required_block_count = new_block_no - current_block_no;

     //write buffer;
    //open_FileTable[fd] appendlenicek entrynode
    
    struct inode * node;
    node = (struct inode *) malloc ( sizeof ( struct inode ) );

    struct index_block * new_index_block;
    new_index_block = (struct index_block *) malloc ( sizeof ( struct index_block ) );

    //Free fcb node no (fcbid)
    int free_inode;
    if(free_inode == -1) {return -1;}
  
    get_inode_node(open_FileTable[fd].iNodeNo,free_inode);
    
    int inodeNo;
    int indexNodeNo;
 //   int first_block;
    int usedStatus;
    int indexBlockLocation = MAX_FILE_SIZE + (node->inodeNo);

    node->indexNodeNo = indexBlockLocation + (node->inodeNo); 
    int offset = open_FileTable[fd].size % BLOCKSIZE;

    char* data = (char*)malloc(BLOCKSIZE);
    int counter =0;
     int freeBlockLocation;
    if(required_block_count == 0){
        freeBlockLocation = get_free_block_in_Bitmap();
        if(freeBlockLocation == -1){return -1;}
        read_block(data,indexBlockLocation);
        strncpy(data + offset,(char*)buf,n);

        write_block(data,indexBlockLocation);
        bitmap_block_set(indexBlockLocation);
        new_index_block->block_numbers[counter] = freeBlockLocation;
    }else{

        int calc_size = open_FileTable[fd].size;
        while(calc_size > BLOCKSIZE && counter < 1024){

           freeBlockLocation = get_free_block_in_Bitmap();
           if(freeBlockLocation == -1){return -1;}
           read_block(data,indexBlockLocation);
           // update data
           strncpy(data,(char *)buf, BLOCKSIZE > calc_size ? calc_size : BLOCKSIZE);
           
           buf = (char *)buf+ BLOCKSIZE > calc_size ? calc_size : BLOCKSIZE;
           calc_size -= BLOCKSIZE > calc_size ? calc_size : BLOCKSIZE;
           write_block(data,freeBlockLocation);
           bitmap_block_set(freeBlockLocation);
           new_index_block->block_numbers[counter] = freeBlockLocation;
           counter++;
        }
        if(calc_size > 0 && counter < 1024){
            freeBlockLocation = get_free_block_in_Bitmap();
            buf = (char *)buf+ BLOCKSIZE > calc_size ? calc_size : BLOCKSIZE;
            write_block(data,freeBlockLocation);
            bitmap_block_set(freeBlockLocation);
        }
    }
    
    open_FileTable[fd].size += n;
    directory_entry_add(entry_position[fd],open_FileTable+fd);
    return (n); 
}

int sfs_delete(char *filename)
{
    // Checks in the openfile table for the filename directory
    for (int i = 0; i < 16; i++)
    {
        if(get_inode_node_status(open_FileTable[i].iNodeNo) == 1 && strcmp(open_FileTable[i],filename) == 0)
            {
                return -1;
            }
    }
    struct directoryEntry * dir_entry;
    dir_entry = (struct directoryEntry *) malloc ( sizeof ( struct directoryEntry ) );
    struct inode * fcb_node;
    fcb_node = (struct inode *) malloc ( sizeof ( struct inode ) );
    struct index_block * index_block;
    index_block = (struct index_block *) malloc ( sizeof ( struct index_block ) );
    
    int tmp = directory_entry_location_finder_byName(filename,dir_entry);
    dir_entry->size = 0;
    directory_entry_add(tmp,dir_entry);
    
    //clear inode
    get_inode_node(tmp,fcb_node);
    read_block(fcb_node->indexNodeNo,index_block);
    for (int i = 0; i < 1024; i++)
    {
        //clear index block
        bitmap_block_clear(index_block->block_numbers[i]);
        index_block->block_numbers[i] = 0;
    }
    write_block(fcb_node->indexNodeNo,index_block);
    fcb_node->indexNodeNo = -1;
    fcb_node->usedStatus = 0;
    update_fcb_block(tmp,fcb_node);
    return (0); 
}


// Add print disk method to check the status.

//************************************************
//Directory Entry methods

void directory_block_init(){
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
        if(write_block(current_entry_block,i+5) == 1){
        printf("Writeblock initialized");}
    }
}


// created directory entry added to the specific position.
int directory_entry_add(int directory_enrty_no, struct directoryEntry * ent){
    
    int directoryBlockNumber = directory_enrty_no / 32; 
    int offset = (directory_enrty_no % 32);
    
    struct directoryblock * dir_block;
    dir_block = (struct directoryblock *) malloc(sizeof(struct directoryblock));

    if(read_block(dir_block,directoryBlockNumber + 1) != 1){
        return -1;
    }

    strcpy(dir_block->entryList[offset].fileName, ent->fileName);
    dir_block->entryList[offset].iNodeNo = ent->iNodeNo;
    
    
    // real part 
    if(write_block(dir_block,directoryBlockNumber + 1) != 1){
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
        for(int j = 0; j < 32; j++){
            if(get_empty_inodeNo_in_fcb(dir_block -> entryList[j].iNodeNo) == 1 && (strcmp(name,dir_block -> entryList[j].fileName) == 0)){
                    ent->iNodeNo = dir_block->entryList[j].iNodeNo;
                    strcpy(ent->fileName,dir_block->entryList[j].fileName); 
                    return (i*32)+j;
            }
        }
    }
    return -1;
}



