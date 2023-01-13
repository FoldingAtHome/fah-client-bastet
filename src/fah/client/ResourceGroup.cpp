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

#include "ResourceGroup.h"
#include "App.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/event/HTTPConn.h>

using namespace std;
using namespace cb;
using namespace FAH::Client;


ResourceGroup::ResourceGroup(App &app, const string &name,
                             const JSON::ValuePtr &conf,
                             const JSON::ValuePtr &info) :
  app(app), name(name), config(new Config(app, conf)),
  units(new Units(app, *this, config)) {

  // Connect JSON::Observables
  insert("units",  units);
  insert("config", config);
  insert("info",   info);
}


ResourceGroup::~ResourceGroup() {
  // Closing a client causes it to remove itself so first copy the list
  auto clients = this->clients;

  for (auto client: clients)
    TRY_CATCH_ERROR(client->getConnection()->close());
}


void ResourceGroup::add(const SmartPointer<Remote> &client) {
  clients.push_back(client);
}


void ResourceGroup::remove(Remote &client) {
  for (auto it = clients.begin(); it != clients.end(); it++)
    if (*it == &client) return (void)clients.erase(it);
}


void ResourceGroup::broadcast(const JSON::ValuePtr &changes) {
  LOG_DEBUG(5, __func__ << ' ' << *changes);

  for (auto client: clients)
    if (client->isActive())
      client->sendChanges(changes);
}


void ResourceGroup::notify(list<JSON::ValuePtr> &change) {
  SmartPointer<JSON::List> changes =
    new JSON::List(change.begin(), change.end());

  broadcast(changes);

  // Automatically save changes to config
  if (!change.empty() && change.front()->getString() == "config") {
    app.saveGroup(*this);
    units->triggerUpdate();
  }
}
