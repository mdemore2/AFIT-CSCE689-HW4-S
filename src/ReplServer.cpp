#include <iostream>
#include <tuple>
#include <exception>
#include "ReplServer.h"

const time_t secs_between_repl = 20;
const unsigned int max_servers = 10;

/*********************************************************************************************
 * ReplServer (constructor) - creates our ReplServer. Initializes:
 *
 *    verbosity - passes this value into QueueMgr and local, plus each connection
 *    _time_mult - how fast to run the simulation - 2.0 = 2x faster
 *    ip_addr - which ip address to bind the server to
 *    port - bind the server here
 *
 *********************************************************************************************/
ReplServer::ReplServer(DronePlotDB &plotdb, float time_mult)
                              :_queue(1),
                               _plotdb(plotdb),
                               _shutdown(false), 
                               _time_mult(time_mult),
                               _verbosity(1),
                               _ip_addr("127.0.0.1"),
                               _port(9999)
{
   _start_time = time(NULL);
}

ReplServer::ReplServer(DronePlotDB &plotdb, const char *ip_addr, unsigned short port, int offset, 
                        float time_mult, unsigned int verbosity)
                                 :_queue(verbosity),
                                  _plotdb(plotdb),
                                  _shutdown(false), 
                                  _time_mult(time_mult), 
                                  _verbosity(verbosity),
                                  _ip_addr(ip_addr),
                                  _port(port)

{
   _start_time = time(NULL) + offset;
}

ReplServer::~ReplServer() {

}


/**********************************************************************************************
 * getAdjustedTime - gets the time since the replication server started up in seconds, modified
 *                   by _time_mult to speed up or slow down
 **********************************************************************************************/

time_t ReplServer::getAdjustedTime() {
   return static_cast<time_t>((time(NULL) - _start_time) * _time_mult);
}

/**********************************************************************************************
 * replicate - the main function managing replication activities. Manages the QueueMgr and reads
 *             from the queue, deconflicting entries and populating the DronePlotDB object with
 *             replicated plot points.
 *
 *    Params:  ip_addr - the local IP address to bind the listening socket
 *             port - the port to bind the listening socket
 *             
 *    Throws: socket_error for recoverable errors, runtime_error for unrecoverable types
 **********************************************************************************************/

void ReplServer::replicate(const char *ip_addr, unsigned short port) {
   _ip_addr = ip_addr;
   _port = port;
   replicate();
}

void ReplServer::replicate() {

   // Track when we started the server
   _start_time = time(NULL);
   _last_repl = 0;

   // Set up our queue's listening socket
   _queue.bindSvr(_ip_addr.c_str(), _port);
   _queue.listenSvr();

   if (_verbosity >= 2)
      std::cout << "Server bound to " << _ip_addr << ", port: " << _port << " and listening\n";

  
   // Replicate until we get the shutdown signal
   while (!_shutdown) {

      // Check for new connections, process existing connections, and populate the queue as applicable
      _queue.handleQueue();     

      // See if it's time to replicate and, if so, go through the database, identifying new plots
      // that have not been replicated yet and adding them to the queue for replication
      if (getAdjustedTime() - _last_repl > secs_between_repl) {

         queueNewPlots();
         _last_repl = getAdjustedTime();
      }
      
      // Check the queue for updates and pop them until the queue is empty. The pop command only returns
      // incoming replication information--outgoing replication in the queue gets turned into a TCPConn
      // object and automatically removed from the queue by pop
      std::string sid;
      std::vector<uint8_t> data;
      while (_queue.pop(sid, data)) {

         // Incoming replication--add it to this server's local database
         addReplDronePlots(data);         
      }       

      //sort through database, check for skew and duplicates here
      _plotdb.sortByTime();
      bool run = checkSkew();
      while(!run){run = checkSkew();}
      correctSkew();
      run = deduplicate();
      while(!run){run = deduplicate();}
    

      usleep(1000);
   }   
}

/**********************************************************************************************
 * queueNewPlots - looks at the database and grabs the new plots, marshalling them and
 *                 sending them to the queue manager
 *
 *    Returns: number of new plots sent to the QueueMgr
 *
 *    Throws: socket_error for recoverable errors, runtime_error for unrecoverable types
 **********************************************************************************************/

unsigned int ReplServer::queueNewPlots() {
   std::vector<uint8_t> marshall_data;
   unsigned int count = 0;

   if (_verbosity >= 3)
      std::cout << "Replicating plots.\n";

   // Loop through the drone plots, looking for new ones
   std::list<DronePlot>::iterator dpit = _plotdb.begin();
   for ( ; dpit != _plotdb.end(); dpit++) {

      // If this is a new one, marshall it and clear the flag
      if (dpit->isFlagSet(DBFLAG_NEW)) {
         
         if(!dpit->isFlagSet(DBFLAG_DUPE))
         {
            dpit->serialize(marshall_data);
         }
         dpit->clrFlags(DBFLAG_NEW);

         count++;
      }
      if (marshall_data.size() % DronePlot::getDataSize() != 0)
         throw std::runtime_error("Issue with marshalling!");

   }
  
   if (count == 0) {
      if (_verbosity >= 3)
         std::cout << "No new plots found to replicate.\n";

      return 0;
   }
 
   // Add the count onto the front
   if (_verbosity >= 3)
      std::cout << "Adding in count: " << count << "\n";

   uint8_t *ctptr_begin = (uint8_t *) &count;
   marshall_data.insert(marshall_data.begin(), ctptr_begin, ctptr_begin+sizeof(unsigned int));

   // Send to the queue manager
   if (marshall_data.size() > 0) {
      _queue.sendToAll(marshall_data);
   }

   if (_verbosity >= 2) 
      std::cout << "Queued up " << count << " plots to be replicated.\n";

   return count;
}

/**********************************************************************************************
 * addReplDronePlots - Adds drone plots to the database from data that was replicated in. 
 *                     Deconflicts issues between plot points.
 * 
 * Params:  data - should start with the number of data points in a 32 bit unsigned integer, 
 *                 then a series of drone plot points
 *
 **********************************************************************************************/

void ReplServer::addReplDronePlots(std::vector<uint8_t> &data) {
   if (data.size() < 4) {
      throw std::runtime_error("Not enough data passed into addReplDronePlots");
   }

   if ((data.size() - 4) % DronePlot::getDataSize() != 0) {
      throw std::runtime_error("Data passed into addReplDronePlots was not the right multiple of DronePlot size");
   }

   // Get the number of plot points
   unsigned int *numptr = (unsigned int *) data.data();
   unsigned int count = *numptr;

   // Store sub-vectors for efficiency
   std::vector<uint8_t> plot;
   auto dptr = data.begin() + sizeof(unsigned int);

   for (unsigned int i=0; i<count; i++) {
      plot.clear();
      plot.assign(dptr, dptr + DronePlot::getDataSize());
      addSingleDronePlot(plot);
      dptr += DronePlot::getDataSize();      
   }
   if (_verbosity >= 2)
      std::cout << "Replicated in " << count << " plots\n";   
}


/**********************************************************************************************
 * addSingleDronePlot - Takes in binary serialized drone data and adds it to the database. 
 *
 **********************************************************************************************/

void ReplServer::addSingleDronePlot(std::vector<uint8_t> &data) {
   DronePlot tmp_plot;

   tmp_plot.deserialize(data);
   
   
   _plotdb.addPlot(tmp_plot.drone_id, tmp_plot.node_id, tmp_plot.timestamp, tmp_plot.latitude,
                                                         tmp_plot.longitude);
}


void ReplServer::shutdown() {
   _shutdown = true;
}


//check for time skew and map skew to node_id
bool ReplServer::checkSkew(){
     
   auto priorityOrder = _queue.getLeader();

   for(auto i = _plotdb.begin(); i != _plotdb.end(); i++)
   {
         if(priorityOrder.front() == ("ds" + std::to_string(i->node_id)))
         {
           i->setFlags(DBFLAG_LEADER);
         
         }

         for(auto j = i++; j != _plotdb.end(); j++)
         {
            
            if(priorityOrder.front() == ("ds" + std::to_string(j->node_id)))
            {
               j->setFlags(DBFLAG_LEADER);
            }
            
            if((i->latitude == j->latitude) && (i->longitude == j->longitude))
            {
               if(i->timestamp != j-> timestamp)//same place, different time -- calculate skew if possible
               {
                  if((_skew.find(i->node_id) != _skew.end()) && (_skew.find(j->node_id) == _skew.end()))
                  {
                     _skew.emplace(j->node_id,((_skew[i->node_id] + i->timestamp) - j->timestamp));
                  }
                  else if((_skew.find(i->node_id) == _skew.end()) && (_skew.find(j->node_id) != _skew.end()))
                  {
                     _skew.emplace(i->node_id,((_skew[j->node_id] + j->timestamp) - i->timestamp));
                  }
                  else if(i->isFlagSet(DBFLAG_LEADER))
                  {
                     _skew.emplace(j->node_id,(i->timestamp - j->timestamp));
                  }
                  else if(j->isFlagSet(DBFLAG_LEADER))
                  {
                     _skew.emplace(i->node_id,(j->timestamp - i->timestamp));
                  }
                  
               }
               else //same place, same time -- delete node with lower priority
               {
                  for(auto it = priorityOrder.cbegin(); it != priorityOrder.cend(); it++)
                  {
                     if(*it == ("ds" + std::to_string(i->node_id)))
                     {
                        //set flag for j
                        _plotdb.erase(j);
                        return false;
                     }
                     else if(*it == ("ds" + std::to_string(j->node_id)))
                     {
                        _plotdb.erase(i);
                        return false;
                        
                     }
                  }
               }
               if(_plotdb.size() < 1)
               {
                  break;
               }
               
            }
            
         }
         if(_plotdb.size() < 1)
               {
                  break;
               }
   }
   return true;
}

void ReplServer::correctSkew(){
   //if node is skewed, fix time

   for(auto i = _plotdb.begin(); i != _plotdb.end(); i++)
   {
      if(_skew.find(i->node_id) != _skew.end())
      {
         
         i->timestamp += _skew[i->node_id];
            
      }
   }

}

//delete duplicate points
bool ReplServer::deduplicate(){

   auto priorityOrder = _queue.getLeader();

   for(auto i = _plotdb.begin(); i != _plotdb.end(); i++)
   {

      for(auto j = i++; j != _plotdb.end(); j++)
      {
         if(i->timestamp == j->timestamp) //if point is duplicate, delete lower priority
         {
            for(auto it = priorityOrder.begin(); it != priorityOrder.end(); it++)
            {
               if(*it == ("ds" + std::to_string(i->node_id)))
               {
                  _plotdb.erase(j);
                  return false;
               }
               else if(*it == ("ds" + std::to_string(j->node_id)))
               {
                 _plotdb.erase(i);
                  return false;
               }
            }
         }
        if(_plotdb.size() < 1)
               {
                  break;
               }
         
      }
      if(_plotdb.size() < 1)
               {
                  break;
               }
   }

   return true;
}