/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#include "tcptest.h"
#include "tcptest_thread.h"
#include "direction.h"
#include "messages.h"
#include "md5.h"
#include "return_codes.h"

int recv_msg(int sockfd, unsigned char *buf, int bufsize, unsigned char *msg, int *recvbytes){
	if ((*recvbytes = recv(sockfd, buf, bufsize, 0)) == -1) {
		perror("recv");
		return RETURN_ERROR;
	}
	if ((*recvbytes == sizeof(MSG_OK)) && (memcmp(buf, msg, *recvbytes) == 0)){
		return RETURN_OK;
	}
	return RETURN_MSG_MISMATCH;
}

int send_msg(int sockfd, unsigned char *msg, int len){
	int bytes_sent = 0, failed = 0, bytes;
	
	do{
		if ((bytes = send(sockfd, msg, len, 0)) == -1){
			++failed;
		}
		else
			failed = 0;

		if (failed >= MAX_RETRY){
			perror("send");
			return RETURN_ERROR;
		}
		bytes_sent += bytes;
	}while(bytes_sent < len);
	return RETURN_OK;
}

void craft_response(char *user, char *password, unsigned char *challenge, unsigned char *response){
	md5_state_t lvl1_state, lvl2_state;
	md5_byte_t lvl1_digest[16], lvl2_digest[16];
	int len = strlen(password);

	md5_init(&lvl2_state);
	if (len > 0)
		md5_append(&lvl2_state, (const md5_byte_t *) password, len);
	md5_append(&lvl2_state, (const md5_byte_t *) challenge, CHALLENGE_SIZE);
	md5_finish(&lvl2_state, lvl2_digest);

	md5_init(&lvl1_state);
	if (len > 0)
		md5_append(&lvl1_state, (const md5_byte_t *) password, len);
	md5_append(&lvl1_state, (const md5_byte_t *) lvl2_digest, sizeof(lvl2_digest));
	md5_finish(&lvl1_state, lvl1_digest);

	memcpy(response, lvl1_digest, sizeof(lvl1_digest));
	strncpy((char *) response+sizeof(lvl1_digest), user, RESPONSE_SIZE-sizeof(lvl1_digest));
}

int open_socket(char *host, char *port){
	struct addrinfo hints, *servinfo, *p;
	int rv, sockfd=-1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
			fprintf(stderr, "Invalid host: %s\n", gai_strerror(rv));
			return RETURN_ERROR;
	}

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("connect");
			continue;
		}
		break;
	}
	freeaddrinfo(servinfo);

	if (p == NULL) {
		fprintf(stderr, "failed to connect\n");
		if (sockfd != -1)
			close(sockfd);
		return RETURN_ERROR;
	}
	return sockfd;
}

int init_test(int sockfd, char *user, char *password,  direction_t direction, int mtu){
	unsigned char *buffer, challenge[CHALLENGE_SIZE], response[RESPONSE_SIZE];
	int numbytes, rv=0;

	buffer = (unsigned char *) malloc(mtu);

	if (recv_msg(sockfd, buffer, mtu, MSG_OK, &numbytes) != 0){
		close(sockfd);
		free(buffer);
		return RETURN_ERROR;
	}

	switch(direction){
		case RECEIVE:
			rv = send_msg(sockfd, MSG_TCP_DOWN, sizeof(MSG_TCP_DOWN));
			break;
		case SEND:
			rv = send_msg(sockfd, MSG_TCP_UP, sizeof(MSG_TCP_UP));
			break;
		case BOTH:
			rv = send_msg(sockfd, MSG_TCP_BOTH, sizeof(MSG_TCP_BOTH));
			break;
	}
	if (rv != 0){
		close(sockfd);
		free(buffer);
		return RETURN_ERROR;
	}

	rv = recv_msg(sockfd, buffer, mtu, MSG_OK, &numbytes);
	if (rv == RETURN_OK){
		free(buffer);
		return RETURN_OK;
	}
	else if (rv == RETURN_MSG_MISMATCH){
		if (numbytes == CHALLENGE_TOTAL_SIZE && memcmp(buffer, CHALLENGE_HEADER, sizeof(CHALLENGE_HEADER)) == 0){
			memcpy(challenge, buffer+sizeof(CHALLENGE_HEADER), CHALLENGE_SIZE);
			craft_response(user, password, challenge, response);
			if (send_msg(sockfd, response, sizeof(response)) == 0){
				if (recv_msg(sockfd, buffer, mtu, MSG_OK, &numbytes) == 0){
					free(buffer);
					return RETURN_OK;
				}
			}
		}
	}
	fprintf(stderr, "Auth failed\n");
	close(sockfd);
	free(buffer);
	return RETURN_ERROR;
}

int tcptest(char *host, char *port, char *user, char *password, direction_t direction, int mtu, int time){
	int sockfd, elapsed_seconds = 0;
	double mbps;
	pthread_t threads[2];
	thread_args_t threads_arg[2];
	pthread_mutex_t mutexes[2];
	pthread_attr_t attr;

	if ((sockfd = open_socket(host, port)) == RETURN_ERROR)
		return RETURN_ERROR;

	if (init_test(sockfd, user, password, direction, mtu) == RETURN_ERROR){
		close(sockfd);
		return RETURN_ERROR;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_mutex_init(&mutexes[RECEIVE], NULL);
	pthread_mutex_init(&mutexes[SEND], NULL);

	if (direction == BOTH){
		init_thread_args(&threads_arg[RECEIVE], sockfd, &mutexes[RECEIVE], mtu, RECEIVE);
		pthread_create(&threads[RECEIVE], &attr, tcptest_thread, (void *) &threads_arg[RECEIVE]);
		init_thread_args(&threads_arg[SEND], sockfd, &mutexes[SEND], mtu, SEND);
		pthread_create(&threads[SEND], &attr, tcptest_thread, (void *) &threads_arg[SEND]);
	}
	else{
		init_thread_args(&threads_arg[direction], sockfd, &mutexes[direction], mtu, direction);
		pthread_create(&threads[direction], NULL, tcptest_thread, (void *) &threads_arg[direction]);
	}

	do{
		sleep(1);
		elapsed_seconds += 1;
		if (direction == RECEIVE || direction == BOTH){
			pthread_mutex_lock(&mutexes[RECEIVE]);
			mbps = threads_arg[RECEIVE].mbps;
			pthread_mutex_unlock(&mutexes[RECEIVE]);
			printf("Rx: %7.2f Mb/s", mbps);
		}
		if (direction == BOTH)
			printf("\t");
		if (direction == SEND || direction == BOTH){
			pthread_mutex_lock(&mutexes[SEND]);
			mbps = threads_arg[SEND].mbps;
			pthread_mutex_unlock(&mutexes[SEND]);
			printf("Tx: %7.2f Mb/s", mbps);
		}
		printf("\r");
		fflush(stdout);
	}while (elapsed_seconds <= time);

	pthread_mutex_lock(&mutexes[RECEIVE]);
	threads_arg[RECEIVE].stop = TRUE;
	pthread_mutex_unlock(&mutexes[RECEIVE]);

	pthread_mutex_lock(&mutexes[SEND]);
	threads_arg[SEND].stop = TRUE;
	pthread_mutex_unlock(&mutexes[SEND]);

	if (direction == RECEIVE || direction == BOTH){
		pthread_join(threads[RECEIVE], NULL);
		printf("Rx: %7.2f Mb/s", threads_arg[RECEIVE].mbps);
	}
	if (direction == BOTH)
			printf("\t");
	if (direction == SEND || direction == BOTH){
		pthread_join(threads[SEND], NULL);
		printf("Tx: %7.2f Mb/s", threads_arg[SEND].mbps);
	}
	printf("\n");

	pthread_mutex_destroy(&mutexes[RECEIVE]);
	pthread_mutex_destroy(&mutexes[SEND]);
	close(sockfd);
	return RETURN_OK;
}
