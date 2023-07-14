#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main (int argc, char **argv)
{

	openlog ("write-app", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

	if (argc <= 2) {
		printf(" missing arguments: %s [filepath] [str]\n", argv[0]);
		syslog(LOG_ERR, "missing arguments: %s [filepath] [str]\n", argv[0]);
		return 1;
	}


	char *filepath = argv[1];

	FILE *fptr = fopen(filepath, "w");
	

	char *str = argv[2];

	syslog(LOG_DEBUG, "Writing %s to %s\n", str, filepath);
	fprintf(fptr, "%s", str);

	fclose(fptr);

	
	closelog();	

	return 0;
}
