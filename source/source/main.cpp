#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sstream>
#include <Poco/String.h>

#include "globalcontext.h"
#include "globallogger.h"
#include "exceptions.h"
#include "utils.h"
#include "drunner_setup.h"
#include "command_general.h"
#include "drunner_settings.h"
#include "showhelp.h"
#include "main.h"
#include "unittests.h"
#include "service.h"
#include "plugins.h"
#include "validateimage.h"
#include "service_manage.h"
#include "drunner_settings.h"
#include "drunner_paths.h"

// ----------------------------------------------------------------------------------------------------------------------

int main(int argc, char **argv)
{
   bool forcereturn = false;

   try
   {
      if (argc > 1 && 0==Poco::icompare(std::string(argv[1]), "debug"))
      {
         forcereturn = true;
         std::vector<std::string> args = { "drunner", "-v","-p","-d","initialise" };
//         std::vector<std::string> args = { "drunner", "-v","__plugin__dbackup","run" };
//         std::vector<std::string> args = { "drunner", "-v","unittest" };

         GlobalContext::init(args);
      }
      else
         GlobalContext::init(argc, argv);

      logmsg(kLDEBUG,"dRunner C++ "+GlobalContext::getParams()->getVersion());

      cResult rval = mainroutines::process();
      if (rval.error())
      {
         logdbg("Error context: "+rval.context());
         fatal(rval.what());
      }
      mainroutines::waitforreturn(forcereturn);
      return rval;
   }

   catch (const eExit & e) {
      mainroutines::waitforreturn(forcereturn);
      return e.exitCode();
   }
}

// 

std::string _imageparse(std::string imagename)
{
   if (imagename.find('/') == std::string::npos)
      return "drunner/" + imagename;
   return imagename;
}

// ----------------------------------------------------------------------------------------------------------------------

cResult mainroutines::process()
{
   const params & p(*GlobalContext::getParams());
   
   if ((p.getCommand() == c_initialise) || (!utils::fileexists(drunnerPaths::getPath_Root())))
   {
      logmsg(kLINFO, "Updating drunner setup (initialising).");
      return drunnerSetup::check_setup();
   }

   // allow unit tests to be run directly from installer.
   if (p.getCommand()==c_unittest)
   {
      // todo: pass args through to catch.
      // int result = Catch::Session().run( argc, argv );
      int result = UnitTest();
      if (result!=0)
         logmsg(kLERROR,"Unit tests failed.");
      logmsg(kLINFO,"All unit tests passed.");
      return kRSuccess;
   }

   if (!GlobalContext::hasSettings())
      fatal("Settings global object not created.");

   if (p.getCommand()==c_configure)
   {
      CommandLine cl;
      cl.command = "configure";
      cl.args = p.getArgs();
      drunnerSettings newSettings;
      return newSettings.handleConfigureCommand(cl); // needs to be read/write settings.
   }

   cResult rval = GlobalContext::getSettings()->checkRequired();
   if (!rval.success())
      fatal("Please configure dRunner:\n " + rval.what());

   // ----------------
   // command handling
   switch (p.getCommand())
   {
      case c_clean:
      {
         command_general::clean();
         break;
      }

      case c_list:
      {
         command_general::showservices();
         break;
      }

      case c_update:
      {
         if (p.numArgs() < 1)
            return drunnerSetup::update_drunner();
         else
            return service_manage::update(p.getArg(0));
      }

      case c_checkimage:
      {
         if (p.numArgs()<1)
            logmsg(kLERROR,"Usage: drunner checkimage IMAGENAME");
         
         validateImage::validate(_imageparse(p.getArg(0)));
         break;
      }

      case c_install:
      {
         if (p.numArgs()<1 || p.numArgs()>2)
            logmsg(kLERROR,"Usage: drunner install IMAGENAME [SERVICENAME]");

         std::string imagename = _imageparse(p.getArg(0));
         std::string servicename = p.numArgs() == 2 ? p.getArg(1) : "";

         cResult r = service_manage::install(servicename, imagename, GlobalContext::getParams()->isDevelopmentMode());
         if (r==kRSuccess)
            logmsg(kLINFO, "Installation complete - try running " + servicename + " now!");
         return r;
      }

      case c_restore:
      {
         if (p.numArgs() < 1 || p.numArgs() > 2)
            logmsg(kLERROR, "Usage: [PASS=?] drunner restore BACKUPFILE [SERVICENAME]");

         return service_manage::service_restore(p.getArg(0), p.numArgs()==2 ? p.getArg(1) : "");
      }

      case c_backup:
      {
         if (p.numArgs() < 1 || p.numArgs() > 2)
            logmsg(kLERROR, "Usage: [PASS = ? ] drunner backup SERVICENAME [BACKUPFILE]");
         service svc(p.getArg(0));
         return svc.backup( p.numArgs()==2 ? p.getArg(1) : "");
      }

      case c_servicecmd:
      {
         if (p.numArgs() < 1)
            logmsg(kLERROR, "servicecmd should not be invoked manually.");

         service svc(p.getArg(0));
         return svc.servicecmd();
      }

      case c_uninstall:
      {
         if (p.numArgs()<1)
            logmsg(kLERROR, "Usage: drunner uninstall SERVICENAME");
         return service_manage::uninstall(p.getArg(0));
      }

      case c_obliterate:
      {
         if (p.numArgs() < 1)
            logmsg(kLERROR, "Usage: drunner obliterate SERVICENAME");

         return service_manage::obliterate(p.getArg(0));
      }

      case c_help:
      {
         showhelp();
         return kRSuccess;
      }

      case c_plugin:
      {
         plugins p;
         return p.runcommand();
      }


      default:
      {
         return cError("The command '"+p.getCommandStr()+"' is not recognised.");
      }
   }
   return 0;
}


void mainroutines::waitforreturn(bool force)
{
   if (force || (GlobalContext::hasParams() && GlobalContext::getParams()->doPause()))
   {
      std::cerr << std::endl << std::endl << "--- PRESS RETURN TO CONTINUE ---" << std::endl;
      std::string s;
      std::getline(std::cin, s);
   }
}


// ----------------------------------------------------------------------------------------------------------------------
