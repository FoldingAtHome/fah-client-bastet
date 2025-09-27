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

#include <cbang/os/PowerManagement.h>
#include <cbang/log/Logger.h>

#include <sys/utsname.h>
#ifdef HAVE_SYSTEMD
#include <systemd/sd-device.h>
#endif


using namespace FAH::Client;
using namespace std;
using namespace cb;


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


bool LinOSImpl::isGPUReady() const {
#ifdef HAVE_SYSTEMD
  sd_device_enumerator *enumerator;
  sd_device *device;
  bool ready = false;

  if (sd_device_enumerator_new(&enumerator) < 0)
    return true;

  if (sd_device_enumerator_add_match_subsystem(enumerator, "drm", 1) < 0 ||
      sd_device_enumerator_add_match_sysname(enumerator, "renderD*") < 0) {
    sd_device_enumerator_unref(enumerator);
    return true;
  }

  for (device = sd_device_enumerator_get_device_first(enumerator);
       device != NULL;
       device = sd_device_enumerator_get_device_next(enumerator)) {
    // "platform" is for Raspberry Pi et al
    if (sd_device_get_parent_with_subsystem_devtype(device, "pci", NULL, NULL) >= 0 ||
        sd_device_get_parent_with_subsystem_devtype(device, "platform", NULL, NULL) >= 0) {
      ready = true;
      break;
    }
  }

  sd_device_enumerator_unref(enumerator);
  return ready;
#endif // HAVE_SYSTEMD

  return true;
}
