#include <vector>
#include <string>
#include "params.h"

#ifndef __UTILS_H
#define __UTILS_H

// Shell utilities
namespace utils
{
   const std::string kCODE_S="\e[32m";
   const std::string kCODE_E="\e[0m";

   std::string replacestring(std::string subject, const std::string& search, const std::string& replace);
   std::string getabsolutepath(std::string path);
   void createordie(const params & p, std::string path);

   bool canrundocker(std::string username);
   bool isindockergroup(std::string username);
   std::string getUSER();

   std::string bashcommand(const params & p, std::string c);
   int bashcommand(std::string command, std::string & output);
   std::string trim_copy(std::string s, const char* t = " \t\n\r\f\v");
   std::string& trim(std::string& s, const char* t = " \t\n\r\f\v");

   void die( const params & p, std::string msg, int exit_code=1 );
   void dielow( std::string msg, int exit_code=1 );
}

#endif
