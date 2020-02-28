#ifndef DRONEPLOTDB_H
#define DRONEPLOTDB_H

#include <list>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include "exceptions.h"


// Flags for the DronePlot object. The first two are already coded in and
// you can define more. It's based off bitwise and/or operations so just
// create a new one up to 0x128 
#define DBFLAG_NEW      0x1   // Was newly added to the database
#define DBFLAG_SYNCD    0x2   // Has been sync'd
#define DBFLAG_DUPE    0x4   // Skew has been calculated
#define DBFLAG_LEADER    0x8   // Is time coordinator
#define DBFLAG_USER3    0x16  // Change as needed
#define DBFLAG_USER4    0x32

// Manages the drone plot database for a particular node.
class DronePlot
{
public:
   DronePlot();
   DronePlot(int in_droneid, int in_nodeid, int in_timestamp, float in_latitude, float in_longitude);
   virtual ~DronePlot();

   // Function to serialize, or convert this data into a binary stream in a vector class and back
   void serialize(std::vector<uint8_t> &buf);
   void deserialize(std::vector<uint8_t> &buf, unsigned int start_pt = 0);

   // Reads and writes this plot to/from a buffer in comma-separated format
   int readCSV(std::string &buf);
   void writeCSV(std::string &buf);

   static size_t getDataSize();   // Num of bytes required to store the data (for serialization)
  
   // Flag manipulation -- pass in a define above as in setFlags(DBFLAG_NEW); 
   void setFlags(unsigned short flags);
   void clrFlags(unsigned short flags);
   bool isFlagSet(unsigned short flags); 

   // attributes - freely accessible to modify as needed 
   unsigned int drone_id;
   unsigned int node_id;
   time_t timestamp;
   float latitude;
   float longitude;
   
private:
   unsigned short _flags;

};


/**************************************************************************************************
 * DronePlotDB - class to manage a database of DronePlot objects, which manage drone GPS plots that
 *               are "received" by the antenna or another replication server
 *
 **************************************************************************************************/
class DronePlotDB 
{
public:
   DronePlotDB();
   virtual ~DronePlotDB();

   // Add a plot to the database with the given attributes (mutex'd)
   void addPlot(int drone_id, int node_id, time_t timestamp, float lattitude, float longitude);

   // Load or write the database to/from a CSV file, 
   int loadCSVFile(const char *filename);
   int writeCSVFile(const char *filename);

   // Direct binary load/write to/from the specified file
   int loadBinaryFile(const char *filename);
   int writeBinaryFile(const char *filename);
   
   // Sort the database in order of timestamp 
   void sortByTime();

   // Remove all plotpoints of a particular node (used to generate binary, not for student use)
   void removeNodeID(unsigned int node_id);

   // Iterators for simple access to the database. Can use these to modify drone plot points
   // but won't be able to add/delete PlotObjects. Use erase (below) for that as it is mutex'd
   std::list<DronePlot>::iterator begin() { return _dbdata.begin(); };
   std::list<DronePlot>::iterator end() { return _dbdata.end(); };
   
   // Manipulate database entries (mutex'd functions)
   void popFront();
   void erase(unsigned int i);
   std::list<DronePlot>::iterator erase(std::list<DronePlot>::iterator dptr);


   // Return the number of plot points stored
   size_t size() { return _dbdata.size(); };

   // Wipe the database
   void clear();

private:
   std::list<DronePlot> _dbdata;

   pthread_mutex_t _mutex; 
};


#endif
