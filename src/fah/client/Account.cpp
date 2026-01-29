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

#include "Account.h"
#include "App.h"
#include "NodeRemote.h"
#include "Config.h"
#include "Groups.h"

#include <cbang/Catch.h>
#include <cbang/json/Reader.h>
#include <cbang/json/Builder.h>
#include <cbang/log/Logger.h>
#include <cbang/net/Base64.h>
#include <cbang/openssl/Digest.h>
#include <cbang/openssl/KeyContext.h>
#include <cbang/util/Random.h>
#include <cbang/util/WeakCallback.h>
#include <cbang/comp/Press.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/http/Conn.h>
#include <cbang/config/RegexConstraint.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Account::Account(App &app) : app(app) {
  auto &options = app.getOptions();
  options.pushCategory("Account");
  options.add("account-token", "Folding@home account token.");
  options.add("machine-name", "Name used to identify this machine.",
    new RegexConstraint(Regex("[^<>;&'\\\"]{1,64}"),
    "Must be between 1 and 64 characters and cannot include any of <>;&'\""));
  options.popCategory();

  updateEvent = app.getEventBase().newEvent([this] {update();}, 0);
  updateEvent->activate();
}


string Account::getMachName() const {
  string machName = app.getDB("config").getString("machine-name", "");

  if (machName.empty())
    try {machName = SystemInfo::instance().getHostname();} catch (...) {}

  return machName.empty() ? "machine-#" : machName;
}


void Account::init() {
  auto &db = app.getDB("config");

  // Get account info
  if (db.has("account")) setData(JSON::Reader::parse(db.getString("account")));

  // Command line linking
  auto &options = app.getOptions();
  if (options["account-token"].hasValue()) {
    string token = options["account-token"];

    if (token != db.getString("command-line-token", "")) {
      db.set("command-line-token", token);
      db.set("requested-token",    token);
      if (options["machine-name"].hasValue())
        db.set("machine-name", options["machine-name"]);
    }
  }

  restart();
}


void Account::link(const string &token, const string &machName) {
  auto &db = app.getDB("config");

  db.set("requested-token", token);
  db.set("machine-name", machName);

  restart();
}


void Account::restart() {
  // Close websocket
  close(WS::Status::WS_STATUS_NORMAL, "Account restart");

  // Are we linked with the account?
  auto &db = app.getDB("config");
  string requestedToken = db.getString("requested-token", "");
  string accountToken   = db.getString("account-token",   "");

  if (!requestedToken.empty() && requestedToken != accountToken)
    return setState(STATE_LINK);

  // Already linked
  if (!accountToken.empty()) return setState(STATE_INFO);

  // Delete account data
  data.release();
  app.getDict("info").insert("account", "");
  if (db.has("account")) db.unset("account");

  setState(STATE_IDLE);
}


void Account::sendEncrypted(const JSON::Value &_msg, const string &sid) {
  string payload = _msg.toString(0, false);
  string iv = Random::instance().string(16);
  Cipher cipher("aes-256-cbc", true, sessionKey.data(), iv.data());
  JSON::Dict msg;

  ivs.insert(iv); // Prevent IV reuse and message replay

  if (10000 < payload.length()) {
    msg.insert("compression", "gzip");
    payload = Press("gzip").compress(payload);
  }

  msg.insert("type",    "message");
  msg.insert("client",  app.getID());
  msg.insert("session", sid);

  LOG_DEBUG(5, "Sending: " << msg.toString() << " with payload="
            << payload.size());

  msg.insert("iv",      URLBase64().encode(iv));
  msg.insert("payload", URLBase64().encode(cipher.crypt(payload)));
  send(msg);
}


void Account::setState(state_t state) {
  if (this->state == state) return;
  this->state = state;

  updateBackoff.reset();

  if (state != STATE_CONNECTED && state != STATE_IDLE)
    updateEvent->activate();
}


void Account::setData(const JSON::ValuePtr &data) {
  this->data = data;

  string akey = data->getString("pubkey");
  accountKey.readPublicSPKI(Base64().decode(akey));
  string aid = Digest::urlBase64(accountKey.getRSA_N().toBinString(), "sha256");
  app.getDict("info").insert("account", aid);

  LOG_DEBUG(3, "account = " << data->toString(2));
}


void Account::retry() {updateEvent->add((unsigned)updateBackoff.next());}


void Account::reset() {
  auto &db = app.getDB("config");

  // Clear token
  if (db.has("account-token"))   db.unset("account-token");
  if (db.has("requested-token")) db.unset("requested-token");

  // Delete account data
  data.release();
  app.getDict("info").insert("account", "");
  if (db.has("account")) db.unset("account");

  restart();
}


void Account::info() {
  HTTP::Client::callback_t cb = [this] (HTTP::Request &req) mutable {
    pr.release();

    try {
      if (req.logResponseErrors()) {
        // If the account does not exist, forget about it
        if (req.getResponseCode() == HTTP_NOT_FOUND) reset();
        else retry();

      } else { // Account is valid, connect to node
        setData(req.getInputJSON());
        app.getConfig()->configure(*data);

        auto db = app.getDB("config");
        db.set("account", data->toString());

        if (data->hasString("mach_name")) {
          auto name = data->getString("mach_name");
          app.getDict("info").insert("mach_name", name);
          db.set("machine-name", name);
        }

        setState(STATE_CONNECT);
      }

      return;
    } CATCH_ERROR;

    retry();
  };

  string id = app.getID();
  URI uri(app.getOptions()["api-server"].toString() + "/machine/" + id);
  pr = app.getClient().call(uri, HTTP_GET, WeakCall(this, cb));
  pr->send();
}


void Account::connect() {
  try {
    URI uri("wss://" + data->getString("node") + "/ws/client");
    Websocket::connect(app.getClient(), uri);
    setState(STATE_CONNECTED);
    return;

  } CATCH_ERROR;

  retry();
}


void Account::link() {
  auto &db = app.getDB("config");
  string requestedToken = db.getString("requested-token", "");

  HTTP::Client::callback_t cb =
    [this, requestedToken] (HTTP::Request &req) {
      pr.release();
      if (req.getResponseCode() == HTTP_NOT_FOUND) reset();
      else if (req.logResponseErrors()) retry();

      else {
        LOG_INFO(1, "Account linked");
        app.getDB("config").set("account-token", requestedToken);
        setState(STATE_INFO);
      }
    };

  string api = app.getOptions()["api-server"].toString();
  URI uri(api + "/machine/" + app.getID());
  pr = app.getClient().call(uri, HTTP::Method::HTTP_PUT, WeakCall(this, cb));

  JSON::Dict data;
  data.insert("name",  getMachName());
  data.insert("token", requestedToken);

  auto &key        = app.getKey();
  string signature = URLBase64().encode(key.signSHA256(data.toString()));
  string pubkey    = app.getPubKey();

  pr->getRequest()->send([&] (JSON::Sink &sink) {
    sink.beginDict();
    sink.insert("data",      data);
    sink.insert("signature", signature);
    sink.insert("pubkey",    pubkey);
    sink.endDict();
  });
  pr->send();
}


void Account::update() {
  switch (state) {
  case STATE_CONNECTED:
  case STATE_IDLE:    return;
  case STATE_LINK:    return link();
  case STATE_INFO:    return info();
  case STATE_CONNECT: return connect();
  }
}


void Account::onEncrypted(const JSON::ValuePtr &_msg) {
  string iv      = Base64().decode(_msg->getString("iv"));
  string payload = Base64().decode(_msg->getString("payload"));

  // Check that this is a unique IV.  Also prevents replay attacks.
  if (!ivs.insert(iv).second) THROW("IV cannot be used more than once");

  // Reset connection to prevent memory overflow.
  if (4e6 < ivs.size()) THROW("Too many IVs");

  Cipher cipher("aes-256-cbc", false, sessionKey.data(), iv.data());
  JSON::ValuePtr msg = JSON::Reader::parse(cipher.crypt(payload));

  LOG_DEBUG(5, *msg);

  string type = msg->getString("type");
  string sid  = msg->getString("session");

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
}


void Account::onBroadcast(const JSON::ValuePtr &msg) {
  auto payload     = msg->get("payload");
  string signature = msg->getString("signature");

  // Verify signature
  accountKey.verifyBase64SHA256(signature, payload->toString(0, true));

  // Process command
  string cmd = payload->getString("cmd");

  if (cmd == "state")   return app.setState(*payload);
  if (cmd == "config")  return app.configure(*payload);
  if (cmd == "restart") {
    if (app.validateChange(*payload)) restart();
    return;
  }

  LOG_WARNING("Unsupported broadcast message '" << cmd << "'");
}


void Account::onSessionClose(const JSON::ValuePtr &msg) {
  // Close all remotes with this session ID
  string sid = msg->getString("session");

  const auto &it = nodes.find(sid);
  if (it != nodes.end()) {
    it->second->close();
    nodes.erase(it);
  }
}


void Account::onOpen() {
  LOG_INFO(1, "Logging into node account");

  // Generate encryption key
  sessionKey = Random::instance().string(32);
  ivs.clear(); // Reset IVs

  // Encrypt key with account public key using RSA-OAEP
  KeyContext kctx(accountKey);
  kctx.encryptInit();
  kctx.setRSAPadding(KeyContext::PKCS1_OAEP_PADDING);
  kctx.setRSAOAEPMD("SHA256");
  string encryptedKey = Base64().encode(kctx.encrypt(sessionKey));

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

  if (type == "message")       return onEncrypted(msg);
  if (type == "broadcast")     return onBroadcast(msg);
  if (type == "session-close") return onSessionClose(msg);
}


void Account::onClose(WS::Status status, const string &msg) {
  LOG_INFO(1, "Account websocket closed: " << status << " msg=" << msg);

  // Close remotes
  for (auto &remote: nodes)
    remote.second->close();

  nodes.clear();

  if (status != WS::Status::WS_STATUS_NORMAL) restart();
}


void Account::send(const JSON::Value &msg) {
  LOG_DEBUG(5, "Sending: " << msg);
  WS::JSONWebsocket::send(msg);
}
