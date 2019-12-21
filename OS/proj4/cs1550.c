//  Haoyue Cui (hac113)
/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

//size of a disk block
#define	BLOCK_SIZE 512

//size of the .disk file
#define DISK_SIZE 5*2^20

//bits in a byte
#define BITS_PER_BYTE 8;

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

//bitmap setting
#define MAX_BITMAP_BYTE ((1+ MAX_DIRS_IN_ROOT* (1+ MAX_FILES_IN_DIR))/8)+1
#define MAX_BITMAP_BLOCK_BIT 1+ MAX_DIRS_IN_ROOT* (1+ MAX_FILES_IN_DIR)

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

static FILE* cs1550_open_disk(void){
  FILE *disk = fopen(".disk", "rb");
  if(disk == NULL){
    printf("No .disk file.\n");
    return NULL;
  }
  return disk;
}

static void cs1550_close_disk(FILE* disk){
	if(fclose(disk)){
		printf("Error closing disk file\n");
	}
}

static int cs1550_load_dir_block(char* directory, cs1550_directory_entry* dir){
	if(strlen(directory) == 0){
		printf("directory name not valid\n");
		return -1;
	}
	FILE *disk = cs1550_open_disk();
	if(disk == NULL){
		printf("Error opening .disk\n");
		return -1;
	}
	//create root directory
	cs1550_root_directory *root = malloc(sizeof(cs1550_root_directory));
	if(fread((void*)root, sizeof(cs1550_root_directory), 1, disk) != 1){
		printf("Error reading root directory.\n");
		cs1550_close_disk(disk);
		return -1;
	}
	//loop over root directory finding the directory
	int i;
	for(i = 0; i<root->nDirectories; i++){
		if(strcmp(root->directories[i].dname, directory) == 0){
			int directory_location = root->directories[i].nStartBlock;
			//load the directory to dir
			if(fseek(disk, BLOCK_SIZE*directory_location, SEEK_SET) != 0){
				printf("Error loading the directory block\n");
				cs1550_close_disk(disk);
				return -1;
			}
			if(fread((void*)dir, sizeof(cs1550_directory_entry), 1, disk) != 1){
				printf("Error reading the directory information\n");
				cs1550_close_disk(disk);
				return -1;
			}
			cs1550_close_disk(disk);
			return directory_location;
		}
	}
	cs1550_close_disk(disk);
	return -1;
}

static int cs1550_load_file_block(int directory_location, char* filename, char* extension, struct cs1550_file_directory *file){
	FILE* disk = cs1550_open_disk();
	if(disk == NULL){
		printf("Error opening .disk\n");
		return -1;
	}
	//create directory and load information
	fseek(disk, directory_location*BLOCK_SIZE, SEEK_SET);
	cs1550_directory_entry *dir = malloc(sizeof(cs1550_directory_entry));
	if(fread((void*)dir, sizeof(cs1550_directory_entry), 1, disk) != 1){
		printf("Error reading directory entry\n");
		cs1550_close_disk(disk);
		return -1;
	}

	int i;
	for(i = 0; i< dir->nFiles; i++){
		if(strcmp(dir->files[i].fname, filename) == 0 && strcmp(dir->files[i].fext, extension) == 0){
			int file_location = dir->files[i].nStartBlock;
			//load information for the file
			if(fseek(disk, BLOCK_SIZE*file_location, SEEK_SET) != 0){
				printf("Error loading the file block\n");
				cs1550_close_disk(disk);
				return -1;
			}
			if(fread((void*)file, sizeof(struct cs1550_file_directory), 1, disk) != 1){
				printf("Error reading the file information\n");
				cs1550_close_disk(disk);
				return -1;
			}
			cs1550_close_disk(disk);
			return file_location;
		}
	}	
	cs1550_close_disk(disk);
	return -1;
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	//get the path information into directory, filename and extension
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	directory[0] = '\0';
	filename[0] = '\0';
	extension[0] = '\0';
	if (sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension) == 0)
        printf("Error in parsing path.\n");

	cs1550_directory_entry *dir = malloc(BLOCK_SIZE);
	int directory_location = cs1550_load_dir_block(directory, dir);

	//check if exists
	if (strcmp(path, "/") != 0 && directory_location == -1)
        return -ENOENT;
    //check if it is a file
    if (strlen(filename) == 0) {
    	//it is a directory
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    //it is a file
    int i = 0;
    for (; i < dir->nFiles; i++) {
        if (strcmp(dir->files[i].fname, filename) == 0 && 
            strcmp(dir->files[i].fext, extension) == 0) {
            stbuf->st_mode = S_IFREG | 0666;
            stbuf->st_nlink = 1;
            stbuf->st_size = dir->files[i].fsize;
            return 0;
        }
    }
    return -ENOENT;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	//This line assumes we have no subdirectories
	if(strcmp(path, "/") == 0){
		FILE *disk = cs1550_open_disk();
  		if(disk == NULL){
			printf("Error opening .disk\n");
			return -ENOENT;
		}
		cs1550_root_directory *root = malloc(sizeof(cs1550_root_directory));
    	if(fread((void*)root, sizeof(cs1550_root_directory), 1, disk) != 1){
    		printf("Error reading root directory\n");
    		cs1550_close_disk(disk);
      		return -1;
    	}
		//add subdirectories under root directory to the list
		int i;
		for(i = 0; i < root->nDirectories; i++){
			filler(buf, root->directories[i].dname, NULL, 0);	
		}
		cs1550_close_disk(disk);
	}else{
		char directory[MAX_FILENAME + 1];
		char filename[MAX_FILENAME + 1];
    	char extension[MAX_EXTENSION + 1];
        //set strings to empty
  		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
  
		//direct to the subdirectory
		cs1550_directory_entry *dir = malloc(sizeof(cs1550_directory_entry));
		int directory_location = cs1550_load_dir_block(directory, dir);
		if(directory_location == -1)
			return -ENOENT;

		int i;
		for(i = 0; i < dir->nFiles; i++){
			char newfile[MAX_FILENAME+MAX_EXTENSION+2];
			strcpy(newfile, dir->files[i].fname);
			if(strlen(dir->files[i].fext)>0){
				strcat(newfile, ".");
				strcat(newfile, dir->files[i].fext);
			}
			filler(buf, newfile, NULL, 0);	
		}
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
    //set strings to empty
	memset(directory,  0,MAX_FILENAME  + 1);
	memset(filename, 0,MAX_FILENAME  + 1);
	memset(extension,0,MAX_EXTENSION + 1);
    //check if the directory is not under the root directory or the path is root directory path
  	if(sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension) > 1 || sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension) <= 0){
  		return -EPERM;
  	}

  	//check if the name is beyond 8 chars
  	if(strlen(directory) > MAX_FILENAME) return -ENAMETOOLONG;

  	//check if the directory already exists
	cs1550_directory_entry *dir = malloc(sizeof(cs1550_directory_entry));
	int directory_location = cs1550_load_dir_block(directory, dir);
	if(directory_location != -1)
		return -EEXIST;

	FILE *disk = fopen(".disk","rb+");
	if(disk == NULL)
		return -1;

	cs1550_root_directory *root = malloc(sizeof(cs1550_root_directory));
	if(fread((void*)root, sizeof(cs1550_root_directory), 1, disk) != 1){
		printf("Error reading root directory.\n");
		cs1550_close_disk(disk);
		return -ENOENT;
	}

	//check if the root directory could hold new directory
	if(root->nDirectories == MAX_DIRS_IN_ROOT){
		printf("Could not make new directory\n");
		cs1550_close_disk(disk);
		return -ENOSPC;
	}
	//check bitmap setting
	unsigned char bitmap[MAX_BITMAP_BYTE];
	if(fseek(disk, -sizeof(bitmap), SEEK_END) != 0){
		printf("Error seeking the location of the bitmap\n");
		cs1550_close_disk(disk);
		return -1;
	}
	fread(bitmap, sizeof(bitmap), 1, disk);

	int i;
	unsigned int byte, bit;
	unsigned char mask;
	for(i = 1; i<= MAX_BITMAP_BLOCK_BIT; i++){
		byte = i / BITS_PER_BYTE;
		bit = i % BITS_PER_BYTE;
		mask = 0x80 >> bit;
		if((bitmap[byte]&mask) == 0){
			break;
		}
	}
	//check if bitmap is full
	if(i == (MAX_BITMAP_BLOCK_BIT + 1)){
		printf("Bitmap is already full\n");
		cs1550_close_disk(disk);
		return -EPERM;
	}
	
	//set bitmap 0 to 1 as used
	bitmap[byte] |= mask;

	//updating bitmap
	if(fseek(disk, -sizeof(bitmap), SEEK_END) != 0 || !fwrite(bitmap, sizeof(bitmap), 1, disk)){
		printf("Error updating the bitmap\n");
		cs1550_close_disk(disk);
		return -1;
	}
	strcpy(root->directories[root->nDirectories].dname, directory);
	root->directories[root->nDirectories].nStartBlock = i;
	root->nDirectories++;

	//updating root directory
	if(fseek(disk, 0, SEEK_SET) != 0|| !fwrite(root, sizeof(cs1550_root_directory), 1, disk)){
		printf("Error updating root directory\n");
		cs1550_close_disk(disk);
		return -1;
	}

	//updating directory information
	cs1550_directory_entry *new_dir = malloc(sizeof(cs1550_directory_entry));
	memset(new_dir, 0, BLOCK_SIZE);
	if(fseek(disk, BLOCK_SIZE * i, SEEK_SET) != 0){
		printf("Error seeking to the right place\n");
		cs1550_close_disk(disk);
		return -1;
	}

	if(!fwrite((void*)new_dir, sizeof(cs1550_directory_entry), 1, disk)){
		printf("Error updating the directory entry\n");
		cs1550_close_disk(disk);
		return -1;
	}
	cs1550_close_disk(disk);
	return 0;
}



/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
    //set strings to empty
	memset(directory,  0,MAX_FILENAME  + 1);
	memset(filename, 0,MAX_FILENAME  + 1);
	memset(extension,0,MAX_EXTENSION + 1);
    //check if the directory is not under the root directory or the path is root directory path
  	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

  	//check if the name is beyond 8 chars
  	if(strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION) return -ENAMETOOLONG;

	//check if the directory exists
	cs1550_directory_entry *dir = malloc(sizeof(cs1550_directory_entry));
	int directory_location = cs1550_load_dir_block(directory, dir);
	if(directory_location == -1){
		printf("Error locating directory in path\n");
		return -1;
	}

	//check if the directory could contain another file
	if(dir->nFiles == MAX_FILES_IN_DIR){
		printf("Could not make new files\n");
		return -1;
	}

  	//check if the file already exists
	struct cs1550_file_directory *file = malloc(sizeof(struct cs1550_file_directory));
	int file_location = cs1550_load_file_block(directory_location, filename, extension, file);
	if(file_location > 0){
		printf("The file already exists\n");
		return -EEXIST;
	}

	FILE *disk = fopen(".disk","rb+");
	if(disk == NULL)
		return -ENOENT;

	unsigned char bitmap[MAX_BITMAP_BYTE];

	if(fseek(disk, -sizeof(bitmap), SEEK_END) != 0){
		printf("Error seeking the location of the bitmap\n");
		cs1550_close_disk(disk);
		return -1;
	}
	fread(bitmap, sizeof(bitmap), 1, disk);
	int i;

	unsigned int byte, bit;
	unsigned char mask;
	for(i = 1; i<= MAX_BITMAP_BLOCK_BIT; i++){
		byte = i / BITS_PER_BYTE;
		bit = i % BITS_PER_BYTE;
		mask = 0x80 >> bit;
		if((bitmap[byte]&mask) == 0){
			break;
		}
	}
	//check if bitmap is full
	if(i == (MAX_BITMAP_BLOCK_BIT + 1)){
		printf("Bitmap is already full\n");
		cs1550_close_disk(disk);
		return -EPERM;
	}

	//set bitmap 0 to 1 as used
	bitmap[byte] |= mask;

	if(fseek(disk, -sizeof(bitmap), SEEK_END) != 0 || !fwrite(bitmap, sizeof(bitmap), 1, disk)){
		printf("Error updating the bitmap\n");
		cs1550_close_disk(disk);
		return -1;
	}

	//add file to the subdirectory
	strcpy(dir->files[dir->nFiles].fname, filename);
	strcpy(dir->files[dir->nFiles].fext, extension);
	dir->files[dir->nFiles].nStartBlock = i;
	dir->files[dir->nFiles].fsize = 0;
	dir->nFiles++;

	//write the updated subdirectory to disk
	if(fseek(disk, directory_location*BLOCK_SIZE, SEEK_SET) != 0|| !fwrite(dir, sizeof(cs1550_directory_entry), 1, disk)){
		printf("Error updating directory entry\n");
		cs1550_close_disk(disk);
		return -1;
	}

	//fill the file block
	cs1550_disk_block *new_file = malloc(BLOCK_SIZE);
	memset(new_file, 0, BLOCK_SIZE);

	if(fseek(disk, BLOCK_SIZE * i, SEEK_SET) != 0){
		printf("Error seeking to the right place\n");
		cs1550_close_disk(disk);
		return -1;
	}

	if(!fwrite((void*)new_file, BLOCK_SIZE, 1, disk)){
		printf("Error adding block for new file\n");
		cs1550_close_disk(disk);
		return -1;
	}
	cs1550_close_disk(disk);
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;
    return 0;
}


/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) fi;
	//check to make sure path exists
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
    directory[0] = '\0'; 
	filename[0] = '\0';
	extension[0] = '\0';
  	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
  	if(strlen(filename) == 0){
  		printf("Filename is empty\n");
  		return -EISDIR;
  	}

	//check if the directory exists
	cs1550_directory_entry *dir = malloc(sizeof(cs1550_directory_entry));
	int directory_location = cs1550_load_dir_block(directory, dir);
	if(directory_location == -1){
		printf("Error locating directory in path\n");
		return -1;
	}

	//find the location of the file
	int k;
	for(k = 0; k< dir->nFiles; k++){
		if(strcmp(dir->files[k].fname, filename) == 0 && strcmp(dir->files[k].fext, extension) == 0){
			break;
		}
	}
	//check if the file exists
	if(k == dir->nFiles){
		printf("File not found\n");
		return -1;
	}
	//check that size is > 0
	if(size <= 0){
		printf("Size is not bigger than 0");
		return -1;
	}
	//get the file from the directory
	struct cs1550_file_directory *file = &(dir->files[k]);

	//check that offset is <= to the file size
	if(offset > file->fsize){
		printf("Offset is beyond the file size\n");
		return -EFBIG;
	}

	FILE *disk = fopen(".disk","rb+");
	if(disk == NULL){
		printf("Error opeing the .disk file\n");
		return -1;
	}

	//read in data
	//set size and return, or error
	fseek(disk, file->nStartBlock * BLOCK_SIZE + offset, SEEK_SET);
	if(size > (file->fsize-offset)){
		size = file->fsize - offset;
	}
	fread(buf, size, 1, disk);
	cs1550_close_disk(disk);
	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];

  	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
  	if(strlen(filename) == 0){
  		printf("Error reading the file\n");
  		return -1;
  	}

	//check if the directory exists
	cs1550_directory_entry *dir = malloc(sizeof(cs1550_directory_entry));
	int directory_location = cs1550_load_dir_block(directory, dir);
	if(directory_location == -1){
		printf("Error locating directory in path\n");
		return -1;
	}

	//find the location of the file
	int k;
	for(k = 0; k< dir->nFiles; k++){
		if(strcmp(dir->files[k].fname, filename) == 0 && strcmp(dir->files[k].fext, extension) == 0){
			break;
		}
	}
	//check if the file exists
	if(k == dir->nFiles){
		printf("File not found\n");
		return -1;
	}

	//check that size is > 0
	if(size <= 0){
		printf("Size is not bigger than 0");
		return -1;
	}

	//get the file from the directory
	struct cs1550_file_directory *file = &(dir->files[k]);

	//check that offset is <= to the file size
	if(offset > file->fsize){
		printf("Offset is beyond the file size\n");
		return -EFBIG;
	}

	//write data
	FILE *disk = fopen(".disk","rb+");
	if(disk == NULL){
		printf("Error opeing the .disk file\n");
		return -1;
	}
	//check bitmap
	unsigned char bitmap[MAX_BITMAP_BYTE];

	if(fseek(disk, -sizeof(bitmap), SEEK_END) != 0){
		printf("Error seeking the location of the bitmap\n");
		cs1550_close_disk(disk);
		return -1;
	}
	fread(bitmap, sizeof(bitmap), 1, disk);
	unsigned int byte, bit;
	unsigned char mask;

	int i;
	if(offset+size > file->fsize){
		//check if new block needed
		for(i = file->fsize/BLOCK_SIZE + 1; i <= (offset+size)/BLOCK_SIZE; i++){
			int j = file->nStartBlock + i;
			byte = j / BITS_PER_BYTE;
			bit = j % BITS_PER_BYTE;
			mask = 0x80 >> bit;
			if((bitmap[byte]&mask) != 0){
				printf("Could not allocate contiguous block\n");
				cs1550_close_disk(disk);
				return -1;
			}
			bitmap[byte] |= mask;
		}		
	}
	//update bitmap
	if(fseek(disk, -sizeof(bitmap), SEEK_END) != 0 || !fwrite(bitmap, sizeof(bitmap), 1, disk)){
		printf("Error updating the bitmap\n");
		cs1550_close_disk(disk);
		return -1;
	}
	//update the directory
	file->fsize = offset + size;
	if(fseek(disk, directory_location*BLOCK_SIZE, SEEK_SET) != 0 || !fwrite(dir, BLOCK_SIZE, 1, disk)){
		printf("Error updating the directory\n");
		cs1550_close_disk(disk);
		return -1;
	}
	//update file
	fseek(disk, file->nStartBlock*BLOCK_SIZE + offset, SEEK_SET);
	fwrite(buf, size, 1, disk);
	cs1550_close_disk(disk);
	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
