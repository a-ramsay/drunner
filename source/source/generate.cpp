#include <fstream>
#include <iostream>
#include <string>

#include <Poco/File.h>
#include <Poco/TemporaryFile.h>

#include "generate.h"
#include "globallogger.h"
#include "utils.h"
#include "chmod.h"

// check if two files are the same.
bool filesameasstring(std::string filename, const std::string & thestring)
{
   FILE* f = fopen(filename.c_str(), "r");
   if (f == NULL)
      return false;

   fseek(f, 0, SEEK_END);
   size_t size = ftell(f);
   char* where = new char[size];
   rewind(f);
   fread(where, sizeof(char), size, f);

   bool rval = (thestring.compare(where) == 0);

   delete[] where;

   return rval;
}

void generate_low(std::string tfile, const std::string & content)
{
   std::string op;
   std::ofstream ofs;
   ofs.open(tfile);
   if (!ofs.is_open())
      logmsg(kLERROR, "Couldn't write to " + tfile);
   ofs << content;
   ofs.close();
}

cResult generate(
   Poco::Path fullpath,
   const mode_t mode,
   const std::string & content
)
{
   // for now we force the generation. In future should check
   // whether any changes have been made.
   poco_assert(fullpath.isFile());

   if (filesameasstring(fullpath.toString(), content))
      return kRNoChange;

   Poco::File f(fullpath);
   if (f.exists())
   {
      logmsg(kLDEBUG, "Updating " + fullpath.toString());
      if (kRSuccess != utils::delfile(fullpath.toString()))
         logmsg(kLERROR, "Unable to delete file " + fullpath.toString());
   }
   else
      logmsg(kLDEBUG, "Creating " + fullpath.toString());

   // we re-generate rather than move the temp file, as it may be across filesystems (which won't work for C++ move).
   generate_low(fullpath.toString(), content);

   if (xchmod(fullpath.toString().c_str(), mode) != 0)
      logmsg(kLERROR, "Unable to change permissions on " + fullpath.toString());

   return kRSuccess;
}
