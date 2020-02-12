/****************************************************************************************
 * keygen_main - generates an AES key and stores it in the sharedkey file
 *
 *              **Students should not modify this code! Or at least you can to test your
 *                code, but your code should work with the unmodified version
 *
 ****************************************************************************************/  

#include <stdexcept>
#include <iostream>
#include <crypto++/rijndael.h>
#include <crypto++/osrng.h>
#include <crypto++/filters.h>
#include <crypto++/files.h>
#include "FileDesc.h"
#include "strfuncts.h"

using namespace std;
using namespace CryptoPP; 

void displayHelp(const char *execname) {
   std::cout << execname << " <output file>\n";
//   std::cout << "   t: maximum number of threads to use\n";
//   std::cout << "   n: calculate primes up to the given range\n";
//   std::cout << "   s: only run in single process mode\n";
//   std::cout << "   m: only run in multithreaded mode\n";
//   std::cout << "   w: skip writing to disk\n";
}


int main(int argc, char *argv[]) {

   // Check the command line input
   if (argc < 2) {
      displayHelp(argv[0]);
      exit(0);
   }

   AutoSeededRandomPool rnd;

   // Generate a random key
   SecByteBlock key(0x00, AES::DEFAULT_KEYLENGTH);
   rnd.GenerateBlock(key, key.size());

   // Generate random initialization vector
//   SecByteBlock iv(AES::BLOCKSIZE);
//   rnd.GenerateBlock(iv, iv.size());

   
   std::vector<uint8_t> keybytes(key.begin(), key.end());

   ArraySource outfile(key.begin(), key.size(), true, new FileSink(argv[1]));
/* 
   FileFD outfile(argv[1]);
   if (!outfile.openFile(FileFD::writefd, true))
   {
      std::cerr << "Failed to open file " << argv[1] << " to write the key into.\n";
      exit(0); 
   }

   outfile.writeBytes<uint8_t>(keybytes);
   outfile.closeFD();
*/
   std::cout << "128 bit key generated and written to " << argv[1] << "\n";
   
   /*
   std::cout << "Reading in the CSV file.";

   DronePlotDB db;
   int count = 0;
   if ((count = db.loadCSVFile(input_file.c_str())) < 0) {
      std::cerr << "Either failed opening file for reading or file was corrupted.\n";
      exit(-1);
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
   */
   return 0;
}
