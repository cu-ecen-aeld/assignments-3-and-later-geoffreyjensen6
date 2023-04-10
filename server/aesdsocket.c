/*-----------------------------------------------------------------------------
Author: Geoffrey Jensen
ECEA 5306 Assignment #6
Date: 03/21/2023
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
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>

#define USE_AESD_CHAR_DEVICE 1  //Set to 1 by default in Assignment 8

#ifdef USE_AESD_CHAR_DEVICE
#  define WRITE_FILE "/dev/aesdchar"
#else
#  define WRITE_FILE "/var/tmp/aesdsocketdata"
#endif

#define PORT "9000"
#define BACKLOG 20
#define READ_WRITE_SIZE 1024

bool caught_signal = false;

struct arg_struct {
	int connected_skt_fd;
	int write_fd;
	int thread_complete;
};

struct thread_info {
	pthread_t thread_id;
	struct arg_struct input_args;
};

struct slist_data_struct {
	struct thread_info tinfo;
	SLIST_ENTRY(slist_data_struct) entries;
};

SLIST_HEAD(slisthead,slist_data_struct) head = SLIST_HEAD_INITIALIZER(head);
pthread_mutex_t write_lock;

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
	//syslog(LOG_DEBUG, "Reversed IP addr for Debug Printing: %s ",modified_ip_addr);
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
				syslog(LOG_DEBUG,"FD is %i",socket_fd);
				return;
			}	
			//syslog(LOG_DEBUG,"RECV: %li bytes and read_buffer=%s",bytes_read,read_buffer);
			if(strchr(read_buffer,newline) != NULL){
				end_of_packet = 1;
			}
			while(1){
				if(pthread_mutex_lock(&write_lock) == 0){
					bytes_written = write(writer_fd, read_buffer, bytes_read);
					if(bytes_written == -1){
						if (errno == EINTR){
							continue;
						}
						perror("SOCKET WRITE ERROR: ");
						return;
					}
					else{
						pthread_mutex_unlock(&write_lock);
						break;
					}
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
		//syslog(LOG_DEBUG,"READ: %li bytes and File contents=%s",bytes_read,write_buffer);	
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

void *data_processor(void *input_args){
	struct arg_struct *in_args = input_args;
	//Receive, Store, Read back, and Send data back
	receive_and_write_to_file(in_args->connected_skt_fd, in_args->write_fd);
	read_file_and_send(in_args->connected_skt_fd);
	in_args->thread_complete = 1;

	return input_args;
}

int add_slist_entry(int connected_skt_fd, int writer_fd){
	struct slist_data_struct *entry;
	struct slist_data_struct *current_entry;

	entry  = (struct slist_data_struct*)malloc(sizeof(struct slist_data_struct));

	entry->tinfo.input_args.connected_skt_fd = connected_skt_fd;
	entry->tinfo.input_args.write_fd = writer_fd;
	entry->tinfo.input_args.thread_complete = 0;
	pthread_create(&entry->tinfo.thread_id,NULL,data_processor,(void *)&entry->tinfo.input_args);

	if(SLIST_EMPTY(&head) != 0){
		SLIST_INSERT_HEAD(&head, entry, entries);
	}
	else{
		SLIST_FOREACH(current_entry, &head, entries){
			if(current_entry->entries.sle_next == NULL){
				SLIST_INSERT_AFTER(current_entry, entry, entries);
				break;
			}
		}
	}
	return 0;
}

void timestamp_logger(union sigval sigval){
	syslog(LOG_DEBUG,"TIMER THREAD REACHED");
	char write_string[256];
	int bytes_written;
	time_t time_val;
	struct thread_info *thread_data = (struct thread_info*) sigval.sival_ptr;
	struct tm *tmp;
	time_val = time(NULL);
	tmp = localtime(&time_val);
	strftime(write_string, sizeof(write_string), "timestamp: %c\n",tmp);
	syslog(LOG_DEBUG, "SOCKET TIMER: %s", write_string);
	while(1){
		if(pthread_mutex_lock(&write_lock) == 0){
			bytes_written = write(thread_data->input_args.write_fd, write_string, strlen(write_string));
			if(bytes_written == -1){
				if (errno == EINTR){
					continue;
				}
				perror("SOCKET WRITE ERROR: ");
			}
			else{
				pthread_mutex_unlock(&write_lock);
				break;
			}	
		}
	}
}

//Check the input argument count to ensure both arguments are provided
int main(int argc, char *argv[]){
	int skt_fd, connected_skt_fd, daemon_pid, ret_val, writer_fd;
	struct addrinfo skt_addrinfo, *res_skt_addrinfo, *rp;
	struct sockaddr connected_sktaddr;
	struct sockaddr_in *ip_addr = (struct sockaddr_in *)&connected_sktaddr;
	struct in_addr connected_ip = ip_addr->sin_addr;
	char client_ip[INET_ADDRSTRLEN], client_ip_hostview[INET_ADDRSTRLEN];
	struct sigaction socket_sigaction;
	int yes=1;
	struct sigevent sig_event;
	struct thread_info thread_data;
	struct itimerspec itimer;
	timer_t timer_id;
	struct slist_data_struct *current_entry;

	openlog(NULL,0,LOG_USER);
	syslog(LOG_DEBUG,"Starting Script Over");	
	writer_fd = creat(WRITE_FILE, 0644);

	//Initialize SLIST Head
	SLIST_INIT(&head);

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
	skt_addrinfo.ai_family = AF_INET;
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

	//Initialize Timer 	
	if(!USE_AESD_CHAR_DEVICE){
		memset(&sig_event,0,sizeof(struct sigevent));
		sig_event.sigev_notify = SIGEV_THREAD;
		sig_event.sigev_notify_function = timestamp_logger;
		sig_event.sigev_value.sival_ptr = &thread_data;

		itimer.it_value.tv_sec = 10;
		itimer.it_value.tv_nsec = 0;
		itimer.it_interval.tv_sec = 10;
		itimer.it_interval.tv_nsec = 0;
	
		ret_val = timer_create(CLOCK_MONOTONIC,&sig_event,&timer_id);
		if(ret_val != 0){
			perror("SOCKET Error creating timer: ");
			return -1;
		}
		ret_val = timer_settime(timer_id,0,&itimer,NULL);
		if(ret_val != 0){
			perror("SOCKET Error setting timer: ");
			return -1;
		}
	}

	thread_data.input_args.write_fd = writer_fd;

	while(1){
		//Establish Accepted Connection
		sktaddr_size = sizeof connected_sktaddr; 
		if(caught_signal == true){
			syslog(LOG_DEBUG, "SOCKET: Caught signal, exiting");
			timer_delete(timer_id);
			SLIST_FOREACH(current_entry, &head, entries){
				if(current_entry->tinfo.input_args.thread_complete == 1){
					pthread_join(current_entry->tinfo.thread_id,NULL);
				}
				//Close connection
				close(current_entry->tinfo.input_args.connected_skt_fd);
			}
			//Free all elements of SLIST
			struct slist_data_struct *tmp_slist_struct;
			while(SLIST_EMPTY(&head) != 0){
				tmp_slist_struct = SLIST_FIRST(&head);
				SLIST_REMOVE_HEAD(&head, entries);
				free(tmp_slist_struct);
			}
			close(skt_fd);
			close(writer_fd);
			if(!USE_AESD_CHAR_DEVICE){
				remove(WRITE_FILE);
			}
			closelog();
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
		
		//Launch Thread and Create SLIST entry to store thread ID
		add_slist_entry(connected_skt_fd, writer_fd);
		//Check for Threads ready for joining
		SLIST_FOREACH(current_entry, &head, entries){
			if(current_entry->tinfo.input_args.thread_complete == 1){
				syslog(LOG_DEBUG,"SLIST: Attempting to join thread pointed to by %i",current_entry->tinfo.input_args.connected_skt_fd);
				pthread_join(current_entry->tinfo.thread_id,NULL);
				//Close connection
				close(current_entry->tinfo.input_args.connected_skt_fd);
				syslog(LOG_DEBUG,"SOCKET Closed connection from %s",client_ip_hostview);	
			}
			current_entry->tinfo.input_args.thread_complete = 0;
		}
	}
	return 0;
}
