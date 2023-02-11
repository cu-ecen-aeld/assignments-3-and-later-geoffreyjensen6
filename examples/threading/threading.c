#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int mutex_ret;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    syslog(LOG_INFO,"Thread: Started thread");

    // Wait
    syslog(LOG_INFO,"Thread: Started sleeping for %i",thread_func_args->sleep_before_lock);
    usleep((thread_func_args->sleep_before_lock) * 1000);
    syslog(LOG_INFO,"Thread: Finished sleeping for %i",thread_func_args->sleep_before_lock);

    // Obtain Mutex
    syslog(LOG_INFO,"Thread: Obtaining Mutex Lock");
    mutex_ret = pthread_mutex_lock(thread_func_args->mutex);
    syslog(LOG_INFO,"Mutex Lock Returned: %i",mutex_ret);
    if(mutex_ret != 0){
	perror("Mutex Lock failed, returned: ");
	thread_func_args->thread_complete_success = false;
    }
    // Wait
    syslog(LOG_INFO,"Thread: Started sleeping for %i",thread_func_args->sleep_after_lock);
    usleep((thread_func_args->sleep_after_lock) * 1000);
    syslog(LOG_INFO,"Thread: Finished sleeping for %i",thread_func_args->sleep_before_lock);

    // Release Mutex
    mutex_ret = pthread_mutex_unlock(thread_func_args->mutex);
    syslog(LOG_INFO,"Thread: Released Mutex Lock");
    if(mutex_ret != 0){
	perror("Mutex Unlock failed, returned: ");
	thread_func_args->thread_complete_success = false;
    }
    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    int thread_ret;
    struct thread_data* thread_param;
    thread_param = malloc(sizeof(struct thread_data));
    thread_param->thread = malloc(sizeof(pthread_t));
    thread_param->mutex = malloc(sizeof(pthread_mutex_t));

    thread_param->thread = thread;
    thread_param->mutex = mutex;
    thread_param->sleep_before_lock = wait_to_obtain_ms;
    thread_param->sleep_after_lock = wait_to_release_ms;

    openlog(NULL,0,LOG_USER);

    thread_ret = pthread_create(thread_param->thread,NULL,threadfunc,thread_param);
    if(thread_ret == 0){
	    return true;
    }
    perror("Thread Failure, returned: ");
    free(thread_param->thread);
    free(thread_param->mutex);
    return false;

}

