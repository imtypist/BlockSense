#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#include "Archiver.h"

#define DEFAULT_PORT "4242"

using namespace std;

class Network : public Archiver {
public:
	~Network();
	void startServer();
	void startClient(const string server_ip);

	void write(unsigned char *buf, int len);
	void read(unsigned char *buf, int len);

private:
	WSADATA wsaData;

	SOCKET Socket;
};
