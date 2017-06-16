#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <map>
#include <ctype.h>
#include "passdb.h"
//#include "mychat.h"

struct chatter {
  char server[61];
  char name[21];
  char password[21];
  int sock;
  int whispers;
  int room;
  pthread_t thread;
  struct chatter * next;
  };
  
 struct chatter * find(char *);
 
 pthread_mutex_t lock_writ=PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t  cond_writ=PTHREAD_COND_INITIALIZER;
 int writers=0;
 pthread_mutex_t lock_read=PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t  cond_read=PTHREAD_COND_INITIALIZER;
 int readers=0;
 pthread_mutex_t lock_head=PTHREAD_MUTEX_INITIALIZER;
 struct chatter * head=NULL;
 pthread_mutex_t lock_done=PTHREAD_MUTEX_INITIALIZER;
 struct chatter * done=NULL;
 
 pthread_mutex_t lock_pass=PTHREAD_MUTEX_INITIALIZER;
 std::map<std::string, std::string> passwdDB;
 
 
int writestr(int fd, char * msg) {
	 return send(fd, msg, strlen(msg), 0);
 }

 
 
int readstr(int fd, char * msg) {
	int ret=recv(fd, msg, 256, 0);
	int i;
	if (ret>=0) msg[ret]=0;
	for (i=ret-1; msg[i]=='\n' || msg[i]=='\r'; i--) msg[i]=0;
	 return ret;
 }

 
 void writer_lock(void) {
   // Tell the world another writer is waiting
   pthread_mutex_lock(&lock_writ);
   writers++;
   pthread_mutex_unlock(&lock_writ);
   // Wait for readers to get out of the way
   pthread_mutex_lock(&lock_read);
   while (readers>0)
    pthread_cond_wait(&cond_read,&lock_read);
   pthread_mutex_unlock(&lock_read);
   // Now that the readers are out of the way, we can
   // try to lock the client list then do our work
   pthread_mutex_lock(&lock_head);
   }
   
void writer_unlock(void) {
  //release head
   pthread_mutex_unlock(&lock_head);
  // Now tell the world one less writer is active
  pthread_mutex_lock(&lock_writ);
  writers--;
  if (writers==0) pthread_cond_signal(&cond_writ);
  pthread_mutex_unlock(&lock_writ);
  }
  
 // READER PROCEDURES
 
 void reader_access(void) {
  // Make sure no writers are interested in making changes
  pthread_mutex_lock(&lock_writ);
  while (writers>0)
    pthread_cond_wait(&cond_writ,&lock_writ);
  // Tell the world another reader is busy accessing the list
  pthread_mutex_lock(&lock_read);
  readers++;
  pthread_mutex_unlock(&lock_read);
  pthread_mutex_unlock(&lock_writ);
  }
  
void reader_finish(void) {
  // Tell the world I am done accessing the list
  pthread_mutex_lock(&lock_read);
  readers--;
  if (readers==0) pthread_cond_signal(&cond_read);
  pthread_mutex_unlock(&lock_read);
  }
  
// SOCKET FUNCTIONS

int setup_post(short port_number) {
  struct sockaddr_in puffaddr;
  int sock;
  if ((sock=socket(AF_INET,SOCK_STREAM,0))==-1)
    { perror("Could not create socket!"); exit(1);}
  puffaddr.sin_family=AF_INET;
  puffaddr.sin_addr.s_addr=INADDR_ANY;
  puffaddr.sin_port=htons(port_number);
  if (bind(sock,(struct sockaddr *)(&puffaddr),sizeof(puffaddr))<0)
    { perror("Could not bind socket!"); exit(1);}
  if (listen(sock,20)<0)
    { perror("Could not set max backlog size."); exit(1);}
  return sock;
  }
  
struct chatter * listening_post(int sock) {
  unsigned char * addy;
  struct sockaddr_in client;
  int length;
  struct hostent * hostptr;
  struct chatter * person;
  int fd;
  int fd_flags;
  length=sizeof(struct sockaddr);
  fd=accept(sock,(struct sockaddr *)(&client),&length);
  if (fd==-1) { printf("accept failed\n"); return NULL; }
  if ((fd_flags=fcntl(fd,F_GETFL,0))==-1) {
    close(fd); printf("initial fcntl failed"); return NULL; 
	} else {
	fd_flags|=O_NONBLOCK;
	if (fcntl(fd,F_SETFL,fd_flags)==-1) {
	  close(fd); printf("second fcntl failed"); return NULL; }
	}
  person=(struct chatter *)malloc(sizeof(struct chatter));
  //figure out domain name... if not possible, use ip addy
  hostptr=gethostbyaddr((char *)(&client.sin_addr.s_addr),4,AF_INET);
  if (hostptr==NULL) {
    addy=(unsigned char *)(&client.sin_addr.s_addr);
	sprintf(person->server,"%d.%d.%d.%d",addy[0],addy[1],addy[2],addy[3]);
	} else 
	strcpy(person->server,hostptr->h_name);
  person->sock=fd;
  person->next=NULL;
  sprintf(person->name,"Connecting...");
  person->whispers=1;
  person->room=1;
  return person;
  }
  
 // USER COMMAND FUNCTIONS
 
void broadcast(char * message) {
  struct chatter * current;
  reader_access();
  current=head;
  while (current!=NULL) {
    if (current->room==1) writestr(current->sock,message);
    current=current->next;
    }
  reader_finish();
  }
	
void whisper(struct chatter * person, char * buf) {
  int buflen;
  char * who_name;
  char * say_what;
  char wbuf[512];
  struct chatter * who_to;
  reader_access();
  buflen=strlen(buf);
  strtok(buf," \t");
  who_name=strtok(NULL," \t");
  if (who_name!=NULL) say_what=who_name+strlen(who_name)+1;
  if (who_name!=NULL && say_what-buf<buflen) {
    if ((who_to=find(who_name))!=NULL) {
      if (who_to->whispers==1) {
        sprintf(wbuf,"<%s> %s\n\r",person->name,say_what);
		writestr(who_to->sock,wbuf); 
		} else {
		sprintf(wbuf,"Sorry, %s isn't listening to whispers right now.\n\r",who_name);
		writestr(person->sock,wbuf);
		}
	  } else if (who_to==NULL) {
	    sprintf(wbuf,"Sorry, %s doesn't seem to be on.\n\r",who_name);
		writestr(person->sock,wbuf);
		}
	  } else {
	  if (who_name==NULL) 
	    sprintf(wbuf,"It helps to enter the command correctly.\n\r");
		else
	    sprintf(wbuf,"It helps to actually enter something to whisper.\n\r");
	  writestr(person->sock,wbuf);
	  }
  reader_finish();
  }
   
void wholist(struct chatter * person) {
	struct chatter * current;
	char buf[65536];
	char * whb;
	int i; 
	int n=0;
	reader_access();
	current=head;
	whb=buf;
	sprintf(whb,"---------- Who's online ----------\n\r");
	i=strlen(whb);
	whb+=i;
	while (current!=NULL) {
		n++;
		sprintf(whb,"[%s:%s]\n\r",current->server,current->name);
		i=strlen(whb);
		whb+=i;
		current=current->next;
		}
	sprintf(whb,"----------------------------------\n\r");
	i=strlen(whb);
	whb+=i;
	sprintf(whb,"Total users online: %d\n\r",n);
	i=strlen(whb);
	whb+=i;
	sprintf(whb,"----------------------------------\n\r");
	i=strlen(whb);
	whb+=i;
	writestr(person->sock,buf);
	reader_finish();
}

// UTILITY FUNCTIONS FOR MAIN CLIENT ROUTINE

struct chatter * find(char * nomen) {
	struct chatter * current;
	current=head;
	while (current!=NULL && !(strcmp(current->name,nomen)==0))
		current=current->next;
	return current;
}

void removal(struct chatter * person) {
	struct chatter * current;
	struct chatter * previous;
	current=head;
	previous=NULL;
	while (current!=NULL && person!=current) {
		previous=current;
		current=current->next;
	}
	if (previous==NULL)
		head=person->next;
		else
		previous->next=person->next;
	person->next=NULL;
}

void disconnect(struct chatter * ego) {
	// REMOVE CLIENT FROM CLIENTS LIST
	writer_lock();
	removal(ego);
	writer_unlock();
	//CLOSE SOCKET
	close(ego->sock);
	printf("Connection from %s closing.\n",ego->server);
	// PLACE SELF ON THE DONE LIST SO I CAN BE WAITED ON
	pthread_mutex_lock(&lock_done);
	ego->next=done;
	done=ego;
	pthread_mutex_unlock(&lock_done);
	printf("The thread is ready to be disposed of.\n");
	pthread_exit(NULL);
}

void await_input(int fd) {
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(fd,&readset);
	select(fd+1,&readset,NULL,NULL,NULL);
}

// checks name to make sure it consists of alpha characters only
// and enforces capitalization, removing leading and trailing spaces
// returns NULL if the string has non-alpha chars
char * check_name(char * name) {
	char * trail=name+strlen(name)-1;
	while (*trail==' ') { *trail=0; trail--;}
	char * lead=name;
	while (*lead==' ') { lead++;}
	char * inspect=lead;
	int flag=1;
	while (flag && inspect-lead<0) {
	  flag=isalpha(*inspect);
	  if (!flag) return NULL;
	  *inspect=tolower(*inspect);
	  inspect++;
	}
	*lead=toupper(*lead);
	return lead;
}

void login(struct chatter * ego) {
	char buf[256];
	char nomen[21];
	int i;
	int flag=3;
	char * inspect;
	while (!(flag==0)) {
		sprintf(buf,"Enter your username, please:\n\r");
		writestr(ego->sock,buf);
		do {
			await_input(ego->sock);
			if (readstr(ego->sock,buf)==0) disconnect(ego);
			//printf("buf %s\n",buf);
			inspect=check_name(buf);			
		} while (inspect!=NULL && !(strlen(inspect)>0 && strlen(inspect)<=20));
		strcpy(nomen,inspect);
		printf("Attempted login by: %s\n",nomen);
		reader_access();
		if (find(nomen)!=NULL) {
			reader_finish();
			sprintf(buf,"That person is already connected. Who are you, really?\n\r");
			writestr(ego->sock,buf);
			flag--;
			if (flag==0) {
				sprintf(buf,"See you on the dark side of the moon.\n\r");
				writestr(ego->sock,buf);
				disconnect(ego);
				}
			} else {
			reader_finish();
			flag=0;
			strcpy(ego->name,nomen);
			sprintf(buf,"Welcome to the machine, %s.\n\r",ego->name);
			writestr(ego->sock,buf);
			}
	}
	
	// check if name exists, if so check password
	// else ask for new password and write it to the DB
	int nameexist=0;
	pthread_mutex_lock(&lock_pass);
	nameexist=(passwdDB.find(nomen) != passwdDB.end());
	pthread_mutex_unlock(&lock_pass);
	if (nameexist) {	
		sprintf(buf,"Your password:\n\r",ego->name);
		writestr(ego->sock,buf);	
		await_input(ego->sock);
		if (readstr(ego->sock,buf)==0) disconnect(ego);	
		if (strcmp(passwdDB[nomen].c_str(),buf)!=0) {
			// send a bad password message first
			sprintf(buf,"Bad password, goodbye.\n\r",ego->name);
			writestr(ego->sock,buf);
			disconnect(ego);
			}			
		} else {			
		// prompt user, get passwd twice. if no match passwds, die. else enter into database and write passwdDB to disk
		char newpass1[256];
		char newpass2[256];
		sprintf(buf,"You are a new user. Please enter a password.\n\r",ego->name);
		writestr(ego->sock,buf);
		await_input(ego->sock);
		if (readstr(ego->sock,newpass1)==0) disconnect(ego);			
		sprintf(buf,"Please re-enter your password. \n\r",ego->name);
		writestr(ego->sock,buf);
		await_input(ego->sock);
		if (readstr(ego->sock,newpass2)==0) disconnect(ego);
		if (strcmp(newpass1,newpass2)==0) {
			// passwords match, enter into DB
			pthread_mutex_lock(&lock_pass);
			passwdDB[nomen]=newpass1;
			savepassDB(passwdDB,"chatpasswd"); // and save
			pthread_mutex_unlock(&lock_pass);			
			sprintf(buf,"Passwords match, user saved.\n\r",ego->name);
			writestr(ego->sock,buf);
			} else {
			sprintf(buf,"Passwords do not match, goodbye.\n\r",ego->name);
			writestr(ego->sock,buf);
			disconnect(ego);	
			}
		
		
		}
	
}


void get_timestamp(char * buf) {
	time_t current_time;
    char* c_time_string;
	
	current_time = time(NULL);
	c_time_string = ctime(&current_time);
	sprintf(buf,"Server time is %s\r",c_time_string);
}

void change_password(struct chatter * ego) {
	char buf[2048];
	char wbuf[2048];
	char newpass1[2048];
	char newpass2[2048];	
	sprintf(wbuf,"Enter current password:\n\r");
	writestr(ego->sock,wbuf);
	await_input(ego->sock);
	if (readstr(ego->sock,buf)==0) disconnect(ego);
	pthread_mutex_lock(&lock_pass);
	int oldpassgood=strcmp(passwdDB[ego->name].c_str(),buf)==0;
	pthread_mutex_unlock(&lock_pass);
	
	
	if (oldpassgood) {
		sprintf(wbuf,"Enter new password:\n\r");
		writestr(ego->sock,wbuf);
		await_input(ego->sock);
		if (readstr(ego->sock,newpass1)==0) disconnect(ego);
		sprintf(wbuf,"Repeat new password:\n\r");
		writestr(ego->sock,wbuf);
		await_input(ego->sock);
		if (readstr(ego->sock,newpass2)==0) disconnect(ego);
		if (strcmp(newpass1,newpass2)==0) {
			// passwords match, enter into DB
			pthread_mutex_lock(&lock_pass);
			passwdDB[ego->name]=newpass1;
			savepassDB(passwdDB,"chatpasswd"); // and save
			pthread_mutex_unlock(&lock_pass);			
		} else {
			sprintf(wbuf,"Passwords do not match.\n\r");
			writestr(ego->sock,wbuf);						
		}		
	} else {
	sprintf(wbuf,"Incorrect password.\n\r");
	writestr(ego->sock,wbuf);
	}
	
}


void help_commands (struct chatter * ego) {
	char wbuf[2048];
	sprintf(wbuf,"  .hl                              this message\n\r");
	writestr(ego->sock,wbuf);						
	sprintf(wbuf,"  .lo                              logout\n\r");
	writestr(ego->sock,wbuf);						
	sprintf(wbuf,"  .wo                              who's online\n\r");
	writestr(ego->sock,wbuf);						
	sprintf(wbuf,"  .wh <name> message               whisper to name\n\r");
	writestr(ego->sock,wbuf);						
	sprintf(wbuf,"  .ti                              server time\n\r");
	writestr(ego->sock,wbuf);						
	sprintf(wbuf,"  .cp                              change password\n\r");
	writestr(ego->sock,wbuf);						
	sprintf(wbuf,"  .tw                              toggle whispers\n\r");
	writestr(ego->sock,wbuf);						
	sprintf(wbuf,"  .tr                              toggle room\n\r");
	writestr(ego->sock,wbuf);						
	
}


// MAIN CLIENT ROUTINE

void * handle_client(void * argh) {
	int i; 
	int flag=0;
	char buf[256];
	char wbuf[2048];
	char check;
	struct chatter * ego;
	ego=(struct chatter *)argh;
	// get name and password information from client
	login(ego);
	// ENTER CLIENT ONTO CLIENTS LIST
	writer_lock();
	ego->next=head;
	head=ego;
	writer_unlock();
	// MAIN LOOP
	while (!flag) {
		await_input(ego->sock);
		if (readstr(ego->sock,buf)==0)
			flag=1;
			else {
			if (buf[0]!='.') {
				sprintf(wbuf,"[%s:%s] %s\n\r",ego->server,ego->name,buf);
				broadcast(wbuf); 
			} else { //COMMANDS
				check=buf[3];
				buf[3]=0;
				if (strcmp(buf,".lo")==0) flag=1;
				if (strcmp(buf,".wo")==0) wholist(ego);
				if (strcmp(buf,".hl")==0) help_commands(ego);
				if (strcmp(buf,".wh")==0) {
					buf[3]=check;
					whisper(ego,buf);
					buf[3]=0;
				}
				if (strcmp(buf,".ti")==0) {
					get_timestamp(wbuf);
					writestr(ego->sock,wbuf);
				}
				if (strcmp(buf,".cp")==0) {
					change_password(ego);
				}
				
				if (strcmp(buf,".tw")==0) {
					ego->whispers=1-ego->whispers;
					if (ego->whispers==1)
						sprintf(wbuf,"You now hear whispers.\n\r");
						else
						sprintf(wbuf,"You no longer hear whispers.\n\r");
					writestr(ego->sock,wbuf);
				}
				if (strcmp(buf,".tr")==0) {
					ego->room=1-ego->room;
					if (ego->room==1)
						sprintf(wbuf,"You now hear the room.\n\r");
						else
						sprintf(wbuf,"You no longer hear the room.\n\r");
					writestr(ego->sock,wbuf);
				}
			}
		}
	}
	//CLEANING UP	
	disconnect(ego);
}

int main() {
	int server;
	short port_num;
	int ok;
	struct chatter * current;
	fd_set readset;
	struct timeval timeout;
	readpassDB(passwdDB,"chatpasswd");
	port_num=5704;
	server=setup_post(port_num);
	int retval;
	printf("Server is launched, using port number %d.\n",port_num);
	while (server!=-1) {
		//printf("server loop, using fd %d\n",server);
		sched_yield();
		timeout.tv_sec=1;
		timeout.tv_usec=0;
		FD_ZERO(&readset);
		FD_SET(server,&readset);
		if ((retval=select(server+1,&readset,NULL,NULL,&timeout)==-1))
			{ perror("Select failed!\n"); printf("retval==%d errno==%d\n",retval,errno); exit(1); } 
			
		sched_yield();
		if (FD_ISSET(server,&readset)) {
			current=listening_post(server);
			if (current!=NULL) {
				printf("New connection from %s\n", current->server);
				//launch new thread which will handle client entirely
				ok=pthread_create(&(current->thread),NULL,handle_client,(void *) current);
				sched_yield(); // yield to give new child opportunity to copy ptr
				if (ok!=0) { perror("Could not create a thread!"); exit(1); }
				sched_yield(); // a second chance for child to copy ptr
			} else printf("no socket found\n");
		}
		sched_yield();
		// check for threads that need to be waited on
		if (pthread_mutex_trylock(&lock_done)==0) {
			while (done!=NULL) {
				if (pthread_join(done->thread,NULL)!=0)
					printf("No thread to join.\n");
					else
					printf("Thread has been joined.\n");
				current=done;
				done=done->next;
				free(current);
				}
			pthread_mutex_unlock(&lock_done);
			}
	} // end while loop
	return 0;
}
