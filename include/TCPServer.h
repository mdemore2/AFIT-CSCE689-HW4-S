#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <list>
#include <memory>
#include "Server.h"
#include "FileDesc.h"
#include "TCPConn.h"
#include "LogMgr.h"
#include <crypto++/secblock.h>

/********************************************************************************************
 * TCPServer - Basic functionality to manage a server socket and a list of connections. 
 *             Includes functionality to manage an AES encryption key loaded from file.
 *
 *             handleConnection is the primary maintenance function. Calls all the TCPConn
 *             handleConnection functions. 
 ********************************************************************************************/

const time_t reconnect_delay = 5;

class TCPServer : public Server 
{
public:
   TCPServer(unsigned int _verbosity = 1);
   virtual ~TCPServer();

   virtual void bindSvr(const char *ip_addr, unsigned short port);
   void listenSvr();
   virtual void runServer();

   void shutdown();

   TCPConn *handleSocket();
   virtual void handleConnections();

   unsigned long getIPAddr() { return _sockfd.getIPAddr(); };
   unsigned short getPort() { return _sockfd.getPort(); };

   // Change where the log file is writing to
   void changeLogfile(const char *newfile);

protected:

   void loadAESKey(const char *filename);

   // List of TCPConn objects to manage connections
   std::list<std::unique_ptr<TCPConn>> _connlist;

   CryptoPP::SecByteBlock _aes_key;

   LogMgr _server_log;

   unsigned int _verbosity;

private:
   // Class to manage the server socket
   SocketFD _sockfd;

};


#endif
