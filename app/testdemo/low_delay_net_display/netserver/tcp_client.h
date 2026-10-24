#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H
#include <memory>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include "../logger/log.h"
#include <unistd.h>
#include <vector>
#include <sys/types.h>
#include <thread>     // for std::thread
#include <vector>     // for std::vector
using namespace std;
using TcpRecvCallBack =
    add_pointer<void(char *buffer, int size)>::type;
class TCPClient
{
private:
    int sock;
    std::string address;
    int port;
    struct sockaddr_in server;
    int quit_;
public:
    TCPClient();
    virtual ~TCPClient();
    bool Setup(string address, int port);
    bool Send(string data);
    int Send(char *buff, int size);
    string Receive(int size);
    int Recvieve();
    int Receive(char *buff, int size);
    TcpRecvCallBack callback_;
    std::shared_ptr<std::thread> recv_thread;
    void RegisterRecvCallBack(TcpRecvCallBack cb)
    {
        callback_ = cb;
    }
    void UnRegisterRecvCallBack()
    {
        callback_ = nullptr;
    }
    void SaveEixt();
};

#endif
