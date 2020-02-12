/****************************************************************************************
 * csv2bin_main - reads in a csv file with drone information and saves it as a binary file
 *
 *              **Students should not modify this code! Or at least you can to test your
 *                code, but your code should work with the unmodified version
 *
 ****************************************************************************************/  

#include <stdexcept>
#include <iostream>
#include "FileDesc.h"
#include "DronePlotDB.h"
#include "strfuncts.h"

using namespace std; 

void displayHelp(const char *execname) {
   std::cout << execname << " <input file> <output file> <NodeID>\n";
//   std::cout << "   t: maximum number of threads to use\n";
//   std::cout << "   n: calculate primes up to the given range\n";
//   std::cout << "   s: only run in single process mode\n";
//   std::cout << "   m: only run in multithreaded mode\n";
//   std::cout << "   w: skip writing to disk\n";
}


int main(int argc, char *argv[]) {

   // Check the command line input
   if (argc < 4) {
      displayHelp(argv[0]);
      exit(0);
   }

   // Get the filenames for the input and output file
   std::string input_file(argv[1]);
   std::string output_file(argv[2]);
   
   unsigned long node_id = strtol(argv[3], NULL, 10);

   std::cout << "Filtering to only node: " << node_id << "\n";

   std::cout << "Reading in the CSV file.";

   DronePlotDB db;
   int count = 0;
   if ((count = db.loadCSVFile(input_file.c_str())) < 0) {
      std::cerr << "Either failed opening file for reading or file was corrupted.\n";
      exit(-1);
   }

   // Filter by NodeID
   for (unsigned int i=1; i<=3; i++) {
      if (i != node_id)
         db.removeNodeID(i);
   }

   if (count == 0) {
      std::cout << "No data points in the file. Exiting without writing to output file.\n";
      exit(0);
   }

   std::cout << "Read in " << count << " drone data points successfully.\n";
   std::cout << "Size: " << db.size() << "\n";

   std::cout << "Writing to: " << output_file.c_str() << "\n";
   if (db.writeBinaryFile(output_file.c_str()) < 0) {
      std::cerr << "Unable to open output file for writing.\n";
      exit(-1);
   }

   std::cout << "Wrote " << db.size() << " drone data points\n";
   db.clear();

   // Test the functions
   db.loadBinaryFile(output_file.c_str());

   std::cout << "Num: " << db.size() << "\n";
   db.writeCSVFile("diditwork.csv");
   
   return 0;
}
