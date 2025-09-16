/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2024, foldingathome.org
                               All rights reserved.

       This program is free software; you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 3 of the License, or
                       (at your option) any later version.

         This program is distributed in the hope that it will be useful,
          but WITHOUT ANY WARRANTY; without even the implied warranty of
          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                   GNU General Public License for more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
           51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

                  For information regarding this software email:
                                 Joseph Coffland
                          joseph@cauldrondevelopment.com

\******************************************************************************/

#include "LinOSImpl.h"

#include <cbang/config.h>
#include <cbang/os/PowerManagement.h>
#include <cbang/log/Logger.h>

#include <sys/utsname.h>

#if defined(HAVE_SYSTEMD)
#include <systemd/sd-bus.h>
#endif


using namespace FAH::Client;
using namespace std;
using namespace cb;


#if defined(HAVE_SYSTEMD)
class SDBusMessage {
  sd_bus_message *msg;

public:
  SDBusMessage(sd_bus_message *msg) : msg(msg) {}


  bool enter(char type, const char *contents) {
    return 0 < sd_bus_message_enter_container(msg, type, contents);
  }


  void exit() {sd_bus_message_exit_container(msg);}
  void skip(const char *contents) {sd_bus_message_skip(msg, contents);}


  string getString() const {
    const char *s = 0;
    sd_bus_message_read_basic(msg, SD_BUS_TYPE_STRING, &s);
    return string(s);
  }


  bool getBool() const {
    int b = 0;
    sd_bus_message_read_basic(msg, SD_BUS_TYPE_BOOLEAN, &b);
    return b;
  }
};


extern "C"
int on_properties_changed(sd_bus_message *_msg, void *os, sd_bus_error *) {
  SDBusMessage msg(_msg);

  if (msg.getString() != "org.freedesktop.login1.Seat") return 0;

  msg.enter(SD_BUS_TYPE_ARRAY, "{sv}");

  while (msg.enter(SD_BUS_TYPE_DICT_ENTRY, "sv")) {
    if (msg.getString() == "CanGraphical") {
      msg.enter(SD_BUS_TYPE_VARIANT, "b");

      if (msg.getBool()) {
        LOG_DEBUG(3, "GPU ready after `CanGraphical` became true");
        ((LinOSImpl *)os)->setGPUReady(true);
      }

      msg.exit();

    } else msg.skip("v");

    msg.exit();
  }

  msg.exit();
  return 0;
}


struct LinOSImpl::private_t {
  sd_bus *bus = 0;


  private_t() {sd_bus_open_system(&bus);}
  ~private_t() {if (bus) sd_bus_flush_close_unref(bus);}


  void start(LinOSImpl &os) {
    if (!bus) return os.setGPUReady(true);

    int ready = 0;
    sd_bus_get_property_trivial(bus, "org.freedesktop.login1",
      "/org/freedesktop/login1/seat/seat0", "org.freedesktop.login1.Seat",
      "CanGraphical", 0, SD_BUS_TYPE_BOOLEAN, &ready);

    if (ready) {
      LOG_DEBUG(3, "GPU ready on start up");
      os.setGPUReady(true);

    } else sd_bus_match_signal(bus, 0, "org.freedesktop.login1",
      "/org/freedesktop/login1/seat/seat0",
      "org.freedesktop.DBus.Properties", "PropertiesChanged",
      on_properties_changed, &os);
  }
};


#else
struct LinOSImpl::private_t {
  void start(LinOSImpl &os) {os.setGPUReady();}
};
#endif // HAVE_SYSTEMD


LinOSImpl::LinOSImpl(App &app) : OS(app), pri(new private_t) {
  setGPUReady(false);
}


LinOSImpl::~LinOSImpl() {}


const char *LinOSImpl::getName() const {return "linux";}


const char *LinOSImpl::getCPU() const {
  struct utsname name;

  uname(&name);
  string machine(name.machine);

  if (machine.find("x86_64") != string::npos) return "amd64";
  if (machine == "arch64" || machine == "arm64") return "arm64";

  return OS::getCPU();
}


bool LinOSImpl::isSystemIdle() const {
  return 15 < PowerManagement::instance().getIdleSeconds();
}


void LinOSImpl::dispatch() {
  pri->start(*this);
  OS::dispatch();
};
