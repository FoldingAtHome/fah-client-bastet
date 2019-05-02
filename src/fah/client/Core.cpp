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

#include "Core.h"

#include "App.h"
#include "Slots.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/event/Event.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/openssl/KeyPair.h>
#include <cbang/openssl/Digest.h>
#include <cbang/util/BZip2.h>
#include <cbang/tar/Tar.h>
#include <cbang/iostream/BZip2Decompressor.h>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>
namespace io = boost::iostreams;

using namespace FAH::Client;
using namespace cb;
using namespace std;


Core::Core(App &app, const JSON::ValuePtr &data) :
  Event::Scheduler<Core>(app.getEventBase()), app(app), data(data) {
  schedule(&Core::next);
}


string Core::getURL() const {return data->getString("url");}
uint8_t Core::getType() const {return data->getU8("type");}
string Core::getPath() const {return data->getString("path");}


string Core::getFilename() const {
  return String::printf("FahCore_%02x", getType());
}


void Core::notify(function<void ()> cb) {
  if (state == CORE_READY || state == CORE_INVALID) cb();
  else notifyCBs.push_back(cb);
}


void Core::next() {
  switch (state) {
  case CORE_INIT:     load();                      break;
  case CORE_CERT:     download(getURL() + ".crt"); break;
  case CORE_SIG:      download(getURL() + ".sig"); break;
  case CORE_DOWNLOAD: download(getURL());          break;

  case CORE_READY:
  case CORE_INVALID:
    break;
  }
}


void Core::ready() {
  state = CORE_READY;

  // Notify
  for (unsigned i = 0; i < notifyCBs.size(); i++) notifyCBs[i]();
  notifyCBs.clear();
}


void Core::load() {
  try {
    auto db = app.getDB("cores");

    if (db.has(getURL())) {
      data = db.getJSON(getURL());
      if (getURL() != data->getString("url"))
        THROW("Core in DB does not match " << *data);

      LOG_INFO(1, "Loaded " << data->getString("path"));
      schedule(&Core::ready);
      return;
    }
  } CATCH_ERROR;

  state = CORE_CERT;
  schedule(&Core::next);
}


void Core::downloadResponse(const string &pkg) {
  string hash = Digest::hash(pkg, "sha256");

  // Check hash
  if (String::hexEncode(hash) != data->getString("sha256"))
    THROW("Core SHA256 hash does not match");

  // Check certificate, key usage and signature
  string usage = String::printf("core|core%02x", getType());
  app.check(cert, "", sig, hash, usage);
  LOG_INFO(1, "Core signature valid");

  // Stream
  io::filtering_istream stream;
  istringstream str(pkg);

  // Decompress
  string path = "cores" + URI(data->getString("url")).getPath();
  if (String::endsWith(path, ".bz2")) {
    stream.push(BZip2Decompressor());
    path = path.substr(0, path.length() - 4);

  } else if (String::endsWith(path, ".gz")) {
    stream.push(io::zlib_decompressor());
    path = path.substr(0, path.length() - 3);
  }

  stream.push(str);

  // Unpack
  if (String::endsWith(path, ".tar")) {
    path = path.substr(0, path.length() - 4);

    SystemUtilities::rmtree(path);

    Tar tar;
    while (true) {
      tar.readHeader(stream);
      if (tar.isEOF()) break;

      string filename = path + "/" + tar.getFilename();
      LOG_INFO(1, "Extracting " << filename);
      SystemUtilities::ensureDirectory(SystemUtilities::dirname(filename));
      tar.readFile(*SystemUtilities::oopen(filename, tar.getMode()), stream);
    }

    string filename = path + "/" + getFilename();
    if (!SystemUtilities::exists(filename))
      THROW("Core package tar missing " << filename);

  } else {
    SystemUtilities::rmtree(path);
    SystemUtilities::ensureDirectory(path);

    string filename = path + "/" + getFilename();
    LOG_INFO(1, "Extracting " << filename);
    SystemUtilities::cp(stream, *SystemUtilities::oopen(filename, 0644));
  }

  // Save
  data->insert("path", path + "/" + getFilename());
  app.getDB("cores").set(getURL(), *data);

  schedule(&Core::ready);
}


void Core::download(const string &url) {
  app.getClient().call(url, HTTP_GET, this, &Core::response)->send();
}


void Core::response(Event::Request &req) {
  try {
    if (req.getConnectionError()) THROW("No response");
    if (!req.isOk()) THROW(req.getResponseCode() << ": " << req.getInput());

    switch (state) {
    case CORE_CERT: {
      cert = req.getInput();
      state = CORE_SIG;
      break;
    }

    case CORE_SIG:
      sig = req.getInput();
      state = CORE_DOWNLOAD;
      break;

    case CORE_DOWNLOAD: downloadResponse(req.getInput()); break;
    default: THROW("Unexpected core state: " << state);
    }

    schedule(&Core::next);
    return;
  } CATCH_ERROR;

  // TODO retry on error
}
