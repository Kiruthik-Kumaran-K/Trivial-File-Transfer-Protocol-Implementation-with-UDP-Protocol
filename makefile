CC = gcc
# Note: I removed file.c from SOURCES as it is no longer in your root folder.
SOURCES = udp.c tftp.c 
HEADERS = tftp.h udp.h

all: client_dir/client server_dir/server

# Target: Dependencies (Make will rebuild if any of these files change)
server_dir/server: server.c $(SOURCES) $(HEADERS)
	$(CC) server.c $(SOURCES) -o server_dir/server

client_dir/client: client.c $(SOURCES) $(HEADERS)
	$(CC) client.c $(SOURCES) -o client_dir/client

# These shortcut rules let you still type "make server" or "make client"
.PHONY: all clean server client

server: server_dir/server
client: client_dir/client

clean:
	rm -f client_dir/client
	rm -f server_dir/server

