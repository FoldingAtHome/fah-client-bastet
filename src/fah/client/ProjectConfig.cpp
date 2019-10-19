/******************************************************************************\

                  This file is part of the Folding@home Client.

           The fahclient runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2019, foldingathome.org
                               All rights reserved.

       This program is free software; you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
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

#include "ProjectConfig.h"

#include <cbang/Catch.h>
#include <cbang/json/JSON.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


void ProjectConfig::writeRequest(JSON::Sink &sink) const {
  string release;
  uint64_t key = 0;
  string cause;

  if (config.hasString("options")) {
    vector<string> options;
    String::tokenize(config.getString("options"), options);

    for (unsigned i = 0; i < options.size(); i++)
      try {
        vector<string> parts;
        String::tokenize(options[i], parts, "=", true, 2);

        if (parts.size() == 2) {
          string name = String::toLower(parts[0]);
          const string &value = parts[1];

          if (name == "release" || name == "client-type") release = value;
          if (name == "key" || name == "project-key")
            key = String::parseU64(value);
          if (name == "cause") cause = value;
        }
      } CATCH_ERROR;
  }

  if (release.empty() && config.hasString("release"))
    release = config.getString("release");

  if (!key && config.hasU64("key")) key = config.getU64("key");
  if (cause.empty() && config.hasString("cause"))
    cause = config.getString("cause");

  sink.insertDict("project");

  if (!release.empty()) sink.insert("release", String::toLower(release));
  if (key)              sink.insert("key",     key);
  if (!cause.empty())   sink.insert("cause",   String::toLower(cause));

  sink.endDict();
}
