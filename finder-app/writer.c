/*-----------------------------------------------------------------------------
Author: Geoffrey Jensen
ECEA 5305 Assignment #2
Date: 01/24/2023
Description: Takes an input file name (must include full path including file
  name) and also takes an input string that it writes to this file. Creates the
  file if it doesn't already exist and overwrites file if it does.
-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

//Check the input argument count to ensure both arguments are provided
int main(int argc, char *argv[]){
	int writer_fd;
	ssize_t num_bytes_written;
	int error_val;

	openlog(NULL,0,LOG_USER);
	//Check the input argument count to ensure both arguments are provided
	if(argc != 3){
		printf("Incorrect number of input arguments provided\n");
		printf("Please ensure you use the following format:\n");
		printf("./writer FileName TextString\n");
		printf("Argument #1 (FileName) must be the full path to a file and include the filename\n");
		printf("Argument #2 (TextString is a string that will be written to that file\n");
		return 1;
	}
	else{
		//Implementation of writing input string to a file. 
		//Assume directory exists. Then regardless just write contents
		syslog(LOG_DEBUG,"Writing %s to %s", argv[2], argv[1]);

		writer_fd = creat(argv[1], 0644);
		if(writer_fd == -1){
			error_val = errno;
			printf("Error in opening file %s\n", argv[1]);
			printf("File Descriptor returned is -1\n");
			printf("Errno: %s\n", strerror(error_val));
			syslog(LOG_ERR,"Error in opening file %s", argv[1]);
			syslog(LOG_ERR,"File Descriptor returned is -1");
			syslog(LOG_ERR,"Errno: %s", strerror(error_val));
			return 1;
		}

		num_bytes_written = write(writer_fd, argv[2], strlen(argv[2]));
		if(num_bytes_written != strlen(argv[2])){
			error_val = errno;
			printf("Error in writing to file %s\n", argv[1]);
			printf("Returned value is %li\n", num_bytes_written);
			printf("Errno: %s\n", strerror(error_val));
			syslog(LOG_ERR,"Error in writing to file %s", argv[1]);
			syslog(LOG_ERR,"Returned value is %li", num_bytes_written);
			syslog(LOG_ERR,"Errno: %s", strerror(error_val));
			return 1;
		}

		return 0;		
	}

}
