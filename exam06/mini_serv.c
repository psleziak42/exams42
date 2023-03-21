#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/select.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


// FEW TIPS
// main.c is given one the exam, you can copy those functions, dont need to change them. Just remember about correc error messages on socket, accept,
// bind and listen.
// 
// gcc -Wall -Werror -Wextra -fsanitize=address mini_serv.c -o mini_serv
//
// To check open fds:
// run the program, open new terminal and run: lsof -c mini_serv. It will show you open file descriptors.

// nc 127.0.0.1 8000
// net cat <ip> <port> is used to connect and talk to the server. Thyen you can open many terminals, connect few clients and send message to see if program works correctly.
// REMEMBER!!! They will test a message that looks like this: "\n\nhello\n\nworld\n\n\", something like this:
// client <id> joined server
// client 0:
// client 0:
// client 0: hello
// client 0:
// client 0: world
// client 0: 
// If there is not \n in the input then you dont display anything!!!
// 
// To test it yourself prepare files and feed them to nc: nc 127.0.0.1 8000 < file.txt


int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

u_int16_t _htons(u_int16_t port) {
    u_int16_t result = 0;

    result = result | port << 8;
    result = result | port >> 8;

    return result; 
}


void ft_exit(int socketfd, int *client_fd, char **extract) {
    for (int i = 0 ; client_fd[i] != 0 ; i++) {
        if (client_fd[i] == -1)
            continue ;
        close(client_fd[i]);
        free(extract[client_fd[i]]);
    }
// have to also close socket file descriptor
    close(socketfd);
    exit (1);
}

void add_fd(int fd, int *client) {
    for (int i = 0 ; ; i++) {
        if (client[i] == 0 || client[i] == -1) {
            client[i] = fd;
            break ;
        }
    }
}

void add_id(int fd, int id, int*client_fd, int *client_id) {
    int i = 0;
    for(; ; i++) {
        if (client_fd[i] == fd)
            break ;
    }
    client_id[i] = id;
}

void _remove(int fd_or_id, int *client_or_id) {
    for (int i = 0 ; client_or_id[i] != 0 ; i++) {
        if (client_or_id[i] == fd_or_id) {
            client_or_id[i] = -1;
            break ;
        }
    }
}

int max(int *client) {
    int fd = 3;
    for (int i = 0 ; client[i] != '\0' ; i++) {
        if (fd > client[i])
            continue ;
        fd = client[i];
    }
    return fd;
}

void broadcast(int fd, int *client, char *msg) {
    for (int i = 0 ; client[i] != 0 ; i++) {
        if (client[i] == fd || client[i] == -1)
            continue ;
        write(client[i], msg, strlen(msg));
    }
}


int main(int argc, char **argv) {

// socket
    int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli; 

// select
    fd_set readfds, copyfds;
    int nfds;

// other
    int port;
    int id = 0;
    int read_bytes = 0;
    int client_fd[65000] = {0};
    int client_id[65000] = {0};

    char _small[50] = {'\0'};
    char _buffer[4097] = {'\0'};
    char *_extract[65000] = { NULL };
    char *_to_print = NULL;


    if (argc != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    port = atoi(argv[1]);
// There is no need to check the port
    // if (port < 0) {
    //     write(2, "Fatal error\n", strlen("Fatal error\n"));
    //     exit(1);
    // }

    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		write(2, "Fatal error\n", strlen("Fatal error\n"));
        exit(1);
	} 

    bzero(&servaddr, sizeof(servaddr)); 

	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); // 0b00000001000000000000000001111111 - this is 127.0.0.1 in netework order
// you dont need to write _htons, its allowed on the exam, same as htonl
	servaddr.sin_port = _htons(port);

    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
	    write(2, "Fatal error\n", strlen("Fatal error\n"));
        exit(1);
	}

    if (listen(sockfd, 10) != 0) {
	    write(2, "Fatal error\n", strlen("Fatal error\n"));
        exit(1);
	}

// prepare select
    FD_ZERO(&readfds);
    FD_ZERO(&copyfds);
    FD_SET(sockfd, &readfds);
    nfds = sockfd;

    while (1) {
        copyfds = readfds;
        if (select(nfds + 1, &copyfds, NULL, NULL, NULL) < 0)
            ft_exit(sockfd, client_fd, _extract);
        if (FD_ISSET(sockfd, &copyfds)) {
            len = sizeof(cli);
	        connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);
	        if (connfd < 0)
                ft_exit(sockfd, client_fd, _extract);
            FD_SET(connfd, &readfds);
        // client_fd i client_id powinny miec te same wartosci -1 i 0 natych samcych pozycjach;
            add_fd(connfd, client_fd);
            add_id(connfd, id, client_fd, client_id);
            sprintf(_small, "server: client %d just arrived\n", id);
            broadcast(connfd, client_fd, _small);
        // id is updated here becasue its only after new client is added. 1st client has id = 0, the next will have 1 etc.
            id += 1;
            nfds = max(client_fd);
        }

        for(int i = 0 ; client_fd[i] != 0 ; i++) {
            if (!FD_ISSET(client_fd[i], &copyfds))
                continue ;
            read_bytes = read(client_fd[i], &_buffer, 4096);
            if (read_bytes == 0) {
                FD_CLR(client_fd[i], &readfds);
                sprintf(_small, "server: client %d just left\n", client_id[i]);
                broadcast(client_fd[i], client_fd, _small);
                free(_extract[client_fd[i]]);
                _extract[client_fd[i]] = NULL;
                close(client_fd[i]);
        // remove must go almost the last, because if you remove client_fd and then later try to close it you will close -1, FD_CLR is not going to work
                _remove(client_id[i], client_id);
                _remove(client_fd[i], client_fd);
        // nfds must go after remove because we need to find max avaliable fd. We removed one so we will check if it was not the biggest.
                nfds = max(client_fd);
            }
            else {
                _buffer[read_bytes] = '\0';
                _extract[client_fd[i]] = str_join(_extract[client_fd[i]], _buffer);
                while(extract_message(&_extract[client_fd[i]], &_to_print)) {
                    sprintf(_small, "client %d: ", client_id[i]);
                    broadcast(client_fd[i], client_fd, _small);
                    broadcast(client_fd[i], client_fd, _to_print);
                    free(_to_print);
                    _to_print = NULL;
                }
            }
        }
    }
}

