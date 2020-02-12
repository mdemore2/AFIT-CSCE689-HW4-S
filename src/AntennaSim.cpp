#include <iostream>
#include "AntennaSim.h"
#include "DronePlotDB.h"

/*****************************************************************************************
 * AntennaSim (constructor) - takes in a reference to the accessible database that will be
 *            populated by the simulator
 *
 *    Params:  dpdb - a reference to the operational database to inject into
 *             time_mult - how fast to run the injects (2.0 = 2x faster)
 *****************************************************************************************/
AntennaSim::AntennaSim(DronePlotDB &dpdb, const char *source_filename, float time_mult, 
                       int verbosity): 
                                             _to_db(dpdb),
                                             _time_mult(time_mult),
                                             _time_offset(0),
                                             _verbosity(verbosity),
                                             _start_time(0)
{
   if (_verbosity == 3)
      std::cout << "SIM: Loading source database: " << source_filename << "\n";

   if (_source_db.loadBinaryFile(source_filename) <= 0)
      throw std::runtime_error("Source database could not be opened or was empty.");

   if (_verbosity >= 2)
      std::cout << "SIM: Source database " << source_filename << " successfully loaded.\n";
   if (_verbosity >= 1)
      std::cout << "SIM: simulation started, time multiplier: " << _time_mult << "\n";
}

// Constructor - no action right now
AntennaSim::~AntennaSim() {

}

/*****************************************************************************************
 * loadSourceDB - Loads in the source file in binary format
 *
 *****************************************************************************************/

void AntennaSim::loadSourceDB(const char *filename) {

   // Load our drone plots to feed to the accessible database
   int results = _source_db.loadBinaryFile(filename);

   if (results < 0)
      throw std::runtime_error("Unable to load the source data file for the simulator.");
   if (results == 0)
      throw std::runtime_error("Source data file for simulator was empty.");

}

double AntennaSim::getAdjustedTime() {
   return static_cast<time_t>(((double) time(NULL) - (double) _start_time) * _time_mult);
}

/*****************************************************************************************
 * simulate - process that manages the simulation that feeds data into the student's database
 *            to simulate receiving drone data
 *
 *****************************************************************************************/

void AntennaSim::simulate() {

   // Sort the database by time   
   _source_db.sortByTime();

   // Set up a random offset between 1 and 3 seconds from true
   srand(time(NULL));
   _time_offset = (rand() % 6) - 3;
   if (_verbosity >= 2) 
      std::cout << "SIM: Simulator time offset: " << _time_offset << " secs\n";

   if (_verbosity >= 1)
      std::cout << "SIM: Delaying 3 seconds before starting sim to let servers come online.\n";

   // Provide a short 3 second delay before starting
   for (unsigned int i=3; i>0; i--) {
      if (_verbosity >= 2)
         std::cout << i << "\n";
      sleep(1);
   }

   // Initialize
   _start_time = time(NULL);

   timespec sleeptime;
   std::list<DronePlot>::iterator diter;

   // Change all the inject timestamps to the offset time
   for (diter = _source_db.begin(); diter != _source_db.end(); diter++) {
      diter->timestamp += _time_offset;
   }
   
   // Loop through the injects, sending them as their time arrives
   while (_source_db.size() > 0) {

      // Get the time until our next inject
      diter = _source_db.begin();
      double adjusted_time = getAdjustedTime();

      // If the adjusted time is not past the timestamp on our next inject, sleep until it is
      if (adjusted_time < (double) diter->timestamp) {

         // We need a more precise adjusted time

         // timespan = how long we need to sleep (add .55 secs to prevent rounding down issues
         double timespan = ((double) diter->timestamp - adjusted_time) / _time_mult + .55;
         sleeptime.tv_sec = (time_t) timespan;
         sleeptime.tv_nsec = (long) (1000000000.0 * (timespan - (double) sleeptime.tv_sec));
         
         if (_verbosity == 3)
            std::cout << "SIM: Sim sleeping for " << sleeptime.tv_sec << " secs, " << sleeptime.tv_nsec
                                                                        << " nanosecs\n";
         nanosleep(&sleeptime, NULL);
      }
      
      // Now inject all that have a timestamp less than the current time
      adjusted_time = getAdjustedTime();
      diter = _source_db.begin();

      if (_verbosity >= 2)
            std::cout << "SIM: Cur systime: " << (time_t) getAdjustedTime() << "\n";

      while ((_source_db.size() > 0) && (diter->timestamp <= adjusted_time)) {
        
         if (_verbosity >= 1)
            std::cout << "SIM: Injecting plot NodeID: " << diter->node_id << " DroneID: " << 
                  diter->drone_id << ", Time: " << diter->timestamp << " Lat: " << 
                  diter->latitude << ", Long: " << diter->longitude << "\n";

         _to_db.addPlot(diter->drone_id, diter->node_id, diter->timestamp, diter->latitude, diter->longitude);
         diter = _to_db.end();
         diter--;
         diter->setFlags(DBFLAG_NEW);

         _source_db.popFront();
         diter = _source_db.begin();
      }
   }
   
   if (_verbosity >= 2) {
      std::cout << "SIM: Drone plot injections complete.\n";
   }


}
