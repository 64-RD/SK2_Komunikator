/*
I HAVE NO IDEA WHAT IM DOING
*/

#include <pthread.h>		// multithreading
#include <sys/types.h>		// constants
#include <sys/socket.h>		// socket control
#include <netinet/in.h>		// net control
#include <arpa/inet.h>		// protocol standards
#include <netdb.h>			// ???
#include <stdio.h>			// print output if DEBUG = 1
#include <stdlib.h>			
#include <string.h>			// strcmp and strncpy
#include <unistd.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <errno.h>			// error control btw
#include <vector>			// dynamic tables lets go
#include <string>			// say goodbye to this little char array of yours!

#define MAX_USERS 100
#define MAIN_PORT 1124

const int DEBUG = 1;
const char userFile[] = "users";
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct Message
{
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


class User
{
	public:
		char username[16];
		char password[16];
		bool online = false;
		int fd = -1;
		std::vector<Message> msgs;
		std::vector<int> friends;

		User(char u[16], char p[16])
		{
			strncpy(this->username, u, 16);
			strncpy(this->password, p, 16);
		}

		void addFriend(int id)
		{
			this->friends.push_back(id);
		}
};


class Server
{
	private:
		int myport;
		int fd_socket;
		struct sockaddr_in addr;
		pthread_t threads[MAX_USERS];
		std::vector<User> users;
		/*
		int regUsers;
		int logUsers;
		RegUser userList[MAX_USERS];
		LogUser loggedList[MAX_USERS];
		*/

		struct PthData
		{
			int fd;
			Server * server;
		};

		int findUser(char u[16])
		{
			for (uint i = 0; i < this->users.size(); i++)
			{
				if (strcmp(this->users[i].username, u) == 0) 
					return i;
			}
			return -1;
		}

		std::string getFriends(int id)
		{
			char buf[1024] = {0};
			std::string result = "";
			for (uint i = 0; i < this->users[id].friends.size(); i++)
			{
				result += users[this->users[id].friends[i]].username;
			}
			return result;
		}
		std::string getUsers(uint id)
		{
			char buf[1024] = {0};
			std::string result = "";
			for (uint i = 0; i < this->users.size(); i++)
			{
				if(i != id) result += users[i].username;
			}
			return result;
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

		void loadUsers(const char * filename)
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
					// strncpy(userList[currentUser].username, username, 16);
					// strncpy(userList[currentUser].password, password, 16);
					this->users.push_back(User(username, password));
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
			return;
		}

		int loginChecker(char username[16], char password[16])
		{
			for (uint i = 0; i < this->users.size(); i++)
			{
				if ( strcmp(username, users[i].username) == 0 &&
					strcmp(password, users[i].password) == 0 )
					return 1;
			}
			return 0;
		}
		
		void sendMessage(struct Message msg)
		{
			int to = this->findUser(msg.to); // findLoggedUser(letter.to) // find reciever
			if ( users[to].online) 						// reciever is online
			{
				write(users[to].fd,msg.from,16);
				write(users[to].fd, msg.content,1024); //sending message
			}
			else 									// reciever is offline
			{
				if ( this->users[to].msgs.size() < 50 ) // adding to a heap of undelivered messages...
				{
					pthread_mutex_lock(&lock);
						this->users[to].msgs.push_back(msg);
					pthread_mutex_unlock(&lock);
				}
				else 								// heap is already full
				{
					if (DEBUG) printf("User: %s too many messages\n",users[to].username);
				}
			}
			
			// todo: send confirmation back
			return;
		}
		int login(int fd)
		{
			char u[16], p[16];
			// logging in happens here
			while (1) 
			{
				memset(&u, 0, 16);
				memset(&p, 0, 16);
				read(fd, &u, sizeof(u));
				read(fd, &p, sizeof(p));
				if (DEBUG) printf("User %s tries to log in...\n", u);

				if (this->loginChecker(u, p)) break;
				else
				{
					char msg[] = "Login unsuccessful!\0";
					write(fd, &msg, sizeof(msg));
				}
				//TODO OBSLUGA PRZYPADKU GDY UZYTKOWNIK ZAMKNIE APLIKACJE W TYM MIEJSCU
			}
			char msg[] = "Login successful!\0";
			write(fd, &msg, sizeof(msg));
			if (DEBUG) printf("User %s logged in.\n", u);
			int userID = this->findUser(u);
			this->users[userID].online = true;
			this->users[userID].fd = fd;

			return userID;
		}

		void initialize(int userID)
		{
			std::string result = this->getUsers(userID);
			//SEND ALL USERS
			// maybe? write(fd1, result, sizeof(result));

			//sending unread messages
			for(uint i =0; i<this->users[userID].msgs.size();i++)
			{
				write(this->users[userID].fd,this->users[userID].msgs[i].from,16);
				write(this->users[userID].fd,this->users[userID].msgs[i].content,1024);
			}
			this->users[userID].msgs.clear();
		}

		static void *handleThread(void *arg)
		{
			PthData x = *((PthData *) arg);
			int fd1 = x.fd;
			Server * s = x.server;

			int userID = s->login(fd1);
			s->initialize(userID); //sending unread messages, maybe all users?


			// handling requests happens here
			char buf[1024] = {0};
			while (1)
			{
				read(fd1, &buf, sizeof(buf));	//receiving option to do

				switch (buf[0])
				{
					case 'f': //friends list
						{
							std::string result = s->getFriends(userID);
							//SENDING FRIENDS LIST
							//write(fd1,result,sizeof(result)); ???
						}
						break;
					//case 'a': add friend

					case 'm': // send messages
						{
							Message msg;

							strcpy(msg.from,s->users[userID].username);
							read(fd1,&msg.to,16);
							read(fd1,&msg.content,1024);

							s->sendMessage(msg);
							
						}
						break;

					case 'l': //logout
						{
							s->users[userID].online = false;
							s->users[userID].fd = -1;

							if (DEBUG) printf("User: %s: Logout\n",s->users[userID].username);
							break;
						}
						break;

					default:
						{
							if(DEBUG) printf("User: %s: Uknown command\n",s->users[userID].username);
						}
						break;

					// todo
				}

				//write(fd1, &buf, sizeof(buf)); don't need?
			}

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
					PthData pdata;
					pdata.fd = client;
					pdata.server = this;
					if (DEBUG) printf("Incoming connection from %s...\n", inet_ntoa(sa.sin_addr));
					int cr = pthread_create(&threads[i], NULL, handleThread, (void *)&pdata);
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
			loadUsers(filename);
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