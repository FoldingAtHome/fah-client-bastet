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

#include "Units.h"

#include "App.h"
#include "Unit.h"

#include <cbang/Catch.h>
#include <cbang/json/JSON.h>
#include <cbang/log/Logger.h>
#include <cbang/net/Base64.h>


using namespace FAH::Client;
using namespace cb;
using namespace std;


Units::Units(App &app) {
  string clientID = Base64().encode(URLBase64().decode(app.getID()));
  unsigned count  = 0;

  app.getDB("units").foreach(
    [this, &app, &clientID, &count] (const string &id, const string &data) {
      try {
        auto unit = SmartPtr(new Unit(app, JSON::Reader::parseString(data)));

        if (unit->getClientID() == clientID) {add(unit); count++;}
        else LOG_ERROR("WU with client ID " << unit->getClientID()
                       << " does not belong client " << clientID);
      } CATCH_ERROR;
    });

  LOG_INFO(3, "Loaded " << count << " wus.");
}


bool Units::isActive() const {
  for (unsigned i = 0; i < size(); i++)
    if (!getUnit(i)->isPaused()) return true;

  return false;
}


bool Units::hasFailure() const {
  for (unsigned i = 0; i < size(); i++)
    if (getUnit(i)->getRetries()) return true;

  return false;
}


void Units::add(const SmartPointer<Unit> &unit) {append(unit);}


int Units::getUnitIndex(const string &id) const {
  for (unsigned i = 0; i < size(); i++)
    if (getUnit(i)->getID() == id) return i;

  return -1;
}


SmartPointer<Unit> Units::findUnit(const string &id) {
  int index = getUnitIndex(id);
  return index == -1 ? 0 : getUnit(index);
}


SmartPointer<Unit> Units::getUnit(unsigned index) const {
  if (size() <= index) THROW("Invalid unit index " << index);
  return get(index).cast<Unit>();
}


SmartPointer<Unit> Units::getUnit(const string &id) const {
  int index = getUnitIndex(id);
  if (index < 0) THROW("Unit " << id << " not found.");
  return getUnit(index);
}


void Units::removeUnit(const string &id) {
  int index = getUnitIndex(id);
  if (index == -1) THROW("Invalid unit " << id);
  erase(index);
}
