/*
Rzeczy do zrobienia:
- obsługa błędu "brak pliku"
- pozamykać pliki i kolejki
*/

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_USERS 100
#define MAIN_PORT 1124

const int DEBUG = 1;
const char userFile[] = "users";

struct Message
{
	long type;
	char from[16];
	char to[16];
	char content[1024];
};

struct RegUser
{
	char username[16];
	char password[16];
	struct Message msgs[50];
	int msgN;
};

struct LogUser
{
	int free;
	char username[16];
	int fd;
};


class Server
{
	private:
		int myport;
		int fd_socket;
		struct sockaddr_in addr;
		pthread_t threads[MAX_USERS];

		int regUsers;
		int logUsers;
		RegUser userList[MAX_USERS];
		LogUser loggedList[MAX_USERS];


		int findExistingUser(char u[16])
		{
			for (int i = 0; i < MAX_USERS; i++)
			{
				if (strcmp(userList[i].username, u) == 0) 
					return i;
			}
			return -1;
		}

		int findLoggedUser(char u[16])
		{
			for (int i = 0; i < MAX_USERS; i++)
			{
				if (loggedList[i].free == 0 && strcmp(loggedList[i].username, u) == 0) 
					return i;
			}
			return -1;
		}

		char * getLoggedUsers()
		{
			// todo
			
			// return
		}

		void readOneWord(int fd, char readTo[16])
		{
			char temp;
			int i = 0;
			while ( read(fd, &temp, sizeof(char)) )
			{
				if ( temp != ' ' && temp != '\n')
					readTo[i] = temp;
				else break;
				i += 1;
			}
			return;
		}

		int loadUsers(const char * filename)
		{
			if (DEBUG) printf("Loading data about registered users...\n");
			int fd = open(filename, O_RDWR);
			if (DEBUG) printf("File succesfully opened...\n");
			int currentUser = 0;
			char username[16] = {0}, password[16] = {0};
			while (1)
			{
				readOneWord(fd, username);
				readOneWord(fd, password);
				if (username[0] != 0)
				{
					strncpy(userList[currentUser].username, username, 16);
					strncpy(userList[currentUser].password, password, 16);
					if (DEBUG) printf("Loaded user %i: %s\n", currentUser+1, username );
				}
				else 
				{
					if (DEBUG) printf("Reached EOF!\n");
					break;
				}
				memset(username, 0, 16);
				memset(password, 0, 16);
				currentUser += 1;
			}
			if (DEBUG) printf("Succesfully loaded %i users into memory.\n", currentUser);
			return currentUser;
		}

		int loginChecker(char username[16], char password[16])
		{
			for (int i = 0; i < this->regUsers; i++)
			{
				if ( strcmp(username, userList[i].username) == 0 &&
					strcmp(password, userList[i].password) == 0 )
					return 1;
			}
			return 0;
		}

		int checkLoginQueue(int fd)
		{
			// check for logins lmao
			int pfd = 0;
			loginHandler(pfd);

			// return
		}

		void loginHandler(int pfd)
		{
			// start child for handling the critical section (user input)
			if ( fork() == 0 )
			{
				
				// DO SOMEHTING HERE

				// kill the child process
				exit(0);
			}
			return;
		}

		void sendMessage(struct Message letter, int reciever, int sender)
		{
			int id = findLoggedUser(letter.to); 	// find reciever
			if ( id != -1 ) 						// reciever is online
			{
				// todo
			}
			else 									// reciever is offline
			{
				if ( userList[reciever].msgN < 50 ) // adding to a heap of undelivered messages...
				{
					// todo
				}
				else 								// heap is already full
				{
					// todo
				}
			}
			
			// todo: send confirmation back
			return;
		}

		void messageHandler(int userID, int type)
		{
			switch (type)
			{
				case 4: // asking for sending a message
					// todo
					break;

				case 5: // asking for list of online users
					// todo
					break;

				case 8: // send messages from heap to user
					// todo
					break;
			}
			return;
		}

		void checkForRequests()
		{
			for (int i = 0; i < MAX_USERS; i++)
			{
				if (loggedList[i].free == 0)
				{
					// todo
				}
			}
			return;
		}

		static void *handleThread(void *fd)
		{
			int fd1 = *((int *) fd);
			char buf[255];
			memset(&buf, 0, 255);

			// handling goes there
			read(fd1, &buf, sizeof(buf));
			printf("1: %s\n", buf);
			read(fd1, &buf, sizeof(buf));
			printf("2: %s\n", buf);

			close(fd1);
			if (DEBUG) printf("Exiting thread... (fd: %d)\n", fd1);
			pthread_exit(NULL);
		}

		void mainLoop()
		{
			socklen_t sa_size;
			struct sockaddr_in sa;
			
			int i = 0; //narazie prowizorka z tablica watkow
			while (1)
			{
				sa_size = sizeof(sa);
				//memset(&sa, 0, sizeof(sa));
				int client = accept(fd_socket, (struct sockaddr *)&sa, &sa_size);
				if (client < 0)
				{
					printf("Error occured on accepting connection!\n");
					perror("accept");
				}
				else
				{
					if (DEBUG) printf("Incoming connection from %s...\n", inet_ntoa(sa.sin_addr));
					int cr = pthread_create(&threads[i], NULL, handleThread, (void *)&client);
					if (cr != 0)
					{
						if (DEBUG) printf("ERROR - thread creation failed!\n");
						// todo odrzuć połączenie
					}
					else if (DEBUG) printf("New thread created! (fd: %d)\n", client);
					i++;
				}


			}
		}

	public:
		
		Server(int port, const char * filename)
		{
			for (int i = 0; i < MAX_USERS; i++)
			{
				loggedList[i].free = 1;
				userList[i].msgN = 0;
			}
			//loadUsers(filename);
			myport = port;
		}

		void start()
		{
			if (DEBUG) printf("Starting services...\n");

			fd_socket = socket(AF_INET, SOCK_STREAM, 0);
			
			if (fd_socket < 0)
			{
				printf("ERROR - socket creation unsuccesful.\n");
				exit(0);
			}
			else if (DEBUG) printf("Socket created...\n");

			memset(&addr, 0, sizeof(addr));

			addr.sin_family = AF_INET;
			addr.sin_port = htons(myport);
			addr.sin_addr.s_addr = htonl(INADDR_ANY);

			int nFoo = 1;
			setsockopt(fd_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nFoo, sizeof(nFoo));

			if (bind(fd_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
			{
				printf("ERROR - bind failed.\n");
				exit(0);
			}
			else if (DEBUG) printf("Bind succesfull...\n");

			if (listen(fd_socket, 10) < 0)
			{
				printf("ERROR - listen failed.\n");
				exit(0);
			}
			else if (DEBUG) printf("Listening set up.\n");
			
			if (DEBUG) printf("Server is now fully operational! I'm all ears.\n");
			mainLoop();
		}
};


int main()
{
	Server server = Server(MAIN_PORT, userFile);
	server.start();
	return 0;
}