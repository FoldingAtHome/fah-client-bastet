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

#include "Unit.h"

#include "App.h"

#include <cbang/event/Event.h>
#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Unit::Unit(App &app, const JSON::ValuePtr &assignment) :
  Event::Scheduler<Unit>(app.getEventBase()), app(app),
  assignment(assignment) {}


void Unit::downloadResponse(Event::Request *req, int err) {
  if (!req || req->getResponseCode() != Event::HTTPStatus::HTTP_OK) {
    LOG_ERROR("Error response from WS");
    // TODO Fail unit
    return;
  }

  LOG_INFO(1, *req->getInputJSON());
}


void Unit::download() {
  URI uri = "http://" + assignment->get(1)->getString("ws") + "/api/assign";
  auto pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
                                 this, &Unit::downloadResponse);
  auto writer = pr->getJSONWriter();

  writer->beginDict();
  writer->insert("assignment", assignment->getDict(1));
  writer->insert("signature",  assignment->getString(2));
  writer->insert("as_cert",    assignment->getString(3));
  writer->endDict();
  writer->close();

  pr->send();
}


void Unit::getCore() {
}


void Unit::run() {
}


void Unit::upload() {
}
