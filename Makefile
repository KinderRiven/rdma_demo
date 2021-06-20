all: server client

server:
	g++ server.cc -o run_server -libverbs

client:
	g++ client.cc -o run_client -libverbs