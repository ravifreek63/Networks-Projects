all: server client proxy
	
server: header.h server.cpp    
	g++ -o server server.cpp  -lpthread -std=gnu++0x

client: header.h client.cpp
	g++ -o client client.cpp  -lpthread -std=gnu++0x

proxy: header.h proxyMultiThreaded.cpp
	g++ -o proxy proxyMultiThreaded.cpp  -lpthread -std=gnu++0x
	
