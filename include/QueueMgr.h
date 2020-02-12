#ifndef QUEUEMGR_H
#define QUEUEMGR_H

#include <queue>
#include <vector>
#include <crypto++/secblock.h>
#include "TCPServer.h"

/*******************************************************************************************
 * QueueMgr - Child class of the TCPServer object, manages a Queue for a middleware/app
 *            server. Designed in a modular format. Messages are placed into the outgoing
 *            queue using sendToServer by Server ID or sendToAll to send to all servers in
 *            the list (Multicast flooding). 
 *             
 *            The handleQueue method is called by the management process, which looks for 
 *            new connections on the socket. These connections are accepted and authenticated,
 *            where they store their data until their data is moved into the queue for
 *            retrieval. 
 *            
 *            The pop function does two things. First, it "pops" (sends) incoming data to the
 *            management process and second, it assigns all outgoing data to a "Message
 *            Channel Agent", or TCPConn object.
 *
 *******************************************************************************************/
class QueueMgr : public TCPServer 
{
public:
   QueueMgr(unsigned int verbosity=1);
   virtual ~QueueMgr();

   void handleQueue();

   void populateQueue();

   // Pops a received queue element off the queue
   bool pop(std::string &sid, std::vector<uint8_t> &data);

   // Loads replication information into the Queue to transmit to servers
   void sendToAll(std::vector<uint8_t> &data);
   void sendToServer(const char *server_id, std::vector<uint8_t> &data);
   
   // Overload simply to remove this server from _server_list. Calls parent funct
   void bindSvr(const char *ip_addr, unsigned short port);


   // Gets the ID of this particular server
   const char *getServerID() { return _server_ID.c_str(); };

   // Get the number of servers we are replicating to
   unsigned int getNumServers() { return _server_list.size(); };

   // Looks up another server based off IP address and port
   const char *getClientID(unsigned long ip_addr, unsigned short port);

   // Overloaded to prevent this function from being used
   virtual void runServer();

private:

   // Launches a connection to the other server from queue data
   void launchDataConn(const char *sid, std::vector<uint8_t> &data);

   // Loads server information from servers.txt
   int loadServerList(const char *filename);

   // Set up our types for managing our queue
   enum qe_type {send, recv};
   struct queue_element {

      queue_element(qe_type in_type, const char *in_sid, std::vector<uint8_t> &in_data)
                  : type(in_type), server_id(in_sid), data(in_data) {}

      qe_type type;
      std::string server_id;
      std::vector<uint8_t> data;
   };

   std::string _server_ID;

   // The queue list
   std::queue<queue_element> _queue;

   std::vector<std::tuple<std::string, unsigned long, unsigned short>> _server_list;  
};


#endif
