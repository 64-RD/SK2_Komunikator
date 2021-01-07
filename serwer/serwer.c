/*
I HAVE NO IDEA WHAT IM DOING
MUCH MORE MUTEXXXXX - DELETE THEM IF YOU THINK THEY ARE USELESS
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
pthread_mutex_t lock_msg = PTHREAD_MUTEX_INITIALIZER;		//mutex to messages
pthread_mutex_t lock_friends = PTHREAD_MUTEX_INITIALIZER;	//mutex to friends
pthread_mutex_t lock_invitation = PTHREAD_MUTEX_INITIALIZER;//mutex to invitations



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
struct Invitation
{
	int user1;
	int user2;
};


class User
{
	public:
		char username[16];
		char password[16];
		bool online = false;
		int fd = -1;
		std::vector<Message> msgs;				//just message(from,to,content)
		std::vector<int> friends;				//friends list
		std::vector<Invitation> invitations;	//invitation list

		User(char u[16], char p[16])
		{
			strncpy(this->username, u, 16);
			strncpy(this->password, p, 16);
		}

		void addFriend(int id)
		{
			pthread_mutex_lock(&lock_friends);
				this->friends.push_back(id);
			pthread_mutex_unlock(&lock_friends);

		}
		void delFriend(int id)
		{
			pthread_mutex_lock(&lock_friends);
				for(uint i = 0; i<this->friends.size();i++)
				{
					if(id == friends[i])
					{
						friends.erase(friends.begin() + i);
					}
				}
			pthread_mutex_unlock(&lock_friends);
		}
};


class Server
{
	private:
		int myport;						//just port
		int fd_socket;					//descriptor to socket
		struct sockaddr_in addr;		//info about socket
		pthread_t threads[MAX_USERS];	//array with threads for all users
		std::vector<User> users;		//vector of all users registered
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

		int findUser(char u[16])	//find number of user having his username
		{
			for (uint i = 0; i < this->users.size(); i++)
			{
				if (strcmp(this->users[i].username, u) == 0) 
					return i;
			}
			return -1;
		}

/*		char *getFriends(int id)
		{
			char result[1024];
			for (uint i = 0; i < this->users[id].friends.size(); i++)
			{
				strcat(result, users[this->users[id].friends[i]].username);
				strcat(result,"\n");
			}
			return result;
		}
		*/
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
/*		char *getInvitation(uint id)
		{
			char result[1024];

			for (uint i = 0; i < this->users[id].invitations.size(); i++)
			{
				strcat(result,users[this->users[id].invitations[i].user1].username);
				strcat(result,"\n");
			}
			return result;
		}*/
		void popInvitation(int id1, int id2)	//remove invitation cause decision has been made(accept or decline)
		{
			pthread_mutex_lock(&lock_invitation);
				for(uint i = 0 ;i < this->users[id1].invitations.size();i++)
				{
					if(this->users[id1].invitations[i].user1 == id2)
					{
						this->users[id1].invitations.erase(this->users[id1].invitations.begin() + i);
					}
				}
				for(uint i = 0 ;i < this->users[id2].invitations.size();i++)
				{
					if(this->users[id2].invitations[i].user2 == id1)
					{
						this->users[id2].invitations.erase(this->users[id2].invitations.begin() + i);
					}
				}
			pthread_mutex_unlock(&lock_invitation);
			return;

		}
		void readOneWord(int fd, char readTo[16])	//no need to comment lmao
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

		void loadUsers(const char * filename)	//load all users from file
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

		int loginChecker(char username[16], char password[16])		//check if username and password are correct
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
			pthread_mutex_lock(&lock_msg);
				if ( this->users[to].msgs.size() < 50 ) // adding to a heap of undelivered messages...
				{
					this->users[to].msgs.push_back(msg);
					if (DEBUG) printf("User: %s Message Sending - added to a heap\n",msg.from);
				}
				else 								// heap is already full
				{
					if (DEBUG) printf("User: %s has too many messages\n",users[to].username);
				}
			pthread_mutex_unlock(&lock_msg);
			
			return;
		}

		int login(int fd)		// logging in happens here
		{
			//TODO PREVENT LOGIN FROM TWO AND MORE CLIENTS AT ONE ACCOUNT
			char u[16], p[16];
			
			while (1) 
			{
				memset(&u, 0, 16);
				memset(&p, 0, 16);

				read(fd, &u, sizeof(u));
				if(u[0] == 'l' && u[1] == 0) //i hope it works, but is needed when client suprasingly close a application in login window
				{
					close(fd);
					if (DEBUG) printf("Exiting thread... (fd: %d)\n", fd);
					pthread_exit(NULL);
				}
				read(fd, &p, sizeof(p));
				if (DEBUG) printf("User %s tries to log in...\n", u);

				if (this->loginChecker(u, p)) break;
				else
				{
					char msg[] = "Login unsuccessful!\0";
					write(fd, &msg, sizeof(msg));
				}
				
			}
			char msg[] = "Login successful!\0";
			write(fd, &msg, sizeof(msg));
			if (DEBUG) printf("User %s logged in.\n", u);
			int userID = this->findUser(u);
			this->users[userID].online = true;
			this->users[userID].fd = fd;

			return userID;
		}

		static void *handleThread(void *arg)
		{
			PthData x = *((PthData *) arg);
			int fd1 = x.fd;						//descriptor of client
			Server * s = x.server;				//pointer to server

			int userID = s->login(fd1);			//log in to server

			// handling requests happens here
			char buf[1024] = {0};
			while (1)
			{
				read(fd1, &buf, sizeof(buf));	//receiving option to do

				switch (buf[0])
				{
					case 'f': //sending friends list
						{
							if (DEBUG) printf("User: %s Sending friends list...\n",s->users[userID].username);

							char result[1024];
							pthread_mutex_lock(&lock_friends);
								for (uint i = 0; i < s->users[userID].friends.size(); i++)
								{
									strcat(result, s->users[s->users[userID].friends[i]].username);
									strcat(result,"\n");
								}
							pthread_mutex_unlock(&lock_friends);
							write(fd1,result,1024);
							if (DEBUG) printf("User: %s Sending friends list - successful\n",s->users[userID].username);
						}
						break;
					case 'a': //invite friend
						{
							if (DEBUG) printf("User: %s Sending invitation...\n",s->users[userID].username);
							char u[16];				
							read(fd1,&u,16);		//reveive username to invite
							bool isFriend = false;
							int id = s->findUser(u);
							if(id != -1) 			//check if user exists
							{
								pthread_mutex_lock(&lock_invitation);
									for(uint i = 0;i <s->users[userID].invitations.size();i++)	//check if invitation has been earlier sent
									{
										if(s->users[userID].invitations[i].user2 == id || s->users[id].invitations[i].user1 == userID)
										{
											write(fd1,"Was sent earlier\n",16);
											break;
										}
									}
								pthread_mutex_unlock(&lock_invitation);

								pthread_mutex_lock(&lock_friends);
									for(uint i = 0;i < s->users[userID].friends.size();i++) 	//check if user is already friend
									{
										if(id == s->users[userID].friends[i])	
										{
											write(fd1,"User is friend\n",16);
											isFriend == true;
											break;
										}
									}
								pthread_mutex_unlock(&lock_friends);
								if(!isFriend) //if exist,wasn't invated earlier and isn't friend then set invitation
								{
									Invitation tmp;
									tmp.user1 = userID;
									tmp.user2 = id;

									pthread_mutex_lock(&lock_invitation);
										s->users[id].invitations.push_back(tmp);
										s->users[userID].invitations.push_back(tmp);
									pthread_mutex_unlock(&lock_invitation);
									write(fd1,"Invitation was send\n",21);


								}
							}
							else
							{
								write(fd1,"User don't exist\n",16);
							}

						}
						if (DEBUG) printf("User: %s Invitation - successful\n",s->users[userID].username);
						break;
					case 'i'://send invitation list
						{
							if (DEBUG) printf("User: %s Sending invitation list...\n",s->users[userID].username);
							char result[1024];

							pthread_mutex_lock(&lock_invitation);
								for (uint i = 0; i < s->users[userID].invitations.size(); i++)
								{
									strcat(result,s->users[s->users[userID].invitations[i].user1].username);
									strcat(result,"\n");
								}
							pthread_mutex_unlock(&lock_invitation);

							write(fd1,result,1024);
							if (DEBUG) printf("User: %s Sending invitation list - successful\n",s->users[userID].username);
						}
						break;
					case 'b': //accept or decline invitation from user
						{
							if (DEBUG) printf("User: %s decision...\n",s->users[userID].username);
							char decision[16];
							read(fd1,&decision,16);	//just receive decision
							char user[16];			
							read(fd1,&user,16);		//receive username user accept or decline
							int id = s->findUser(user);
							if(decision[0] = 't') //if want to accept
							{
								pthread_mutex_lock(&lock_friends);
									s->users[userID].addFriend(id);		// add user to friend list
									s->users[id].addFriend(userID);		//
								pthread_mutex_unlock(&lock_friends);

								pthread_mutex_lock(&lock_invitation);
									s->popInvitation(userID,id); 				// delete from invitation list, casue decision has been made
								pthread_mutex_unlock(&lock_invitation);
							}
							else if(decision[0] = 'f') //if want to decline
							{
								pthread_mutex_lock(&lock_invitation);
									s->popInvitation(userID,id);				// delete from invitation list, casue decision has been made
								pthread_mutex_unlock(&lock_invitation);

							}
							if (DEBUG) printf("User: %s decision - successful\n",s->users[userID].username);
							break;
						}
					case 'd': //delete a friend
						{
							
							if(DEBUG) printf("User: %s delete friend...\n",s->users[userID].username);
							char user[16];
							read(fd1,&user,16);
							
							int id = s->findUser(user);
							s->users[userID].delFriend(id);
							s->users[id].delFriend(userID);
							if(DEBUG) printf("User: %s delete friend - successful\n",s->users[userID].username);

						}
						break;

					case 'g':
						{
						pthread_mutex_lock(&lock_msg);
							char tmp[16];
							sprintf(tmp,"%d",int(s->users[userID].msgs.size()));	//convert to char
							write(fd1,tmp,16);		//send number of messages
							for(uint i =0; i<s->users[userID].msgs.size();i++)
							{
								write(s->users[userID].fd,s->users[userID].msgs[i].from,16);//send name of sender
								write(s->users[userID].fd,s->users[userID].msgs[i].content,1024);//send message
							}
							s->users[userID].msgs.clear();
						pthread_mutex_unlock(&lock_msg);
						}
						break;
					case 'm': // send messages
						{
							if (DEBUG) printf("User: %s Sending Message...\n",s->users[userID].username);

							Message msg;

							strcpy(msg.from,s->users[userID].username);	//set sender
							read(fd1,&msg.to,16);						//receive and set receiver
							read(fd1,&msg.content,1024);				//receive content of a message

							s->sendMessage(msg);						//send message
							
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