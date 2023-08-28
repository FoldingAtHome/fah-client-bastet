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
#include "NodeRemote.h"
#include "Config.h"

#include <cbang/Catch.h>
#include <cbang/json/Reader.h>
#include <cbang/json/Builder.h>
#include <cbang/log/Logger.h>
#include <cbang/net/Base64.h>
#include <cbang/openssl/Digest.h>
#include <cbang/openssl/KeyContext.h>
#include <cbang/util/Random.h>
#include <cbang/util/Press.h>
#include <cbang/os/SystemInfo.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Account::Account(App &app) : app(app) {
  try {machName = SystemInfo::instance().getHostname();} catch (...) {}

  auto &options = app.getOptions();
  options.pushCategory("Account");
  options.addTarget("account-token", token, "Folding@home account token.");
  options.addTarget(
    "machine-name", machName, "Name used to identify this machine.");
  options.popCategory();

  updateEvent = app.getEventBase().newEvent(
    this, &Account::update, EVENT_NO_SELF_REF);
  updateEvent->activate();
}


void Account::setData(const JSON::ValuePtr &data) {
  this->data = data;

  string akey = data->getString("pubkey");
  KeyPair accountKey;
  accountKey.readPublicSPKI(Base64().decode(akey));
  string aid = Digest::urlBase64(accountKey.getRSA_N().toBinString(), "sha256");
  app.getDict("info").insert("account", aid);

  LOG_DEBUG(3, "account = " << data->toString(2));
}


void Account::setToken(const string &token) {
  if (this->token == token) return;
  reset();
  this->token = token;
  if (!token.empty()) updateEvent->activate();
}


void Account::init() {
  auto &db = app.getDB("config");

  if (db.has("account"))
    setData(JSON::Reader::parseString(db.getString("account")));
}


void Account::reset() {
  auto &db = app.getDB("config");

  // Clear token
  token = "";
  if (db.has("account-token")) db.unset("account-token");

  // Delete account data
  data.release();
  app.getDict("info").insert("account", "");
  if (db.has("account")) db.unset("account");

  // Close websocket
  close(Event::WebsockStatus::WS_STATUS_NORMAL);
}


void Account::sendEncrypted(const JSON::Value &msg, const string &sid) {
  string payload = msg.toString(0, false);
  string iv = Random::instance().string(16);
  Cipher cipher("aes-256-cbc", true, key.data(), iv.data());
  JSON::ValuePtr data = new JSON::Dict;

  ivs.insert(iv); // Prevent IV reuse and message replay

  if (10000 < payload.length()) {
    data->insert("compression", "gzip");
    payload = Press("gzip").compress(payload);
  }

  data->insert("type",    "message");
  data->insert("client",  app.getID());
  data->insert("session", sid);

  LOG_DEBUG(3, "Sending: " << data->toString() << " with payload="
            << payload.size());

  data->insert("iv",      URLBase64().encode(iv));
  data->insert("payload", URLBase64().encode(cipher.crypt(payload)));
  send(*data);
}


void Account::requestInfo() {
  Event::Client::RequestPtr pr;

  auto cb = [this, pr] (Event::Request &req) mutable {
    try {
      if (req.logResponseErrors()) {
        // If the account does not exist, forget about it
        if (req.getResponseCode() == HTTP_NOT_FOUND) reset();
        else updateEvent->add((unsigned)updateBackoff.next());

      } else { // Account is valid, connect to node
        setData(req.getInputJSON());

        auto &config = app.getConfig();
        config.setUsername(data->getString("name", "Anonymous"));
        config.setPasskey(data->getString("passkey", ""));
        config.setTeam(data->getU32("team", 0));
        config.insert("cause", data->getString("cause", "any"));

        app.getDB("config").set("account", data->toString());
        connect();
        updateBackoff.reset();
      }
    } CATCH_ERROR;

    pr.release(); // Release request
  };

  string id = app.getID();
  URI uri(app.getOptions()["api-server"].toString() + "/machine/" + id);
  pr = app.getClient().call(uri, Event::RequestMethod::HTTP_GET, cb);
  pr->send();
}


void Account::connect() {
  try {
    setURI("wss://" + data->getString("node") + "/ws/client");
    app.getClient().send(this);
    updateBackoff.reset();
    return;

  } CATCH_ERROR;

  updateEvent->add((unsigned)updateBackoff.next());
}


void Account::link() {
  Event::Client::RequestPtr pr;

  auto cb = [this, pr] (Event::Request &req) mutable {
    if (req.getResponseCode() == HTTP_NOT_FOUND) reset();
    else if (req.logResponseErrors())
      updateEvent->add((unsigned)updateBackoff.next()); // Retry

    else {
      LOG_INFO(1, "Account linked");
      app.getDB("config").set("account-token", token);
      requestInfo();
      updateBackoff.reset();
    }

    pr.release(); // Release request
  };

  string api = app.getOptions()["api-server"].toString();
  URI uri(api + "/machine/" + app.getID());
  pr = app.getClient().call(uri, Event::RequestMethod::HTTP_PUT, cb);

  JSON::Dict data;
  data.insert("name",  machName);
  data.insert("token", token);

  auto &key        = app.getKey();
  string signature = URLBase64().encode(key.signSHA256(data.toString()));
  string pubkey    = app.getPubKey();

  auto writer = pr->getJSONWriter();
  writer->beginDict();
  writer->insert("data",      data);
  writer->insert("signature", signature);
  writer->insert("pubkey",    pubkey);
  writer->endDict();
  writer->close();

  pr->send();
}


void Account::update() {
  string savedToken = app.getDB("config").getString("account-token", "");

  // If token is set and it's not the same as the saved token, link account
  if (!token.empty() && token != savedToken) link();
  else {
    // If we already have the account info, try to connect to the node
    if (data.isSet() && !data->getString("node", "").empty()) connect();
    else if (!savedToken.empty()) requestInfo(); // Otherwise get account info
  }
}


void Account::onOpen() {
  LOG_INFO(1, "Logging into node account");

  // Read account public key
  string akey = data->getString("pubkey");
  KeyPair accountKey;
  accountKey.readPublicSPKI(Base64().decode(akey));

  // Generate encryption key
  key = Random::instance().string(32);
  ivs.clear(); // Reset IVs

  // Encrypt key with account public key using RSA-OAEP
  KeyContext kctx(accountKey);
  kctx.encryptInit();
  kctx.setRSAPadding(KeyContext::PKCS1_OAEP_PADDING);
  kctx.setRSAOAEPMD("SHA-256");
  string encryptedKey = Base64().encode(kctx.encrypt(key));

  // Login payload
  JSON::ValuePtr payload = new JSON::Dict;
  payload->insert("time",    Time().toString());
  payload->insert("account", app.getDict("info").getString("account"));
  payload->insert("key",     encryptedKey);

  // Login message
  string signature = app.getKey().signBase64SHA256(payload->toString(0, true));
  JSON::Dict msg;
  msg.insert("type",      "login");
  msg.insert("payload",   payload);
  msg.insert("signature", signature);
  msg.insert("pubkey",    app.getPubKey());
  send(msg);
}


void Account::onMessage(const JSON::ValuePtr &msg) {
  LOG_DEBUG(3, "received = " << *msg);

  string type = msg->getString("type");

  if (type == "message") {
    string iv      = Base64().decode(msg->getString("iv"));
    string payload = Base64().decode(msg->getString("payload"));

    // Check that this is a unique IV.  Also prevents replay attacks.
    if (!ivs.insert(iv).second) THROW("IV cannot be used more than once");

    // Reset connection to prevent memory overflow.
    if (4e6 < ivs.size()) THROW("Too many IVs");

    Cipher cipher("aes-256-cbc", false, key.data(), iv.data());
    JSON::ValuePtr msg = JSON::Reader::parseString(cipher.crypt(payload));

    LOG_DEBUG(5, *msg);

    type = msg->getString("type");
    string sid = msg->getString("session");

    if (type == "message") {
      auto it = nodes.find(sid);
      if (it == nodes.end()) THROW("Session " << sid << " does not exist");

      auto remote = it->second;
      remote->onMessage(msg->get("content"));

    } else if (type == "session-open") {
      // Open the session
      auto remote = SmartPtr(new NodeRemote(app, *this, sid));
      app.add(remote);
      remote->onOpen();
      nodes[sid] = remote;
    }

  } else if (type == "session-close") {
    // Close all remotes with this session ID
    string sid = msg->getString("session");

    const auto &it = nodes.find(sid);
    if (it != nodes.end()) {
      it->second->close();
      nodes.erase(it);
    }
  }
}


void Account::onClose(Event::WebsockStatus status, const string &msg) {
  LOG_INFO(1, "Account websocket closed: " << status << " msg=" << msg);
  updateEvent->add(5);

  // Close remotes
  for (auto &remote: nodes)
    remote.second->close();

  nodes.clear();
}


void Account::send(const JSON::Value &msg) {
  LOG_DEBUG(5, "Sending: " << msg);
  Event::JSONWebsocket::send(msg);
}
