#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>

#include "direction.h"
#include "parse_opt.h"

void trunccopy(char *dst, int dstmaxlen, char *orig, int origlen){
	if (origlen < dstmaxlen)
		strcpy(dst, orig);
	else{
		strncpy(dst, orig, dstmaxlen-1);
		dst[dstmaxlen-1] = '\0';
	}	
}

void set_default_opt(program_options_t *options){
	memset(options, 0, sizeof(*options));
	options->time = DEFAULT_TIME;
	options->mtu = DEFAULT_MTU;
	options->direction = DEFAULT_DIRECTION;
	strcpy(options->port, DEFAULT_PORT);
}

void print_help(){
	printf("This is a bandwidth test client compatible with Mikrotik RouterOS.\n\n");
	printf("Usage: mikrotik_btest [options] HOST[:PORT]\n\n");
	printf("Options:\n");
	printf("\t-h, --help \t\t\t\t show this help message and exit.\n");
	printf("\t-t DURATION, --time=DURATION \t\t test duration in seconds. Default 8 seconds.\n");
	printf("\t-m MTU, --mtu=MTU \t\t\t MTU in bytes for the test. Default 1500 bytes.\n");
	printf("\t-d DIRECTION, --direction=DIRECTION \t Test direction (receive, send, both). Default receive.\n");
	printf("\t-u USER, --user=USER\n");
	printf("\t-p PASSWORD, --password=PASSWORD\n");
}

int parse_opt(int *argc, char **argv, program_options_t *options){
	int c, optarglen, option_index;
	char * pch;

	static struct option long_options[] = {
	        {"time",  required_argument, 0, 't'},
	        {"mtu",  required_argument, 0, 'm'},
	        {"direction",  required_argument, 0, 'd'},
	        {"user",    required_argument, 0, 'u'},
	        {"password",    required_argument, 0, 'p'},
	        {"help",   no_argument, 0, 'h'},
	        {0, 0, 0, 0}};

	set_default_opt(options);

	while ((c = getopt_long(*argc, argv, "p:t:m:d:u:a:h", long_options, &option_index)) != -1) {
		if (optarg)
			optarglen = strlen(optarg);
		switch(c){
			case 't':
				options->time = atoi(optarg);
				break;
			case 'm':
				options->mtu = atoi(optarg);
				break;
			case 'd':
				trunccopy(options->direction_string, sizeof(options->direction_string), optarg, optarglen);
				break;
			case 'u':
				trunccopy(options->user, sizeof(options->user), optarg, optarglen);
				break;
			case 'p':
				trunccopy(options->password, sizeof(options->password), optarg, optarglen);
				break;
			case 'h':
				print_help();
				return 1;
				break;
			default:
				return -1;
		}
	}
	if (*argc - optind == 1){
		if ((pch = strtok(argv[optind], ":")) != NULL){
			trunccopy(options->host, sizeof(options->host), pch, strlen(pch));
			if((pch = strtok(NULL, ":")) != NULL)
				trunccopy(options->port, sizeof(options->port), pch, strlen(pch));
		}
		else
			return -1;
	}
	else
		return -1;
	return 0;
}

int check_opt(program_options_t *options){
	int port;

	port = atoi(options->port);
	if (port < 1 || port >= 65536){
		fprintf(stderr, "Port %s is not valid\n", options->port);
		return -1;
	}

	if (strlen(options->direction_string) > 0){
		if (strcmp(options->direction_string, "receive") == 0){
			options->direction = RECEIVE;
		}
		else if (strcmp(options->direction_string, "send") == 0){
			options->direction = SEND;
		}
		else if (strcmp(options->direction_string, "both") == 0){
			options->direction = BOTH;
		}
		else{
			fprintf(stderr, "Direction %s is not valid\n", options->direction_string);
			return -1;
		}
	}

	if (options->mtu == 0){
		fprintf(stderr, "MTU is not valid\n");
		return -1;
	}

	if (options->time == 0.0){
		fprintf(stderr, "Time is not valid\n");
		return -1;
	}
	
	return 0;
}

