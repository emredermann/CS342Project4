

// Do not change this file //

#define MODE_READ 0
#define MODE_APPEND 1
#define BLOCKSIZE 4096 // bytes
#define FILENAMESIZE 110
#define ENTRY_PER_BLOCK 32
#define MAXFILES 32
#define ENTRYBLOCKS 4
#define MAX_FILE_SIZE 128

struct inode
{
    int size;
    int nodeID;
    int dataBlockNumber;
    int pointersToBlocks[];

};
struct block
{
    struct directoryEntry entryList[ENTRY_PER_BLOCK];
};

struct directoryEntry
{
    char fileName[FILENAMESIZE];
    int iNodeNo;

/*
    int64_t first_block;
    int64_t last_block;
    int64_t size;
    int64_t read_buf;
    int64_t read_fat;
    int64_t capacity;
*/
};

struct superBlock
{
    int iNodeCount;
    int blocksCount;
    int reservedBlocksCount;
    int freeBlockCount;
    int freeiNodeCount;
    int firstDataBlock;
    int blockSize;
    int blocksPerGroup;   
    struct directoryEntry dir_Entry [MAX_FILE_SIZE];
    int freeFCB[MAX_FILE_SIZE];
};

struct superBlock super_block;
struct block block;
struct directoryEntry dir_Entry;


int create_format_vdisk (char *vdiskname, unsigned int  m);

int sfs_mount (char *vdiskname);

int sfs_umount ();

int sfs_create(char *filename);

int sfs_open(char *filename, int mode);

int sfs_close(int fd);

int sfs_getsize (int fd);

int sfs_read(int fd, void *buf, int n);

int sfs_append(int fd, void *buf, int n);

int sfs_delete(char *filename);


