#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>

#include "globalcontext.h"
#include "globallogger.h"
#include "exceptions.h"
#include "utils.h"
#include "command_setup.h"
#include "command_general.h"
#include "command_dev.h"
#include "sh_drunnercfg.h"
#include "showhelp.h"
#include "main.h"
#include "unittests.h"
#include "service.h"

//  sudo apt-get install build-essential g++-multilib libboost-all-dev

using namespace utils;

// ----------------------------------------------------------------------------------------------------------------------


int main(int argc, char **argv)
{
   try
   {
      mainroutines::check_basics();

      GlobalContext::init(argc, argv);

      logmsg(kLDEBUG,"dRunner C++, version "+p.getVersion());
      bool canRunDocker=utils::canrundocker(getUSER());
      logmsg(kLDEBUG,"Username: "+getUSER()+",  Docker OK: "+(canRunDocker ? "YES" : "NO")+", "+utils::get_exename()+" path: "+utils::get_exepath());

      return mainroutines::process();
   }

   catch (const eExit & e) {
      return e.exitCode();
   }
}

// ----------------------------------------------------------------------------------------------------------------------


void mainroutines::check_basics()
{
   uid_t euid=geteuid();
   if (euid == 0)
	   fatal("Please run as a standard user, not as root.");

   std::string user=utils::getUSER();
   if (!utils::isindockergroup(user))
	   fatal("Please add the current user to the docker group. As root: "+kCODE_S+"adduser "+user+" docker"+kCODE_E);

   if (!utils::canrundocker(user))
	   fatal(user+" hasn't picked up group docker yet. Log out then in again, or run "+kCODE_S+"exec su -l "+user+kCODE_E);

   if (!utils::commandexists("docker"))
	   fatal("Please install Docker before using dRunner.\n(e.g. use  https://raw.githubusercontent.com/j842/scripts/master/install_docker.sh )");

   if (!utils::commandexists("curl"))
	   fatal("Please install curl before using dRunner.");

   if (!utils::commandexists("docker-compose"))
	   fatal("Please install docker-compose before using dRunner.");

   std::string v;
   if (utils::bashcommand("docker --version",v)!=0)
	   fatal("Running \"docker --version\" failed! Is docker correctly installed on this machine?");
}

// ----------------------------------------------------------------------------------------------------------------------

int mainroutines::process()
{
   // handle setup specially.
   if (GlobalContext::getParams()->getCommand()==c_setup)
   {
      int rval=command_setup::setup(p);
      if (rval!=0) throw eExit("Setup failed.",rval);
      return 0;
   }

   // allow unit tests to be run directly from installer.
   if (GlobalContext::getParams()->getCommand().getCommand()==c_unittest)
   {
      // todo: pass args through to catch.
      // int result = Catch::Session().run( argc, argv );
      int result = UnitTest();
      if (result!=0)
         logmsg(kLERROR,"Unit tests failed.",p);
      logmsg(kLINFO,"All unit tests passed.",p);
      return 0;
   }

   if (!utils::isInstalled())
      showhelp("Please run "+utils::get_exename()+" setup ROOTPATH");

   logmsg(kLDEBUG,"Settings read from "+GlobalContext::getSettings()->getPath_drunnercfg_sh());


   // ----------------
   // command handling
   switch (GlobalContext::getParams()->getCommand())
   {
      case c_clean:
      {
         command_general::clean(p,settings);
         break;
      }

      case c_list:
      {
         command_general::showservices(p,settings);
         break;
      }

      case c_update:
      {
         if (p.numArgs()<1)
            command_setup::update(p, settings); // defined in command_setup
         else
         { // first argument is service name.
            service s(p, settings, p.getArg(0));
            s.update();
         }
         break;
      }

      case c_checkimage:
      {
         if (p.numArgs()<1)
            logmsg(kLERROR,"Usage: drunner checkimage IMAGENAME",p);
         
         validateImage(p, settings, p.getArg(0));
         break;
      }

      case c_install:
      {
         if (p.numArgs()<1 || p.numArgs()>2)
            logmsg(kLERROR,"Usage: drunner install IMAGENAME [SERVICENAME]",p);
         std::string imagename = p.getArg(0);
         std::string servicename;
         if ( p.numArgs()==2)
            servicename=p.getArg(1); // if empty then install will set to default from imagename.
         else
         {
            servicename = imagename;
            size_t found;
            while ((found = servicename.find("/")) != std::string::npos)
               servicename.erase(0, found + 1);
            while ((found = servicename.find(":")) != std::string::npos)
               servicename.erase(found);
         }

         service svc(p, settings, servicename, imagename);
         svc.install();
         break;
      }

      case c_restore:
      {
         if (p.numArgs() < 2)
            logmsg(kLERROR, "Usage: [PASS=?] drunner restore BACKUPFILE SERVICENAME", p);

         return service_restore(p, settings, p.getArg(1), p.getArg(0));
      }

      case c_backup:
      {
         if (p.numArgs() < 2)
            logmsg(kLERROR, "Usage: [PASS = ? ] drunner backup SERVICENAME BACKUPFILE", p);
         service svc(p, settings, p.getArg(0));
         svc.backup(p.getArg(1));
         break;
      }

      case c_enter:
      {
         if (p.numArgs() < 1)
            logmsg(kLERROR, "Usage: drunner enter SERVICENAME", p);
         service svc(p, settings, p.getArg(0));
         svc.enter();
         break;
      }

      case c_build:
      {
         if (p.numArgs()<1)
            command_dev::build(p,settings);
         else
            command_dev::build(p,settings,p.getArg(0));
         break;
      }

      case c_servicecmd:
      {
         if (p.numArgs() < 1)
            logmsg(kLERROR, "servicecmd should not be invoked manually.", p);

         service svc(p, settings, p.getArg(0));
         if (!svc.isValid())
            logmsg(kLERROR, "Service " + svc.getName() + " is not valid - try recover.", p);

         return svc.servicecmd();
      }

      case c_status:
      {
         if (p.numArgs() < 1)
            logmsg(kLERROR, "Usage: drunner status SERVICENAME", p);
         service svc(p, settings, p.getArg(0));
         return svc.status();
      }

      case c_recover:
      {
         if (p.numArgs() < 1)
            logmsg(kLERROR, "Usage: drunner recover SERVICENAME [IMAGENAME]", p);
         std::string servicename = p.getArg(0);
         std::string imagename;
         if (p.numArgs() < 2)
         { // see if we can read the imagename from the damaged service.
            service svc1(p, settings, servicename);
            if (!utils::fileexists(svc1.getPath()))
               logmsg(kLERROR, "That service is not installed. Try installing, which will preserve any data volumes.",p);

            imagename = svc1.getImageName();
            if (imagename.length() == 0)
               fatal("Programming error - imagename is empty.");
         }
         else
            imagename = p.getArg(1);

         logmsg(kLINFO, "Recovering " + servicename + " from image " + imagename, p);
         service svc(p, settings, servicename, imagename);
         return (int)svc.recover();
      }

      case c_uninstall:
      {
         if (p.numArgs()<1)
            logmsg(kLERROR, "Usage: drunner uninstall SERVICENAME", p);
         service svc(p, settings, p.getArg(0));
         return (int)svc.uninstall();
      }

      case c_obliterate:
      {
         if (p.numArgs() < 1)
            logmsg(kLERROR, "Usage: drunner obliterate SERVICENAME", p);
         service svc(p, settings, p.getArg(0));
         return (int)svc.obliterate();
      }

      case c_saveenvironment:
      {
         if (p.numArgs() < 3)
            logmsg(kLERROR, "Usage: drunner __save-environment SERVICENAME KEY VALUE",p);
         service svc(p, settings, p.getArg(0));
         if (!svc.isValid())
            logmsg(kLERROR, "Service " + svc.getName() + " is not valid - try recover.", p);

         svc.getEnvironment().save_environment(p.getArg(1), p.getArg(2));
         logmsg(kLDEBUG, "Save environment variable " + p.getArg(1) + "=" + p.getArg(2),p);
         return kRSuccess;
      }

      default:
         {
            logmsg(kLERROR,R"EOF(

          /-------------------------------------------------------------\
          |   That command has not been implemented and I am sad. :,(   |
          \-------------------------------------------------------------/
)EOF",p);
            return 1;
      }
   }
   return 0;
}


// ----------------------------------------------------------------------------------------------------------------------
