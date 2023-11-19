
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>
#include <sys/time.h>

//#define DEBUG
#ifdef DEBUG
#define dprintf(...) printf(__VA_ARGS__)
#define dbg_print(...) printf(__VA_ARGS__)
#else
#define dprintf(...) syslog(LOG_INFO, __VA_ARGS__)
#define dbg_print(...) 
#endif


#define BUFFER_SIZE 40000

#define USE_AESD_CHAR_DEVICE

#ifdef USE_AESD_CHAR_DEVICE
#define FILENAME "/dev/aesdchar"
#else
#define FILENAME "/var/tmp/aesdsocketdata"
#endif


int fd;
pthread_mutex_t write_mutex;


struct slist_data {
	pthread_t id;
	int newfd;
	bool finished;
	SLIST_ENTRY(slist_data) entries;

};

SLIST_HEAD(slist_pth, slist_data) head;

void timer_callback();

struct slist_data *insert_list(struct slist_pth* head, int newfd)
{
	struct slist_data *new_node = (struct slist_data *)malloc(sizeof(struct slist_data));
	
	printf("Alloced new_node=%p\n", new_node);
	new_node->newfd = newfd;
	new_node->finished = false;
	SLIST_INSERT_HEAD(head, new_node, entries);
	return new_node;
}



void destroy_element(struct slist_pth* head, pthread_t id) 
{
	struct slist_data *item, *tmp_item;
	int found = false;

	SLIST_FOREACH(item, head, entries) 
	{
	
		if(item->id == id)
		{
			found = true;
			break;
		}
	
	};

	if (found == true)
	{
		SLIST_REMOVE(head, item, slist_data, entries);
		printf("free item=%p\n", item);
		free(item);
	}
	
   
}

#if 0
void print_list(struct slist_pth* head)
{
	struct slist_data* ptr;

	SLIST_FOREACH(ptr, head, entries)
        {
                printf("val: %d\n", ptr->val);
        }

}
#endif

void sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM) {
		dprintf("Caught signal, exiting\n");
		closelog();
		close(fd);
		//remove(FILENAME);
		
	}
}


void* timer_thread(void* arg)
{
	sleep(2);
	while (1)
	{
		sleep(10);
		timer_callback();
	}

}

void* thread_func(void* arg)
{
	int i = 0, rc;
	unsigned char *buffer;
	bool newline_in_packet = false;
	int off_buff = 0;
	struct stat st;
	long filesize;
	unsigned char *rbuff;
	int filefd;
	
	struct slist_data *item = (struct slist_data *)arg;
	
	filefd = open(FILENAME, O_RDWR | O_APPEND | O_CREAT, 0x777);
	if (filefd < 0) {
		perror("open failed\n");
		return NULL;
	}
	
	rc = pthread_mutex_lock(&write_mutex);
	if (rc != 0)
	{
		printf("mutex lock failed!");
		return NULL;
	}
	
	buffer = (unsigned char *)malloc(BUFFER_SIZE);
	if (!buffer) {
		printf("allocation failed\n");
		pthread_mutex_unlock(&write_mutex);
		close(filefd);
		return NULL;
	}

	while (!newline_in_packet) {
		int rc = recv(item->newfd, buffer + off_buff, BUFFER_SIZE, 0);
		if (rc < 0) {
			perror("recv failed\n");
			free(buffer);
			pthread_mutex_unlock(&write_mutex);
			close(filefd);
			return NULL;
		}
		
		dbg_print("got %d bytes\n", rc);
		
	
		for (i = 0; i < rc; i++) {
			dbg_print("%c", buffer[off_buff + i]);
		}
	
		
		off_buff += rc;
		
		if (buffer[off_buff - 1] == '\n') {
			newline_in_packet = true;
		}
		
	
	}
	
	
	rc = write(filefd, buffer, off_buff);
	if (rc < 0) {
		perror("write failed\n");
		free(buffer);
		pthread_mutex_unlock(&write_mutex);
		close(filefd);
		return NULL;
	}

	printf("writing buffer:%s\n", buffer);

	
	free(buffer);


//	stat(FILENAME, &st);
//	filesize = st.st_size;
//	printf("filesize:%ld\n", filesize);
	rbuff = (unsigned char *)malloc(131072);
	if (!rbuff) {
		pthread_mutex_unlock(&write_mutex);
		return NULL;
	}

	rc = lseek(filefd, 0, SEEK_SET);
	if (rc < 0) {
		perror("lseek failed");
		free(rbuff);
		pthread_mutex_unlock(&write_mutex);
		close(filefd);
		return NULL;
	}

	rc = 0;
	int sum = 0;

        while (1)
	{

		rc = read(filefd, rbuff + sum, 131072);
		
		if (rc >= 131072 || rc == 0)
		      break;	

		if (rc < 0) {
			perror("read failed\n");
			free(rbuff);
			pthread_mutex_unlock(&write_mutex);
			close(filefd);
			return NULL;
		}

		sum += rc;


        }
	
      	printf("rc=%d\n", rc);

	int len = strlen(rbuff);

	printf("rbuff: len=%d\n", len);	
	for (i = 0; i < len; i++) {
		printf("%c", rbuff[i]);
		
	}


	rc = send(item->newfd, rbuff, len, 0);
	if (rc < 0) {
		perror("send failed\n");
		free(rbuff);
		pthread_mutex_unlock(&write_mutex);
		close(filefd);
		return NULL;
	}
	
	rc = pthread_mutex_unlock(&write_mutex);
	if (rc != 0)
	{
		printf("mutex unlock failed!");
		close(filefd);
		return NULL;
	}
	
	item->finished = true;
	free(rbuff);
	close(filefd);
}

#define Size 50

void timer_callback(void)
{
	int rc;
	time_t t ;
	struct tm *tmp ;
	char MY_TIME[Size];
	char line[Size + 2];
	int filefd;
	
	time( &t );
	
	filefd = open(FILENAME, O_RDWR | O_APPEND | O_CREAT, 0x777);
	if (filefd < 0) {
		perror("open failed\n");
		return;
	}

	//localtime() uses the time pointed by t ,
	// to fill a tm structure with the 
	// values that represent the 
	// corresponding local time.
	
	printf("timer_callback called !\n");

	tmp = localtime( &t );

	// using strftime to display time
	strftime(MY_TIME, sizeof(MY_TIME), "timestamp:%Y %m %e %H %M %S", tmp);
	sprintf(line,"%s\n", MY_TIME);

	//printf("timestamp:%s\n", MY_TIME );
	
	rc = pthread_mutex_lock(&write_mutex);
	if (rc != 0)
	{
		printf("mutex lock failed from timer!");
		return;
	}
	rc = write(filefd, line, strlen(line));
	if (rc < 0) {
		perror("write failed\n");
		close(filefd);
		return;
	}

	rc = pthread_mutex_unlock(&write_mutex);
	if (rc != 0)
	{
		printf("mutex unlock failed from timer!");
		close(filefd);
		return;
	}
	
	close(filefd);
}
  
#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN
int main(int argc, char **argv)
{
	int rc, newfd, nready;
	struct sockaddr_in serv_addr, cli_addr;
	char cli_address_str[10];
	socklen_t peer_addr_size = 0;
	struct sigaction new_action;
	bool isdaemon = false;
	pid_t pid;
	fd_set rset; 
	int maxfdp1;
	timer_t            timerid;
	sigset_t           mask;
	struct sigevent    sev;
	struct sigaction   sa;
	struct itimerspec  its;


	if (argc > 1) {
		if (!strcmp(argv[1], "-d")) {
			isdaemon = true;
		}
	}

	if (isdaemon) {
		pid = fork();
		if (pid < -1) {
			printf("fork failed.. exiting ..");
			exit(-1);
		} else if (pid > 0) { // parent
			// Let the parent terminate
			exit(0);
		}
	}


	if (pthread_mutex_init(&write_mutex, NULL) != 0) { 
		printf("\n mutex init has failed\n"); 
		return -1; 
	} 

    
	memset(&new_action, 0, sizeof(struct sigaction));
	
	new_action.sa_handler = sig_handler;
	
  	new_action.sa_flags = 0;
  	if (sigaction (SIGINT, &new_action, NULL) != 0) {
  		perror("sigaction SIGINT failed\n");
  		return -1;
  	}
  	
  	if (sigaction (SIGTERM, &new_action, NULL) != 0) {
  		perror("sigaction SIGTERM failed\n");
  		return -1;
  	}

	
	pthread_t id;

#ifndef USE_AESD_CHAR_DEVICE	
	rc = pthread_create(&id, NULL, timer_thread, NULL);
	if (rc != 0)
	{
		printf("pthread_create trimer_thread failed\n");
		return -1;
	}
 
#endif

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		perror("socket failed\n");
		
		return -1;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(9000);

	rc = bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	if (rc < 0) {
		perror("bind failed\n");
		close(fd);
		return -1;
	}
	

	rc = listen(fd, 10);

	if (rc < 0) {
		perror("Listen failed\n");
		close(fd);
		return -1;
	}

	
	// clear the descriptor set 
	
	
	struct timeval timeout;
	timeout.tv_sec = 3; 
    	timeout.tv_usec = 0; 
	
	while (1) {
	
		FD_ZERO(&rset); 
		maxfdp1 = fd + 1; 
		FD_SET(fd, &rset);
		
		// select the ready descriptor 
		nready = select(maxfdp1, &rset, NULL, NULL, &timeout); 
		
		if (nready < 0)
		{
			if (errno == EINTR)
				continue;
			printf("select failed\n");
			return -1;
		}
		else 
		{
			if (FD_ISSET(fd, &rset)) 
			{ 
				int cont_accept = 1;
				
				while (cont_accept)
				{
					newfd = accept(fd, (struct sockaddr *)&cli_addr, &peer_addr_size);

					if (newfd < 0) 
					{

						if (errno != EINTR)
						{
							perror("accept failed\n");
							close(fd);
							return -1;
						}
					}
					else
					{
						cont_accept = 0;
					}
				}

				openlog("server", LOG_PID, LOG_USER);

				inet_ntop(AF_INET, &cli_addr.sin_addr, cli_address_str, sizeof(cli_address_str));
				printf("Accepted connection from %s\n", cli_address_str);

				struct slist_data *new_item = insert_list(&head, newfd);
				
				pthread_create(&new_item->id, NULL, thread_func, (void *)new_item);
				
				printf("new_item->id=%ld\n", new_item->id);
				
				

				
			}
			
			struct slist_data *iter;
			SLIST_FOREACH(iter, &head, entries) 
			{
			
				if (iter->finished == true)
				{
					pthread_join(iter->id, NULL);
					
					destroy_element(&head, iter->id); 

					break;
					
				}
			
			};
		}
		
		
		
	}


	closelog();
	close(fd);

return 0;




}
