/*

SK2 - Komunikator typu Gadu-Gadu

Autorzy:
Jakub Gardecki (inf141218)
Daniel Zwierzchowski (inf141338)

12.01.2021 22:19

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
int DEBUG = 0;

// Mutex locks for multithreading control
pthread_mutex_t lock_friends = PTHREAD_MUTEX_INITIALIZER;	//mutex to friends
pthread_mutex_t lock_invitation = PTHREAD_MUTEX_INITIALIZER;//mutex to invitations


// Few structs that are widely used in server's code.

struct Message
{
	char from[16];
	char to[16];
	char content[1024];
};

struct Invitation
{
	int user1;
	int user2;
};

/* 
Class containing data about the user.
It stores login data (for logging phase), current connection info
(descriptor and status) and also vectors with undelivered messages,
current friends and pending invitations.
*/
class User
{
	public:
		char username[16];						// User's username.
		char password[16];						// Password (for loging in)
		bool online = false;					// Current connection status.
		int fd = -1;							// Current connection descriptor
		std::vector<Message> msgs;				// Vector with undelivered messages.
		std::vector<int> friends;				// Vector with friends list
		std::vector<Invitation> invitations;	// Vectot with invitation list
		pthread_mutex_t lock_msg = PTHREAD_MUTEX_INITIALIZER;		//mutex to messages


		// Create instance of user with given username and password.
		// Used while loading users from file.
		User(char u[16], char p[16])
		{
			strncpy(this->username, u, 16);
			strncpy(this->password, p, 16);
		}

		// Add friend with given id to vector friends.
		void addFriend(int id)
		{
			pthread_mutex_lock(&lock_friends);
				this->friends.push_back(id);
			pthread_mutex_unlock(&lock_friends);

		}

		// Delete friend with given id.
		void delFriend(int id)
		{
			pthread_mutex_lock(&lock_friends);
				for (uint i = 0; i<this->friends.size();i++)
				{
					if (id == friends[i])
					{
						friends.erase(friends.begin() + i);
					}
				}
			pthread_mutex_unlock(&lock_friends);
		}
};


/*
Main class of the program.
Contains crucial data and methods that are required to properly
handle every request of the client.
*/
class Server
{
	private:
		int myport;						// Port number
		int fd_socket;					// Descriptor to socket
		struct sockaddr_in addr;		// Info about socket
		pthread_t threads[MAX_USERS];	// Array with threads for all users
		std::vector<User> users;		// Vector of all users registered

		// Used in *handleThread.
		struct PthData
		{
			int fd;
			Server * server;
		};

		// Returns id of a user with a given username. If not found, returns -1.
		int findUser(char u[16])
		{
			for (uint i = 0; i < this->users.size(); i++)
			{
				if (strcmp(this->users[i].username, u) == 0) 
					return i;
			}
			return -1;
		}

		// Returns string containing usernames of user's (with given username) friends.
		std::string getUsers(uint id)
		{
			std::string result = "";
			for (uint i = 0; i < this->users.size(); i++)
			{
				if(i != id) result += users[i].username;
			}
			return result;
		}
		
		// Erases given invitation from both users' invitation vectors.
		void popInvitation(int id1, int id2)
		{
			pthread_mutex_lock(&lock_invitation);
				for (uint i = 0; i < this->users[id1].invitations.size(); i++)
				{
					if (this->users[id1].invitations[i].user1 == id2)
						this->users[id1].invitations.erase(this->users[id1].invitations.begin() + i);
				}
				for (uint i = 0; i < this->users[id2].invitations.size(); i++)
				{
					if(this->users[id2].invitations[i].user2 == id1)
						this->users[id2].invitations.erase(this->users[id2].invitations.begin() + i);
				}
			pthread_mutex_unlock(&lock_invitation);
			return;
		}

		// Reads one word from given file descriptor into given char array.
		// It's entirely written in basic C, but it works.
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

		// Loads users' data into the server from a given file.
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

		// Used to verify if given username and password are correct AND if user is not already logged in.
		int loginChecker(char username[16], char password[16])
		{
			for (uint i = 0; i < this->users.size(); i++)
			{
				if ( strcmp(username, users[i].username) == 0 &&
					strcmp(password, users[i].password) == 0 &&
					!users[i].online)
					return 1;
			}
			return 0;
		}
		
		// Adds the given message to a reciever's inbox. Prints an error if inbox is full.
		void sendMessage(struct Message msg)
		{
			int to = this->findUser(msg.to);
			pthread_mutex_lock(&users[to].lock_msg);
			if ( this->users[to].msgs.size() < 200 )
			{
				this->users[to].msgs.push_back(msg);
				if (DEBUG) printf("User: %s Message Sending - added to a heap\n", msg.from);
			}
			else printf("User: %s has too many messages! Message from %s discarded...\n", users[to].username, msg.from);
			pthread_mutex_unlock(&users[to].lock_msg);
			return;
		}
		void receive()
		{

		}

		// Handles the login of a client.
		int login(int fd)
		{
			char u[16], p[16];
			
			while (1) 
			{
				
				// Read username and password from the client.
				memset(&u, 0, 16);
				memset(&p, 0, 16);
				if(read(fd, &u, sizeof(u)) == 0)
				{
					u[0] ='l';
					u[1]= 0;
				}
				if (u[0] == 'l' && u[1] == 0) // Sudden connection failure handling
				{
					close(fd);
					if (DEBUG) printf("Exiting thread... (fd: %d)\n", fd);
					pthread_exit(NULL);
				}
				if(read(fd, &p, sizeof(p)) == 0)
				{
					close(fd);
					if (DEBUG) printf("Exiting thread... (fd: %d)\n", fd);
					pthread_exit(NULL);
				}
				if (DEBUG) printf("User %s tries to log in...\n", u);

				// Check login credentials.
				if (this->loginChecker(u, p)) break;
				else
				{
					char msg[] = "Login unsuccessful!\0"; // Send result
					if(write(fd, &msg, sizeof(msg)) == -1)
					{				
						close(fd);
						if (DEBUG) printf("Exiting thread... (fd: %d)\n", fd);
						pthread_exit(NULL);
					}
				}
				
			}

			// If login successful, send confirmation and update server's data
			char msg[] = "Login successful!\0";
			if(write(fd, &msg, sizeof(msg)) == -1)
			{
					close(fd);
					if (DEBUG) printf("Exiting thread... (fd: %d)\n", fd);
					pthread_exit(NULL);
			}
			if (DEBUG) printf("User %s logged in.\n", u);
			int userID = this->findUser(u);
			this->users[userID].online = true;
			this->users[userID].fd = fd;

			return userID;
		}

		/*
		Biggest function in the entire code.
		Every thread runs this function to handle the requests of a single client.
		Both login and requests phases are fired from here.
		Parameter is a PthData struct, containing socket descriptor for communication
		with client, and a pointer to a server instance.
		*/
		static void *handleThread(void *arg)
		{
			PthData *x = ((PthData*) arg);
			int fd1 = x->fd;						// Descriptor of client
			Server * s = x->server;				// Pointer to server
			// Strart the login phase
			int userID = s->login(fd1);

			// Handling requests happens here
			char buf[1024] = {0};
			while (1)
			{
				
				// Wait for and read the clients request type
				memset(buf, 0, 1024);
				if (read(fd1, &buf, sizeof(buf)) <= 0) break;

				// Depending on a client's request, behave accordingly
				switch (buf[0])
				{
					
					// Sending friends list to client
					case 'f': 
					{
						if (DEBUG) printf("User: %s Sending friends list...\n",s->users[userID].username);

						char result[1024] = {0};
						pthread_mutex_lock(&lock_friends);
							for (uint i = 0; i < s->users[userID].friends.size(); i++)
							{
								strcat(result, s->users[s->users[userID].friends[i]].username);
								strcat(result,"\n");
							}
						pthread_mutex_unlock(&lock_friends);
						if(write(fd1,result,1024) == -1)
						{
							buf[0] = 0;
							break;
						}
						if (DEBUG) printf("User: %s Sending friends list - successful\n",s->users[userID].username);
					}
					break;
					
					// Invite friend
					case 'a': 
					{
						if (DEBUG) printf("User: %s Sending invitation...\n",s->users[userID].username);
						char u[16];				
						if(read(fd1,&u,16) <= 0) // Reveive username to invite
						{
							buf[0] = 0; 
							break;
						}		
						bool isFriend = false;
						int id = s->findUser(u);
						if (id == userID)
						{
							char mess[] = "You can't invite yourself!";
							if(write(fd1, mess, sizeof(mess))==-1)
							{				
								buf[0] = 0;
								break;
							}

						}
						else if (id != -1) 		// Check if user exists
						{
							pthread_mutex_lock(&lock_invitation);
							for (uint i = 0; i <s->users[userID].invitations.size(); i++)	// Check if invitation has been earlier sent
							{
								if (s->users[userID].invitations[i].user2 == id || s->users[id].invitations[i].user1 == userID)
								{
									if(write(fd1, "Was sent earlier\n", 16) ==-1)
									{				
										buf[0] = 0;
										break;
									}
									isFriend = true;
									break;
								}
							}
							pthread_mutex_unlock(&lock_invitation);

							pthread_mutex_lock(&lock_friends);
							for (uint i = 0; i < s->users[userID].friends.size(); i++) 	//check if user is already friend
							{
								if (id == s->users[userID].friends[i])	
								{
									if(write(fd1,"User is friend\n",16) ==-1)
									{				
										buf[0] = 0;
										break;
									}
									isFriend = true;
									break;
								}
							}
							pthread_mutex_unlock(&lock_friends);

							if (!isFriend) // If user exist, wasn't invited earlier and isn't friend, then send invitation
							{
								
								// Create the invitation to send
								Invitation tmp;
								tmp.user1 = userID;
								tmp.user2 = id;

								pthread_mutex_lock(&lock_invitation);
								s->users[id].invitations.push_back(tmp);
								s->users[userID].invitations.push_back(tmp);
								pthread_mutex_unlock(&lock_invitation);
								if(write(fd1, "Invitation was send\n", 21) ==-1)
								{				
									buf[0] = 0;
									break;
								}
								
								// Create the message with invitation info for client
								Message msg;
								memset(msg.from, 0, 16);
								memset(msg.to, 0, 16);
								memset(msg.content, 0, 1024);
								strcpy(msg.to,s->users[id].username);
								strcpy(msg.from,s->users[userID].username);
								strcpy(msg.content,"");

								pthread_mutex_lock(&(s->users[id].lock_msg));
								s->users[id].msgs.push_back(msg);
								pthread_mutex_unlock(&(s->users[id].lock_msg));

							}
						}
						else if(write(fd1,"User don't exist\n",16) ==-1)
							{				
								buf[0] = 0;
								break;
							}
						
						if (DEBUG) printf("User: %s Invitation - successful\n",s->users[userID].username);
					}
					break;
					
					// Accept or decline invitation from user
					case 'b': 
					{	
						if (DEBUG) printf("User: %s decision...\n", s->users[userID].username);
						char decision[16] = {0};
						if(read(fd1, &decision, 16) <= 0)	// Receive decision
						{
							buf[0] = 0; 
							break;
						}
						char user[16] = {0};			
						if(read(fd1, &user, 16) <= 0)		// Receive username user accept or decline
						{
							buf[0] = 0; 
							break;
						}
						int id = s->findUser(user);
						if (id == -1)
						{
							printf("User %s tried to add %s as a friend, aborting...\n", s->users[userID].username, user);
							break;
						}
						if (decision[0] == 't') 	// If wants to accept
						{
							
							s->users[userID].addFriend(id);		// Add user to friend list
							s->users[id].addFriend(userID);		// Same as before
							s->popInvitation(userID,id); 		// Delete from invitation list, casue decision has been made
						}
						else if(decision[0] == 'f') // If wants to decline
						{
							s->popInvitation(userID,id);		// Delete from invitation list, casue decision has been made
						}
						else printf("User %s selected invalid option during friend request confirmation, aborting...\n", s->users[userID].username);
						if (DEBUG) printf("User: %s added user %s to friends.\n",s->users[userID].username, s->users[id].username);
					}
					break;
					
					// Delete a friend
					case 'd': 
					{	
						if(DEBUG) printf("User: %s delete friend...\n",s->users[userID].username);
						char user[16];
						if(read(fd1,&user,16) <= 0)
						{
							buf[0] = 0; 
							break;
						}
						
						int id = s->findUser(user);
						if (id == -1)
						{
							char mess[] = "No such user!";
							if(write(fd1, mess, sizeof(mess)) ==-1)
							{				
								buf[0] = 0;
								break;
							}
							if(DEBUG) printf("User: %s delete friend aborted, user not found\n",s->users[userID].username);
						}
						else
						{
							s->users[userID].delFriend(id);
							s->users[id].delFriend(userID);
							char mess[] = "Friend removed.";
							if(write(fd1, mess, sizeof(mess)) ==-1)
							{				
								buf[0] = 0;
								break;
							}
							if(DEBUG) printf("User: %s delete friend - successful\n",s->users[userID].username);
						}
					}
					break;

					// Deliver undelivered messages from inbox
					case 'g':
					{	
						if (DEBUG) printf("User: %s - get messages\n", s->users[userID].username);
						pthread_mutex_lock(&(s->users[userID].lock_msg));
						char tmp[1024] = {0};
						int temp = int(s->users[userID].msgs.size());
						if(write(fd1, &temp, sizeof(int)) ==-1)	// Send number of messages first
						{				
							buf[0] = 0;
							break;
						}		
						for (uint i = 0; i < s->users[userID].msgs.size(); i++)
						{
							strcat(tmp,s->users[userID].msgs[i].from);
							strcat(tmp,"\n");
							strcat(tmp,s->users[userID].msgs[i].content);
							if(write(s->users[userID].fd,&tmp,1024) ==-1)//send Message
							{				
								buf[0] = 0;
								break;
							} 
						}
						s->users[userID].msgs.clear();
						pthread_mutex_unlock(&(s->users[userID].lock_msg));
						if (DEBUG) printf("User: %s - get messages successful\n", s->users[userID].username);
					}
					break;

					// Send a message
					case 'm':
					{
						if (DEBUG) printf("User: %s Sending Message...\n",s->users[userID].username);

						Message msg;
						memset(msg.from, 0, 16);
						memset(msg.to, 0, 16);
						memset(msg.content, 0, 1024);
						strcpy(msg.from,s->users[userID].username);	// get sender username
						if(read(fd1,&msg.to,16) <= 0)					// get receiver username
						{
							buf[0] = 0; 
							break;
						}
						//while(1)
						//{
							int size = read(fd1,&msg.content,1024);
							s->sendMessage(msg);
						//	if(size >= 0 && size <1024)
						//		break;		
						//}
												// send message
					}
					break;

					// Logout
					case 'l':
						break;
						
					// Unknown command. Basically unused
					default:
					{
						if (DEBUG) printf("User: %s: Unknown command\n",s->users[userID].username);
					}
					break;

				}
				
				// Logout gets handled here, both expected and unexcpected
				if (!s->users[userID].online || buf[0] == 'l' || buf[0] == 0)
				{
					s->users[userID].online = false;
					s->users[userID].fd = -1;
					if (DEBUG) printf("User: %s: Logout\n",s->users[userID].username);
					break;
				}
			}

			// Close connection and shut down thread
			close(fd1);
			delete x;
			if (DEBUG) printf("Exiting thread... (fd: %d)\n", fd1);
			pthread_exit(NULL);
		}

		// Main loop of the main thread. Accepts connections and generates new threads
		void mainLoop()
		{
			socklen_t sa_size;
			struct sockaddr_in sa;
			
			int i = 0;
			while (1)
			{
				sa_size = sizeof(sa);
				int client = accept(fd_socket, (struct sockaddr *)&sa, &sa_size);
				if (client < 0)
				{
					printf("Error occured on accepting connection!\n");
					perror("accept");
				}
				else
				{
					// Create new thread
					PthData *pdata = new PthData();
					pdata->fd = client;
					pdata->server = this;
					if (DEBUG) printf("Incoming connection from %s...\n", inet_ntoa(sa.sin_addr));
					int cr = pthread_create(&threads[i], NULL, handleThread, (void *)pdata);
					
					// If thread creation failed, discard connection
					if (cr != 0)
					{
						if (DEBUG) printf("ERROR - thread creation failed!\n");
						close(client);
					}
					else if (DEBUG) printf("New thread created! (fd: %d)\n", client);
					i++;
				}


			}
		}

	public:
		
		// Create the server instance on given port and user data loaded from given filename
		Server(int port, const char * filename)
		{
			loadUsers(filename);
			myport = port;
		}

		// Start the server,
		void start()
		{
			if (DEBUG) printf("Starting services...\n");

			fd_socket = socket(AF_INET, SOCK_STREAM, 0);	// Initialize the socket
			
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
			setsockopt(fd_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nFoo, sizeof(nFoo));	// Set socket properties

			if (bind(fd_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)		// Bind the socket
			{
				printf("ERROR - bind failed.\n");
				exit(0);
			}
			else if (DEBUG) printf("Bind succesfull...\n");

			if (listen(fd_socket, 10) < 0)		// Start up the listening
			{
				printf("ERROR - listen failed.\n");
				exit(0);
			}
			else if (DEBUG) printf("Listening set up.\n");
			
			if (DEBUG) printf("Server is now fully operational! I'm all ears.\n");
			
			// Run the main loop.
			mainLoop();
		}
};


int main(int argc, char * argv[])
{
	if (argc != 4)
	{
		printf("Invalid usage!\nPlease use: ./serwer.out port filename debug(0/1)\nFor example: ./serwer.out 1124 users 1");
	}
	else
	{
		int port = atoi(argv[1]);
		const char * filename = argv[2];
		DEBUG = atoi(argv[3]);
		Server server = Server(port, filename);
		server.start();
	}
	return 0;
}