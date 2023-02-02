#include "systemcalls.h"
#include "stdlib.h"
#include "unistd.h"
#include "errno.h"
#include "syslog.h"
#include "sys/types.h"
#include "sys/wait.h"
#include "string.h"
#include "fcntl.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int return_val;
    
    return_val = system(cmd);
    if(return_val == -1){
        perror("system Error: ");
	return false;
    }


    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

    va_end(args);
/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    openlog(NULL,0,LOG_USER);

    char* arguments[count+1];
    int j;

    //Grab program name
    char input_str[100];
    strcpy(input_str,command[0]);
    char* split_arg0 = strtok(input_str,"/");
    while(split_arg0 != NULL){ 
	arguments[0]=split_arg0;
	split_arg0=strtok(NULL,"/");
    }
    syslog(LOG_INFO,"REDIRECT - Program Name: %s",arguments[0]);

    //Copy other input parameters from command over to new array
    for(j=1;j<count+1;j++){
	arguments[j] = command[j];
    }
    //Print all arguments to syslog for debug purposes
    for(i=0;i<count+1;i++){
	syslog(LOG_INFO,"REDIRECT - Argument Readout: arguments[%i]:  %s",i,arguments[i]);
    }

    //Fork
    pid_t pid;
    pid = fork();
    if(pid == -1){
        perror("fork Error: ");
	return false;
    }
    
    //Execv - Children Only
    if(pid == 0){
        int return_val;
    	return_val = execv(command[0],arguments);
    	if(return_val == -1){
            perror("execv Error: ");
	    syslog(LOG_INFO,"EXECV ERROR, exiting"); 
	    exit(EXIT_FAILURE);
    	}
	exit(EXIT_SUCCESS);
    }

    //Wait - Parent Only
    if(pid != 0){
	int status;
	pid_t wpid;
	syslog(LOG_INFO, "WAITING ON EXECV");
	wpid = wait(&status);
	syslog(LOG_INFO, "DONE WAITING ON EXECV");
	if(wpid == -1){
	    perror("waitpid Error: ");
	    return false;
	}
	if(WIFEXITED(status)){
    	    if(WEXITSTATUS(status) == EXIT_FAILURE){
		syslog(LOG_INFO,"EXECV: EXIT_FAILURE");
 	        return false;
	    }
	}
    }

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    va_end(args);
    
    openlog(NULL,0,LOG_USER);
    
    char* arguments[count+1];
    int j;

    //Grab file descriptor and open outputfile for write
    int outfile_fd = creat(outputfile, 0644);
    if(outfile_fd == -1){
	    perror("REDIRECT - File Open");
	    return false;
    }

    //Grab program name
    char input_str[100];
    strcpy(input_str,command[0]);
    char* split_arg0 = strtok(input_str,"/");
    while(split_arg0 != NULL){ 
	arguments[0]=split_arg0;
	split_arg0=strtok(NULL,"/");
    }
    syslog(LOG_INFO,"REDIRECT - Program Name: %s",arguments[0]);

    //Copy other input parameters from command over to new array
    for(j=1;j<count+1;j++){
	arguments[j] = command[j];
    }
    //Print all arguments to syslog for debug purposes
    for(i=0;i<count+1;i++){
	syslog(LOG_INFO,"REDIRECT - Argument Readout: arguments[%i]:  %s",i,arguments[i]);
    }
    
    //Fork
    pid_t pid;
    pid = fork();
    if(pid == -1){
        perror("fork Error: ");
	return false;
    }
    
    //Execv - Children Only
    if(pid == 0){
	int dup2_ret_val;
	//Copy the file descriptor which points to the outputfile and place that copy in the STDOUT 
	//File descriptor so that the STDOUT is redirected to a file
	dup2_ret_val = dup2(outfile_fd,STDOUT_FILENO);
	if(dup2_ret_val == -1){
		perror("REDIRECT - dup2: ");
		exit(EXIT_FAILURE);
	}
	//Close out the file descriptor since it is not needed and we only want STDOUT to be writing
	//to this file at a single time
	close(outfile_fd);
        int return_val;	
    	return_val = execv(command[0],arguments);
    	if(return_val == -1){
            perror("execv Error: ");
	    syslog(LOG_INFO,"EXECV ERROR, exiting"); 
	    exit(EXIT_FAILURE);
    	}
	syslog(LOG_INFO,"EXECV RETURNED %i",return_val);
	exit(EXIT_SUCCESS);
    }

    //Wait - Parent Only
    if(pid != 0){
	int status;
	pid_t wpid;
	syslog(LOG_INFO, "WAITING ON EXECV");
	wpid = wait(&status);
	syslog(LOG_INFO, "DONE WAITING ON EXECV");
	if(wpid == -1){
	    perror("waitpid Error: ");
	    return false;
	}
	if(WIFEXITED(status)){
    	    if(WEXITSTATUS(status) == EXIT_FAILURE){
		syslog(LOG_INFO,"EXECV: EXIT_FAILURE");
 	        return false;
	    }
	}
    }

    return true;
}
