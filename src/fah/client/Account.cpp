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

#include "Account.h"
#include "App.h"
#include "ResourceGroup.h"

#include <cbang/json/Reader.h>
#include <cbang/log/Logger.h>
#include <cbang/net/Base64.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Account::Account(App &app) : app(app) {
  updateEvent = app.getEventBase().newEvent(
    this, &Account::update, EVENT_NO_SELF_REF);
  updateEvent->activate();
}


void Account::init() {
  auto &db = app.getDB("config");

  if (db.has("account"))
    setInfo(JSON::Reader::parseString(db.getString("account")));
}


void Account::setInfo(const JSON::ValuePtr &account) {
  if (account.isNull()) {
    if (app.has("account")) app.erase("account");

    auto &db = app.getDB("config");
    if (db.has("account")) db.unset("account");

  } else {
    LOG_DEBUG(3, "account = " << account->toString(2));
    app.insert("account", account);
  }
}


void Account::getInfo() {
  Event::Client::RequestPtr pr;

  auto cb = [this, pr] (Event::Request &req) mutable {
    if (req.logResponseErrors()) {
      if (req.getResponseCode() == HTTP_NOT_FOUND) setInfo(0);
      else updateEvent->add((unsigned)updateBackoff.next());

    } else {
      auto account = req.getInputJSON();
      app.getDB("config").set("account", account->toString());
      setInfo(account);
      connect(account->getString("node"));
    }

    pr.release(); // Release request
  };

  string id = app.getString("id");
  URI uri(app.getOptions()["api-server"].toString() + "/machine/" + id);
  pr = app.getClient().call(uri, Event::RequestMethod::HTTP_GET, cb);
  pr->send();
}


void Account::connect(const string &node) {
  setURI("wss://" + node + "/ws/client");
  app.getClient().send(this);
}


void Account::link(const string &token) {
  Event::Client::RequestPtr pr;

  auto cb = [this, pr, token] (Event::Request &req) mutable {
    if (req.logResponseErrors()) {
      if (req.getResponseCode() != HTTP_NOT_FOUND)
        updateEvent->add((unsigned)updateBackoff.next()); // Retry

    } else {
      LOG_INFO(1, "Account linked");
      app.getDB("config").set("account-token", token);
      getInfo();
    }

    pr.release(); // Release request
  };

  string id = app.getString("id");
  URI uri(app.getOptions()["api-server"].toString() + "/machine/" + id);
  pr = app.getClient().call(uri, Event::RequestMethod::HTTP_PUT, cb);

  JSON::ValuePtr data = new JSON::Dict;
  data->insert("name", app.getGroup("")->getConfig()->getMachineName());
  data->insert("token", token);

  auto &key        = app.getKey();
  string signature = URLBase64().encode(key.signSHA256(data->toString()));
  string pubkey    = URLBase64().encode(key.publicToDER());

  auto writer = pr->getJSONWriter();
  writer->beginDict();
  writer->insert("data",      *data);
  writer->insert("signature", signature);
  writer->insert("pubkey",    pubkey);
  writer->endDict();
  writer->close();

  pr->send();
}


void Account::update() {
  auto  &db         = app.getDB("config");
  string token      = app.getOptions()["account-token"].toString();
  string savedToken = db.getString("account-token", "");

  if (!token.empty() && token != savedToken) link(token);
  else if (!savedToken.empty()) getInfo();
}


void Account::onOpen() {
  LOG_INFO(1, "Account WS opened");
}


void Account::onMessage(const JSON::ValuePtr &msg) {
  LOG_INFO(1, "message = " << *msg);
}


void Account::onPing(const string &payload) {
  Websocket::onPing(payload);
  LOG_INFO(1, CBANG_FUNC);
}


void Account::onPong(const string &payload) {
  Websocket::onPong(payload);
  LOG_INFO(1, CBANG_FUNC);
}


void Account::onClose(Event::WebsockStatus status, const string &msg) {
  LOG_INFO(1, "Account WS closed: " << status << " msg=" << msg);
  updateEvent->add(15);
}
