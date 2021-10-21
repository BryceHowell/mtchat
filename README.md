# mtchat
multi-threaded chat server 

(It uses posix threads.)

# compilation
compile it with 

> $ g++ -o mtchat mtchat.c -lpthread

# execution
The chat server will have its server socket on port 5704. 
To try it, use a telnet client to that port on loopback 127.0.0.1
There are several commands, all starting with a period.
They may be listed with the .hl command.
When you first connect, the server will ask you for a name, and if it is not in the database already (chatpasswd), it will ask you to provide a password.
Otherwise you will be prompted to provide the correct password for that nickname.
