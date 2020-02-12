#ifndef SERVER_H
#define SERVER_H

/******************************************************************************************
 * Server - Parent class for student's TCP server
 *
 *  	   Server(Const): right now does nothing (still should call for future portability
 *  	   ~Server(Dest): no cleanup required in parent
 *
 *  	   bind - binds to an IP address and port on the server
 *  	   listen - starts listening on a single-threaded process (non-blocking) and loops
 *                listening for connections and handling the existing connections
 *  	   shutdown - shuts down the server properly
 *  	   
 *  	   Exceptions: sub-classes should throw a std::exception with the what string field
 *  	               populated for any issues.
 *
 *****************************************************************************************/

class Server { 
   public:
      virtual ~Server();

      // Overload me!
      virtual void bindSvr(const char *ip_addr, unsigned short port) = 0;
      virtual void listenSvr() = 0;

      // if you overload, don't forget to call me
      virtual void shutdown() = 0;

   protected:

      // Do not forget, your constructor should call this constructor
      Server();

   private:
 

      // Stuff to be left alone
};

#endif
