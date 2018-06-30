/*
Gaganjeet Singh
1001070011
Assignment 5
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define NUM_BLOCKS	4226
#define BLOCK_SIZE	8192
#define NUM_FILES	128
#define NUM_INODES	128
#define MAX_BLOCKS_PER_FILE	32


//All of the data, including the directory and inodes, will be saved in this 2D array
unsigned char data_blocks[NUM_BLOCKS][BLOCK_SIZE];

//Keeps track of which blocks are available
int used_blocks[NUM_BLOCKS];

//There will be an entry for every file placed in the file system
//All entries will be in block 0 of the data array, accessed through a pointer
struct directory_entry{
	char* name;
	int valid;
	int inode_idx;
};

//This will hold the meta data for each file, and also which blocks hold the file data
//Each inode will take up a whole block 
struct inode{
	time_t date;
	int size;
	int valid;
	int blocks[MAX_BLOCKS_PER_FILE];
};

//these pointers will allow us to easily go through the directory entries and the inodes
struct directory_entry* dir_ptr;
struct inode* inode_arr_ptr[NUM_INODES];


//This function finds a free place to put in the file. There are a total of 128 available entries.
//It takes no paramter, and simply returns the index of the available entry as an int. 
int findFreeDirectoryEntry(){
	int i; 
	int retval = -1;
	for(i = 0; i < 128; i++){
		
		//if the valid bit is 0, then it means the entry is not being used
		//1 means it is being used. 
		if(dir_ptr[i].valid == 0){
			retval = i;
			break;
		}	
	}
	return retval;
}


//This function returns the free block. It needs no paramter and simply returns the index
//as an int. It goes through the used blocks array and when it finds the first free block,
//it returns it. 
int findFreeBlock(){
	int retval = -1;
	int i = 0;
	
	for(i = 130; i < NUM_BLOCKS; i++){
		if(used_blocks[i] == 0){
			retval = i;
			break;
		}
	}
	return retval;
}


//For an inode, it goes through it and sees if the file can hold more blocks. If it can
//then the function returns the index of that block within the inode as an int. 
//params - inode_index -> the index of the inode we are looking through 
int findFreeInodeBlockEntry(int inode_index){
	int i;
	int retval = -1;
	for(i = 0; i < MAX_BLOCKS_PER_FILE; i++){
		if(inode_arr_ptr[inode_index]->blocks[i] == -1){
			retval = i;
			break;
		}
	}
	return retval;
}


//The system holds a total of 128 files, so this function looks through the inode array to 
//find an inode not being used. It then returns its index as an int. 
int findFreeInode(){
	int i;
	int retval = -1;
	for(i = 0; i < 128; i++){
		if(inode_arr_ptr[i]->valid == 0){
			retval = i;
			break;
		}
	}
	return retval;
}


//df is one of the primary required functions. It returns how much space is available in the
//file system. It goes through the used blocks array to find the available space. It returns 
//the available space in units of unsigned char 
int df(){
	int count = 0;
	int i = 0;
	
	for(i = 130; i < NUM_BLOCKS; i++){
		if(used_blocks[i] == 0){
			count++;
		}
	}
	return count * BLOCK_SIZE;
}


//put is one of the primary required functions. It places a file into the file system. 
//params - filename -> a string that holds the name of the file we have to put in. 
void put(char* filename){

	//filename has to be less than 32
	if(strlen(filename) > 32){
		printf("put error: File name too long\n");
		return;
	}

	//using stat, we get the specs of the file we are looking for. We need the files size. 
	struct stat buf;
	
	int status = stat(filename, &buf);
	
	if(status == -1){
		printf("put error: File not found\n");
		return;
	}
	
	//This checks if there is enough space in the file system 
	if(buf.st_size > df()){
		printf("put error: Not enough room in the file system\n");
		return;
	}
	
	int dir_idx = findFreeDirectoryEntry();

	if(dir_idx == -1){
		printf("put error: Not enough room in the file system\n");
		return;
	}
	
	//setting up the directory entry here 
	dir_ptr[dir_idx].valid = 1;
	dir_ptr[dir_idx].name = (char*)malloc(strlen(filename));

	strncpy(dir_ptr[dir_idx].name, filename, strlen(filename));

	int inode_idx = findFreeInode();

	if(inode_idx == -1){
		printf("put error: No free inode\n");
		return;
	}
	
	//setting up the inode and its metadata here. 
	dir_ptr[dir_idx].inode_idx = inode_idx;
	
	inode_arr_ptr[inode_idx]->size = buf.st_size;
	inode_arr_ptr[inode_idx]->date = time(NULL);
	inode_arr_ptr[inode_idx]->valid = 1;
	
	//using basic file commands to open and read file
	FILE* ifp = fopen(filename, "r");
	
	int copy_size = buf.st_size;
	int offset = 0;

	//we go until we can cut off whole block size chuncks from the file here. 
	//we will take care of the remainder later. 
	while(copy_size >= BLOCK_SIZE){
		int block_index = findFreeBlock();

		if(block_index == -1){
			printf("put error: Cannot find free block\n");
			//Cleanup directory and inode. This should not happen.
			return;
		}

		used_blocks[block_index] = 1;
		int inode_block_entry = findFreeInodeBlockEntry(inode_idx);
		if(inode_block_entry == -1){
			printf("put error: Cannot find free node block\n");
			return;
		}
		inode_arr_ptr[inode_idx]->blocks[inode_block_entry] = block_index;
		
		fseek(ifp, offset, SEEK_SET);
		int bytes = fread(data_blocks[block_index], 1, BLOCK_SIZE, ifp);
		
		if(bytes == 0 && !feof(ifp)){
			printf("put error: Read 0 bytes without reaching end of file\n");
			return;
		}
		
		clearerr(ifp);
		
		copy_size	-= BLOCK_SIZE;
		offset		+= BLOCK_SIZE;
	}
	
	//we handle the remainder here. 
	if(copy_size > 0){
		int block_index = findFreeBlock();
	
		if(block_index == -1){
			printf("put error: Cannot find free block\n");
			//Cleanup directory and inode. This should not happen.
			return;
		}
		
		used_blocks[block_index] = 1;
		int inode_block_entry = findFreeInodeBlockEntry(inode_idx);
		if(inode_block_entry == -1){
			printf("put error: Cannot find free node block\n");
			return;
		}
		inode_arr_ptr[inode_idx]->blocks[inode_block_entry] = block_index;

		fseek(ifp, offset, SEEK_SET);
		fread(data_blocks[block_index], copy_size, 1, ifp);
	}
}


//List is a required function that lists the files in the directory
//It goes through the directory array to see which ones
//are valid. Prints within this function
void list(){
	int i = 0;
	int count = 0; 				//there to check if any files were found
	for(i = 0; i < 128; i++){
		if(dir_ptr[i].valid == 1){
			int inode_index = dir_ptr[i].inode_idx;

			//using timeinfo to get the time from the stored date attribute
			struct tm* timeinfo;
			char buff[20];
			timeinfo = localtime(&(inode_arr_ptr[inode_index]->date));
			strftime(buff,20,"%b %d %H:%M",timeinfo);
			printf("%d\t %s\t %s\n",inode_arr_ptr[inode_index]->size, buff, dir_ptr[i].name);
			count++;
		}
	}
	if(count == 0){
		printf("list: No files found\n");
	}
}


//Deldir is a function used by del to delete a directory. 
//deletes the given directory index. It also goes through all the blocks and sets them as invalid.
//params - i -> index of the directory to be deleted.  
void deldir(int i){
	free(dir_ptr[i].name);
	dir_ptr[i].valid = 0;
	int inode_idx = dir_ptr[i].inode_idx;
	int j = 0;
	for(j = 0; j < MAX_BLOCKS_PER_FILE; j++){
		int block_idx = inode_arr_ptr[inode_idx]->blocks[j];
		if(block_idx != -1){
			used_blocks[block_idx] = 0;
			inode_arr_ptr[inode_idx]->blocks[j] = -1;
		}
	}
	inode_arr_ptr[inode_idx]->valid = 0;
}


//del is one of the required functions. It looks for the file with the given name and calls 
//deldir to delete that directory. 
//params - filename -> name of the file to be deleted. 
void del(char* filename){
	int i = 0;

	for(i = 0; i < NUM_FILES; i++){
		if(dir_ptr[i].valid == 1){
			if(strcmp(filename, dir_ptr[i].name) == 0){
				deldir(i);
				return;
			}
		}
	}
	printf("del error: File not found\n");
}


//storefile stores the file from the file system to the current directory. It is used by get
//params - inode_idx -> index of the inode of the file to be copied over. 
//		 - newfilename -> name to give that file. 
void storefile(int inode_idx, char* newfilename){
	int i = 0;
	int j = 0;
	int filesize = inode_arr_ptr[inode_idx]->size;
	FILE* fp = fopen(newfilename, "w");

	if(fp == NULL){
		printf("get error: could not open file: %s", newfilename);
		return;
	}

	//we write one char at a time
	for(i = 0; i < MAX_BLOCKS_PER_FILE; i++){
		int block_idx = inode_arr_ptr[inode_idx]->blocks[i];
		if(block_idx != -1){
			//write the chars in the block until filesize is 0. filesize is decremented
			//after every input of a character. 
			for(j = 0; filesize > 0 && j < BLOCK_SIZE; j++){
				fputc(data_blocks[block_idx][j], fp);
				filesize -= 1;
			}
		}
	}
	fclose(fp);
}


//get is one of the required functions. It copies over a file from the file system to the
//current working directory. 
//This is a wrapper function that calls storefile once it finds the file. 
//params - filename -> file to look for in the file system. 
//		 - newfilename -> what to call the file in the directory. if null, then it is same name
//							as filename. 
void get(char* filename, char* newfilename){
	
	//checking to see if we need to correct the newfilename
	if(newfilename == NULL){
		newfilename = (char*)malloc(sizeof(char)*strlen(filename));
		strncpy(newfilename, filename, strlen(filename));
	}

	int i = 0;
	for(i = 0; i < NUM_FILES; i++){
		if(dir_ptr[i].valid == 1){
			if(strcmp(filename, dir_ptr[i].name) == 0){
				storefile(dir_ptr[i].inode_idx, newfilename);
				return;
			}
		}
	}
	printf("get error: File not found\n");
}


//this initializes all the arrays and sets the valid bits. 
void init(){
	//setting up directory entries
	dir_ptr = (struct directory_entry*)&data_blocks[0];
	int i = 0;
	for(i = 0; i < NUM_FILES; i++){
		dir_ptr[i].valid = 0;
	}
	used_blocks[0] = 1;

	//setting up inodes. 
	int j = 0;
	for(i = 0; i < 129; i++){
		inode_arr_ptr[i] = (struct inode*)&data_blocks[i+1];
		for(j = 0; j < MAX_BLOCKS_PER_FILE; j++){
			inode_arr_ptr[i]->blocks[j] = -1;
		}
		used_blocks[i+1] = 1;
	}
	
	//setting the data blocks 
	for(i = 130; i < NUM_BLOCKS; i++){
		used_blocks[i] = 0;
	}
}

//main function first initializes everything with init, then runs an endless loop
//loop only breaks if the user types in exit. 
//to go through the paramters, string tokenization is used
//with space and end line as the deliminators. 
int main() {

	init();
	char input[100];
	while(1){
		char* token;
		printf("mfs>");
		fgets(input, 100, stdin);
		token = strtok(input, " \n");
		
		if(strcmp(token, "exit") == 0){
			break;
		}
		
		if(strcmp(token, "list") == 0){
			list();
		}

		else if(strcmp(token, "put") == 0){
			//getting the second paramter, the file name
			token = strtok(NULL, " \n");
			put(token);
		}

		else if(strcmp(token, "del") == 0){
			//getting the second paramter, the file name
			token = strtok(NULL, "\n");
			del(token);
		}

		else if(strcmp(token, "df") == 0){
			printf("%d\n",df());
		}

		else if(strcmp(token, "get") == 0){
			//getting the second paramter, the file name to look for
			token = strtok(NULL, " \n");
			char* filename = token;
			token = strtok(NULL, " \n");
			//getting the third paramter, the name to give the file. 
			//if there is no third paramter, token is set to NULL and we fix that in get
			char* newfilename = token;
			get(filename, newfilename);
		}
	}	
	return 0;
}
