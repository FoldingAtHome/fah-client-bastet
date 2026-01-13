/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2026, foldingathome.org
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

#include "CoreProcess.h"
#include "Core.h"

#include <cbang/os/SystemUtilities.h>
#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


CoreProcess::CoreProcess(const std::string &path) :
  path(SystemUtilities::absolute(path)) {

  // Set environment library paths
  vector<string> paths;
  paths.push_back(SystemUtilities::dirname(this->path));
  const string &ldPath = SystemUtilities::library_path;
  if (SystemUtilities::getenv(ldPath))
    SystemUtilities::splitPaths(SystemUtilities::getenv(ldPath), paths);
  set(ldPath, SystemUtilities::joinPaths(paths));

  // Set working directory
  setWorkingDirectory("work");
}


void CoreProcess::exec(const vector<string> &_args) {
  vector<string> args;
  args.push_back(path);
  args.insert(args.end(), _args.begin(), _args.end());

  LOG_INFO(3, "Running FahCore: " << Subprocess::assemble(args));
  Subprocess::exec(args, Subprocess::NULL_STDOUT | Subprocess::NULL_STDERR |
    Subprocess::CREATE_PROCESS_GROUP | Subprocess::W32_HIDE_WINDOW,
    Subprocess::PRIORITY_IDLE);
}


void CoreProcess::stop() {
  if (!interruptTime) {
    interruptTime = Time::now();
    interrupt();

  } else if (interruptTime + 60 < Time::now()) {
    LOG_WARNING("Core did not shutdown gracefully, killing process");
    kill();
    interruptTime = 1; // Prevent further interrupt or kill
  }
}
