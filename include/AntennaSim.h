#ifndef ANTENNASIM_H
#define ANTENNASIM_H

#include <list>
#include <unistd.h>
#include "exceptions.h"
#include "DronePlotDB.h"

// Simulates an antenna receiving drone information and populates the DronePlotDB class as it "receives"
// information. 
//
// Students should not change anything with this class

class AntennaSim 
{
public:
   AntennaSim(DronePlotDB &dpdb, const char *source_filename, float time_mult,
               int verbosity);
   virtual ~AntennaSim();

   // Load the data that will be fed to the accessible DB to simulate drone updates
   void loadSourceDB(const char *filename);

   // Run the simulation (usually in a thread)
   void simulate();

   // Terminate the simulation (and probably exit the thread)
   void terminate() { _exiting = true; };

   // Are we in the process of exiting the simulation?
   bool isExiting() { return _exiting; };

   int getOffset() { return _time_offset; };

private:
   
   double getAdjustedTime();

   // Simulation checks periodically to know when to exit the thread
   bool _exiting;

   DronePlotDB &_to_db;
   DronePlotDB _source_db;

   float _time_mult;
   int _time_offset;
   int _verbosity;
 
   time_t _start_time;
};


#endif
