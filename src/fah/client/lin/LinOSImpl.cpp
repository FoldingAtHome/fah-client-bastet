/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2023, foldingathome.org
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

#include <sys/utsname.h>

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
