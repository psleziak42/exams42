#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/types.h>
// for structure sockaddr_in;
#include <netinet/in.h>



// delete later
#include <stdio.h>

#define MAX_CLIENTS 1024
#define MAX_MESSAGE 4096

typedef struct client {
	int id;
	int fd;
} client;


void _add(int fd, int *clients) {
	for (int i = 0 ; ; i++)
		if (clients[i] == 0) {
			clients[i] = fd;
			break ;
		}
}

int _find_id(int fd, int *clients) {
	int i;
	for(i = 0 ; clients[i] != 0 ; i++)
		if (clients[i] == fd)
			break ;
	return i;
}

// remove means to substitue with -1
// to tez moze przyniesc problemy, bo szybko moga sie wyczerpac miejsca dla klientow
void _remove(int fd, int *clients) {
	for (int i = 0 ; ; i++) {
		if (clients[i] != fd)
			continue ;
		close(clients[i]);
		clients[i] = -1;
		break ;
	}
}

// 0 will be always "last element", so once we have reach it we know there is nothing after it
// need to cover the case when there is only main socket left and all clients are gone;
int _max(int *clients) {
	int max = 3;
	for(int i = 0 ; clients[i] != 0 ; i++) {
		if (clients[i] > max)
			max = clients[i];
		printf("clients[%d]: %d\n max: %d\n\n", i, clients[i], max);
	}
	return max;
}


void _broadcast(int self, int *clients, char *msg) {
	for(int i = 0 ; clients[i] != 0 ; i++) {
		if (clients[i] == self || clients[i] == -1)
			continue;
		write(clients[i], msg, strlen(msg));
	}
}

void _close(int *clients) {
	for(int i = 0 ; clients[i] != 0 ; i++) {
		if (clients[i] == -1)
			continue;
		close(clients[i]);
	}
}

// int validate_port()


int main(int argc, char **argv)
{
	(void)argv;

	int main_socket;
	int port;
	int new_connection;
    struct sockaddr_in define_socket, client;


// for select
    int clients_array[MAX_CLIENTS] = {0};
    int nfds = 0;
    int select_fds = 0;
    int bytes_read = 0;
    char buffer_read[MAX_MESSAGE + 1] = {0};

    char buff_come_go[50];

    if (argc != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
    }
// QUESTION: do we have to check if port is valid? Can they pass "abc" as an arg or negative number or always valid number?
	port = atoi(argv[1]);

// sockets
    if ((main_socket=socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    	write(2, "Fatal error socket\n", strlen("Fatal error socket\n"));
    	exit(1);
	}

// zero the memory	
	memset(&define_socket, 0, sizeof(define_socket));


	define_socket.sin_family = AF_INET;

//QUESTION: how to do it without HTONL i HTONS and inet_addr(cant use them);
	define_socket.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	define_socket.sin_port = htons(port);

// to avoid waiting for bind //
	int opt = 1;
	if (setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
	    perror("setsockopt");
	    exit(EXIT_FAILURE);
	}
	
	if ( (bind(main_socket, (struct sockaddr*)&define_socket, sizeof(define_socket))) < 0) {
		write(2, "Fatal error bind\n", strlen("Fatal error bind\n"));
    	exit(1);
	}

	if ((listen(main_socket, 10)) < 0) {
		write(2, "Fatal error listen\n", strlen("Fatal error listen\n"));
    	exit(1);
	}

// get select ready
	fd_set read_fds;
	nfds = main_socket;
	FD_ZERO(&read_fds);
	FD_SET(main_socket, &read_fds);
	printf("Server listening on port %d...\n", port);
	while(1) {
		fd_set temp = read_fds;
		printf("nfds: %d\n", nfds);
		select_fds = select(nfds + 1, &temp, NULL, NULL, NULL);
		if (select_fds == -1) {
			write(2, "Fatal error select\n", strlen("Fatal error select\n"));
			_close(clients_array);
    		exit(1);
		}
		// logic for "big socket"
		// .determine if is new connection
		// .broadcast to everyone new conn arrived
		// .add it to monitored fds for select
		// .add it to my array to know who to inform about new connection
		if (FD_ISSET(main_socket, &temp)){
	// narazie nie widze sensu: (main_socket, struct sockaddr *)&define_socket, (socklen_t*)&define_socket)
			new_connection=accept(main_socket, (struct sockaddr *)&client, (socklen_t*)&define_socket);
			if (new_connection == -1) {
				write(2, "Fatal error new connection\n", strlen("Fatal error new connection\n"));
				//need to free all fds,
    			exit(1);
			}
			_add(new_connection, clients_array);
			FD_SET(new_connection, &read_fds);
			nfds = _max(clients_array);
			// broadcast to everyone that new guy arrived
			//buff_come_go_len = //
			//IMPORTANT: CLIENT ID IS UNIQUE!!! its not fd - 4;
			sprintf(buff_come_go, "server: client %d just arrived\n", _find_id(new_connection, clients_array));
			_broadcast(new_connection, clients_array, buff_come_go);
		}



		else {
	// dla kazdego innego przeczytaj co on tam wyslal.
			for (int i = 0 ; clients_array[i] != 0 ; i++) {
				bzero(&buffer_read, sizeof(buffer_read));
				bzero(&buff_come_go, strlen(buff_come_go));
				if (FD_ISSET(clients_array[i], &temp)) {
					bytes_read = read(clients_array[i], buffer_read, MAX_MESSAGE);
					printf("buffer_read: %d\n", bytes_read);
				}
				else
					continue ;
	// usun polaczenie
				if (bytes_read == 0) {
					//buff_come_go_len = //
					sprintf(buff_come_go, "server: client %d just left\n", _find_id(clients_array[i], clients_array));
					// close(clients_array[i]); - put it in remove;
					_broadcast(clients_array[i], clients_array, buff_come_go);
				//clr musi byc przed _remove, bo tam nadpisuje clients_array with -1;
					FD_CLR(clients_array[i], &read_fds);
					_remove(clients_array[i], clients_array);
					nfds = _max(clients_array);
				}
				else { // jak jest wiadomosc -> trzeba stworzyc bufer na wiadomosc malokiem chyba;
					//buff_come_go_len = //
					sprintf(buff_come_go, "client %d: ", _find_id(clients_array[i], clients_array));
					_broadcast(clients_array[i], clients_array, buff_come_go);
					_broadcast(clients_array[i], clients_array, buffer_read);
				}
			}
		}	
	}
	printf("hey?");
	return 0;
}
