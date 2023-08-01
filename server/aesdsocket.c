
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
#include<signal.h>

#define DEBUG
#ifdef DEBUG
#define dprintf(...) printf(__VA_ARGS__)
#define dbg_print(...) printf(__VA_ARGS__)
#else
#define dprintf(...) syslog(LOG_INFO, __VA_ARGS__)
#define dbg_print(...) 
#endif


#define BUFFER_SIZE 40000
#define FILENAME "/var/tmp/aesdsocketdata" 

int filefd;
int fd;

void sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM) {
		dprintf("Caught signal, exiting\n");
		close(filefd);
		closelog();
		close(fd);
		remove(FILENAME);
		
	}
}


int main(int argc, char **argv)
{
	int rc, newfd;
	struct sockaddr_in serv_addr, cli_addr;
	char cli_address_str[10];
	unsigned char *buffer, *rbuff;
	struct stat st;
	long filesize;
	socklen_t peer_addr_size;
	struct sigaction new_action;
	bool isdaemon = false;
	pid_t pid;
	
	printf("argc=%d\n", argc);
	if (argc > 1) {
		if (!strcmp(argv[1], "-d")) {
			isdaemon = true;
		}
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

	filefd = open(FILENAME, O_RDWR | O_APPEND | O_CREAT, 0x777);
	if (filefd < 0) {
		perror("open failed\n");
		return -1;
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		perror("socket failed\n");
		close(filefd);
		return -1;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(9000);

	rc = bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	if (rc < 0) {
		perror("bind failed\n");
		close(filefd);
		close(fd);
		return -1;
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

	rc = listen(fd, 10);

	if (rc < 0) {
		perror("Listen failed\n");
		close(filefd);
		close(fd);
		return -1;
	}

	while (1) {
		newfd = accept(fd, (struct sockaddr *)&cli_addr, &peer_addr_size);

		if (newfd < 0) {
			perror("accept failed\n");
			close(fd);
			return -1;
		}

		openlog("server", LOG_PID, LOG_USER);

		inet_ntop(AF_INET, &cli_addr.sin_addr, cli_address_str, sizeof(cli_address_str));
		dprintf("Accepted connection from %s\n", cli_address_str);

		buffer = (unsigned char *)malloc(BUFFER_SIZE);
		if (!buffer) {
			printf("allocation failed\n");
			close(filefd);
			close(fd);
			return -1;
		}

		int i = 0;
		bool newline_in_packet = false;
		int off_buff = 0;
		
		while (!newline_in_packet) {
			int rc = recv(newfd, buffer + off_buff, BUFFER_SIZE, 0);
			if (rc < 0) {
				perror("recv failed\n");
				close(filefd);
				close(fd);
				return -1;
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
			close(filefd);
			close(fd);
			return -1;
		}


		stat(FILENAME, &st);
		filesize = st.st_size;
		dbg_print("filesize:%ld\n", filesize);
		rbuff = (unsigned char *)malloc(filesize);
		if (!rbuff) {
			close(filefd);
			close(fd);
			return -1;
		}

		rc = lseek(filefd, 0, SEEK_SET);
		if (rc < 0) {
			perror("lseek failed");
			close(filefd);
			close(fd);
			return -1;
		}
		
		rc = read(filefd, rbuff, filesize);
		if (rc < 0) {
			perror("read failed\n");
			close(filefd);
			close(fd);
			return -1;
		}
		
		for (i = 0; i < filesize; i++) {
			dbg_print("%c", rbuff[i]);
			
		}


		rc = send(newfd, rbuff, filesize, 0);
		if (rc < 0) {
			perror("send failed\n");
			close(filefd);
			close(fd);
			return -1;
		}

		dprintf("Closed connection from %s\n", cli_address_str);
	}



	close(filefd);
	closelog();
	close(fd);

return 0;




}
