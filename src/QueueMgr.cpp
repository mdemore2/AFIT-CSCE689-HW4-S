#include <fstream>
#include <arpa/inet.h>
#include <tuple>
#include <sstream>
#include <crypto++/osrng.h>
#include <crypto++/filters.h>
#include <crypto++/files.h>
#include "strfuncts.h"
#include "ReplServer.h"
#include "TCPConn.h"

/********************************************************************************************
 * QueueMgr (constructor) - loads a hard-coded server.txt that contains a comma-separated list
 *                          of server info (including this one)
 *
 ********************************************************************************************/

QueueMgr::QueueMgr(unsigned int verbosity):TCPServer(verbosity)
               
{
   if (loadServerList("servers.txt") <= 0)
      throw std::runtime_error("Could not open server.txt file, or file was empty/corrupt.");

   loadAESKey("sharedkey.bin");
}

// Destructor - does nothing right now
QueueMgr::~QueueMgr() {

}

// Should not be called, overloaded to crash if it is
void QueueMgr::runServer() {
   throw std::runtime_error("runServer function used on QueueMgr object (should not be)");
}

/*********************************************************************************************
 * loadServerList - loads the list of other replication servers from the file given in
 *                  the parameter. Deconflicts the local server
 *
 *    Params:  filename - the path/filename to the server file in the following format:
 *                   <server_id>, <ip_addr>, <port>
 *
 *    Returns: -1 for failure, # of servers opened for success
 *
 *    Throws: socket_error for any network issues
 *********************************************************************************************/
int QueueMgr::loadServerList(const char *filename) {
   std::ifstream sfile;
   unsigned int count = 0;

   sfile.open(filename, std::ifstream::in);
   if (!sfile.is_open())
      return -1;

   std::string buf, left, right;
   std::string svrid;
   while (!sfile.eof()) {
      std::getline(sfile, buf);
      clrNewlines(buf);
      if (buf.size() == 0)
         break;

      if (!split(buf, left, right, ','))
         return -1;
      clrSpaces(left);
      svrid = left;
   
      buf = right;   
      if (!split(buf, left, right, ','))
         return -1;

      clrSpaces(left);
      clrSpaces(right);
   
      in_addr ipaddr;
      inet_pton(AF_INET, left.c_str(), &ipaddr);

      unsigned short port;
      port = (unsigned short) strtol(right.c_str(), NULL, 10);
      port = htons(port);
      
      _server_list.push_back(std::tuple<std::string, unsigned long, 
                                             unsigned long>(svrid, ipaddr.s_addr, port));
      count++;     
   }
   return count;
}

/**********************************************************************************************
 * getClientID - Gets the server ID based on the IP address and port in the lookup table
 *
 *    Params:  ip_addr - host's IP address in network format
 *             port - host's port in network format
 *
 *    Throws: socket_error for recoverable errors, runtime_error for unrecoverable types
 **********************************************************************************************/

const char *QueueMgr::getClientID(unsigned long ip_addr, short unsigned int port) {
   auto sl_iter = _server_list.begin();
   for ( ; sl_iter != _server_list.end(); sl_iter++) {
      if ((std::get<1>(*sl_iter) == ip_addr) && (std::get<2>(*sl_iter) == port)) {
         return std::get<0>(*sl_iter).c_str();
      }
   }
   return NULL;
}


/**********************************************************************************************
 * bindSvr - Creates a network socket and sets it nonblocking so we can loop through looking for
 *           data. Then binds it to the ip address and port
 *
 *    Throws: socket_error for recoverable errors, runtime_error for unrecoverable types
 **********************************************************************************************/

void QueueMgr::bindSvr(const char *ip_addr, short unsigned int port) {
   // Call the parent function
   TCPServer::bindSvr(ip_addr, port);
   bool found = false;

   // Now remove this server from the server list (if it's in there)
   auto sliter = _server_list.begin();
   while (sliter != _server_list.end()) {

      // If we found our server, clear it from the list
      if ((std::get<1>(*sliter) == getIPAddr()) && (std::get<2>(*sliter) == htons(getPort()))){
         _server_ID = std::get<0>(*sliter);
         sliter = _server_list.erase(sliter);
         found = true;
      }
      else
         sliter++;
   }

   // If we never found our server, that's a problem--crash out
   if (!found) {
      std::stringstream msg;
      msg << "Server at " << ip_addr << " port " << port << " not listed in servers.txt file.";
      throw std::runtime_error(msg.str().c_str());
   }

   // Now re-open the server log with the server ID info
   std::string logname = getServerID();
   logname += "server.log";
   changeLogfile(logname.c_str()); 
   _server_log.writeLog("Server started.");
}


/*********************************************************************************************
 * handleQueue - runs through a cycle on the queue, accepting new connections and handling
 *               any data read from the connections, storing it in the connection buffer
 *               for later retrieval. 
 *
 *    Throws: socket_error for any network issues
 *********************************************************************************************/
void QueueMgr::handleQueue() {

   // Accept new connections, if any
   handleSocket();

   // Handle any open connections, reading from and writing to the socket
   handleConnections();
   
   // Get data from input buffers on connections and add to the queue
   populateQueue();

}

/**********************************************************************************************
 * populateQueue - Gets the information from the connections and populates them into the queue
 *                 for handling later
 *
 *    Throws: socket_error for recoverable errors, runtime_error for unrecoverable types
 **********************************************************************************************/
void QueueMgr::populateQueue() {

   // Loop through the connections, handling each one
   auto conn_it = _connlist.begin();
   for ( ; conn_it != _connlist.end(); conn_it++) {
      
      // If the connection has data marked ready, get it and handle it based on the
      // command at the beginning
      if (((*conn_it)->getStatus() == TCPConn::s_hasdata) && (*conn_it)->isInputDataReady()) {
         std::vector<uint8_t> buf;

         (*conn_it)->getInputData(buf);
         if (buf.size() == 0) {
            // Handle this better later on
            throw std::runtime_error("TCPConn claimed replication data but none existed.");
         }
        
         // Add this data to the queue
         _queue.emplace(recv, (*conn_it)->getNodeID(), buf);
         if (_verbosity >= 3) {
            std::cout << "Replication info pulled off connection and placed into queue w/ " <<
                              (buf.size()-4) / DronePlot::getDataSize() << " potential plots.\n";
         }   
      }      
   }
}

/*********************************************************************************************
 * replToAll - places data into the queue for each server (calls replToServer). Replication 
               will happen on its own
 *
 *    Params:  data - the data in binary form to send to the server
 *
 *    Throws: socket_error for any network issues
 *********************************************************************************************/
void QueueMgr::sendToAll(std::vector<uint8_t> &data) {
   for (unsigned int i=0; i<_server_list.size(); i++) {
      sendToServer(std::get<0>(_server_list[i]).c_str(), data);
   }

}

/*********************************************************************************************
 * sendToServer - places data into the queue to be sent to the server indicated by
 *                server_id. Transmission will happen on its own
 *
 *    Params:  server_id - string of the server's name (will be mapped automatically to IP)
 *             data - the data in binary form to send to the server
 *
 *    Throws: socket_error for any network issues
 *********************************************************************************************/
void QueueMgr::sendToServer(const char *server_id, std::vector<uint8_t> &data) {
   _queue.emplace(send, server_id, data);

}

/*********************************************************************************************
 * pop - removes the next received data element sitting in the queue and returns the data 
 *       loaded into the parameters. Also assigns outgoing queue elements to a connection
 *       automatically and starts that connection going
 *
 *    Params:  sid - pop action places the first recv'd pop server id into this attribute
 *             data - data received gets loaded into this vector
 *
 *    Returns: true for an incoming element found, false otherwise. Returns false even if
 *             outgoing connections are found in the process 
 *
 *    Throws: socket_error for any network issues
 *********************************************************************************************/
bool QueueMgr::pop(std::string &sid, std::vector<uint8_t> &data) {
   while (_queue.size() > 0) {
      auto next_qe = _queue.front();

      // If this a send item, create a connection and start sending
      if (next_qe.type == send) {

         // Set up the connection and attempt to establish link (will retry if failure)
         launchDataConn(next_qe.server_id.c_str(), next_qe.data);

         _queue.pop();
         continue;  
      }

      sid = next_qe.server_id;
      data = std::move(next_qe.data);
      _queue.pop();
      return true;
   }
   return false;
}

/*********************************************************************************************
 * launchDataConn - launches a connection and starts the process of sending the queue data to
 *                  the target server
 *
 *    Params:  sid - pop action places the first recv'd pop server id into this attribute
 *             data - data received gets loaded into this vector
 *
 *********************************************************************************************/
void QueueMgr::launchDataConn(const char *sid, std::vector<uint8_t> &data) {

   unsigned long ip_addr;
   unsigned short port;

   // Find the IP address of the destination server
   unsigned int i;
   for (i=0; i<_server_list.size(); i++) {
      if (!std::get<0>(_server_list[i]).compare(sid)) {
         ip_addr = std::get<1>(_server_list[i]);
         port = std::get<2>(_server_list[i]);
         break;
      }
   }

   if (i==_server_list.size()) {
      throw std::runtime_error("Attempt to send data to server ID not in the server list.");
   }

   // Try to connect to the server and if there's an issue, delete and re-throw socket_error
   TCPConn *new_conn = new TCPConn(_server_log, _aes_key, _verbosity);
   new_conn->setNodeID(sid);
   new_conn->setSvrID(getServerID());

   try {
      new_conn->connect(ip_addr, port);
   } catch (socket_error &e) {
      std::stringstream msg;
      msg << "Connect to SID " << sid << " failed when trying to send data. Retrying. Msg: " <<
                        e.what();
      _server_log.writeLog(msg.str().c_str());
      new_conn->disconnect();
      new_conn->reconnect = time(NULL) + reconnect_delay;  // Try again in 5 seconds, real-world
   }


   new_conn->assignOutgoingData(data);
   _connlist.push_back(std::unique_ptr<TCPConn>(new_conn));
}

