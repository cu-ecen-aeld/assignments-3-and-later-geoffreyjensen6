/*-----------------------------------------------------------------------------
Author: Geoffrey Jensen
ECEA 5305 Assignment #5
Date: 02/19/2023
Description: Takes an input file name (must include full path including file
  name) and also takes an input string that it writes to this file. Creates the
  file if it doesn't already exist and overwrites file if it does.
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <signal.h>


#define PORT "9000"
#define BACKLOG 20
#define WRITE_FILE "/var/tmp/aesdsocketdata"
#define READ_WRITE_SIZE 1024

bool caught_signal = false;

void flip_ip_addr(char *ip_addr){
	char modified_ip_addr[INET_ADDRSTRLEN];
	char *delim = ".";
	char *token = strtok(ip_addr,delim);
	char temp_values[4][3];
	int count = 1;
	
	while(token != NULL){
		strcpy(temp_values[count-1],token);
		//syslog(LOG_DEBUG, "Temp_values #%i: %s ",count-1,temp_values[count-1]);
		token = strtok(NULL,delim);
		count++;
	}

	sprintf(modified_ip_addr,"%s.%s.%s.%s",temp_values[3],temp_values[2],temp_values[1],temp_values[0]);
	syslog(LOG_DEBUG, "Reversed IP addr for Debug Printing: %s ",modified_ip_addr);
	strcpy(ip_addr,modified_ip_addr);
	
	return;
}

void receive_and_write_to_file(int socket_fd, int writer_fd){
	ssize_t bytes_read, bytes_written;
	char read_buffer[READ_WRITE_SIZE] = "";
	char newline = '\n';
	int end_of_packet = 0;

	while(end_of_packet == 0){
		if((bytes_read = recv(socket_fd,read_buffer,READ_WRITE_SIZE,0)) != 0){
			if(bytes_read == -1) {
				if (errno == EINTR){
					continue;
				}
				perror("SOCKET RECV ERROR: ");
				return;
			}	
			syslog(LOG_DEBUG,"RECV: %li bytes and read_buffer=%s",bytes_read,read_buffer);
			if(strchr(read_buffer,newline) != NULL){
				end_of_packet = 1;
			}
			while(1){
				bytes_written = write(writer_fd, read_buffer, bytes_read);
				if(bytes_written == -1){
					if (errno == EINTR){
						continue;
					}
					perror("SOCKET WRITE ERROR: ");
					return;
				}
				else{
					break;
				}
			}
		}

	}
	return;
}

void read_file_and_send(int socket_fd){
	ssize_t bytes_read, bytes_written;
	char write_buffer[READ_WRITE_SIZE] = "";
	int reader_fd;

	reader_fd = open(WRITE_FILE, 0444);
	while ((bytes_read = read(reader_fd,write_buffer,READ_WRITE_SIZE)) != 0){
		if(bytes_read == -1) {
			if (errno == EINTR){
				continue;
			}
			perror("SOCKET READ ERROR: ");
			return;
		}
		syslog(LOG_DEBUG,"READ: %li bytes and File contents=%s",bytes_read,write_buffer);	
		while(1){
			bytes_written = write(socket_fd,write_buffer,bytes_read);
			if(bytes_written == -1){
				if (errno == EINTR){
					continue;
				}
				perror("SOCKET WRITE ERROR: ");
				return;
			}
			else{
				break;
			}
		}
	}
	close(reader_fd);
	return;
}

static void socket_signal_handler (int signal_number){
	if (signal_number == SIGTERM || signal_number == SIGINT){
		caught_signal = true;
	} 
}

//Check the input argument count to ensure both arguments are provided
int main(int argc, char *argv[]){
	int skt_fd, connected_skt_fd, daemon_pid, ret_val, writer_fd;
	struct addrinfo skt_addrinfo, *res_skt_addrinfo, *rp;
	struct sockaddr connected_sktaddr;
	struct sockaddr_in *ip_addr = (struct sockaddr_in *)&connected_sktaddr;
	struct sockaddr_in server_addr;
	struct in_addr connected_ip = ip_addr->sin_addr;
	char client_ip[INET_ADDRSTRLEN], client_ip_hostview[INET_ADDRSTRLEN];
	struct sigaction socket_sigaction;
	int yes=1;

	openlog(NULL,0,LOG_USER);
	syslog(LOG_DEBUG,"Starting Script Over");	

	//Setup signal handler
	memset(&socket_sigaction,0,sizeof(struct sigaction));
	socket_sigaction.sa_handler=socket_signal_handler;
	if(sigaction(SIGTERM, &socket_sigaction, NULL) != 0) {
		perror("SOCKET Error registering for SIGTERM: ");
	}
	if(sigaction(SIGINT, &socket_sigaction, NULL) != 0) {
		perror("SOCKET Error registering for SIGINT: ");
	}

	//Setup addrinfo struct
	socklen_t sktaddr_size;
	memset(&skt_addrinfo,0,sizeof skt_addrinfo);
	skt_addrinfo.ai_family = AF_UNSPEC;
	skt_addrinfo.ai_socktype = SOCK_STREAM;
	skt_addrinfo.ai_flags = AI_PASSIVE;
	ret_val = getaddrinfo(NULL,PORT,&skt_addrinfo,&res_skt_addrinfo);
	if(ret_val != 0){
		perror("SOCKETS: getaddrinfo() failed: ");
		syslog(LOG_DEBUG,"SOCKETS: gettaddrinfo() returned %s",gai_strerror(ret_val));
		return -1;
	}

	//Open socket and Bind
	for(rp = res_skt_addrinfo; rp != NULL; rp = rp->ai_next){
		skt_fd = socket(PF_INET,SOCK_STREAM | SOCK_NONBLOCK,0);
		if(skt_fd == -1){
			perror("SOCKETS: socket() failed: ");
			continue;
		}	
		if(setsockopt(skt_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1){
			perror("setsockopt");
		}
		ret_val = bind(skt_fd,res_skt_addrinfo->ai_addr,res_skt_addrinfo->ai_addrlen);
		if(ret_val != 0){
			perror("SOCKETS: bind() failed: ");
			syslog(LOG_DEBUG,"SOCKETS: bind() failed");
			continue;
		}
		else{
			syslog(LOG_DEBUG,"SOCKETS: bind() successful");
			break;
		}
	}
	freeaddrinfo(res_skt_addrinfo);
	
	//Listen
	ret_val = listen(skt_fd,BACKLOG);
	if(ret_val != 0){
		perror("SOCKETS: listen() failed: ");
		return -1;
	}

	//Start Daemon if user provided -d argument
	if(argc == 2){
		if(strcmp(argv[1],"-d") == 0){
			syslog(LOG_DEBUG,"Starting Daemon");
			//Create Daemon
			daemon_pid = fork();
			if (daemon_pid == -1){
				return -1;
			}
			else if (daemon_pid != 0){
				exit(EXIT_SUCCESS);
			}
			setsid();
			chdir("/");
			open("/dev/null",O_RDWR);
			dup(0);
			dup(0);
		}
	}
	writer_fd = creat(WRITE_FILE, 0644);

	while(1){
		//Establish Accepted Connection
		sktaddr_size = sizeof connected_sktaddr; 
		if(caught_signal == true){
			syslog(LOG_DEBUG, "Caught signal, exiting");
			close(skt_fd);
			close(writer_fd);
			remove(WRITE_FILE);
			return 0;
		}
		connected_skt_fd = accept(skt_fd,&connected_sktaddr,&sktaddr_size); 
		if(connected_skt_fd == -1){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				continue;
			}
			perror("SOCKETS: accept() failed: ");
			return -1;
		}
		connected_ip.s_addr = ntohl(ip_addr->sin_addr.s_addr);	
		inet_ntop(AF_INET, &connected_ip, client_ip, INET_ADDRSTRLEN);
		syslog(LOG_DEBUG,"Accepted conection from %s (network format)",client_ip);	
		strcpy(client_ip_hostview,client_ip);
		flip_ip_addr(client_ip_hostview);
		syslog(LOG_DEBUG,"Accepted connection from %s (host format)",client_ip_hostview);	

		//Receive, Store, Read back, and Send data back
		receive_and_write_to_file(connected_skt_fd, writer_fd);
		read_file_and_send(connected_skt_fd);

		//Close connection
		close(connected_skt_fd);
		syslog(LOG_DEBUG,"Closed connection from %s",client_ip_hostview);	

	}
	return 0;
}
