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

#include "Core.h"

#include "App.h"
#include "GPUResources.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/event/Event.h>
#include <cbang/http/ConnOut.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/openssl/KeyPair.h>
#include <cbang/openssl/Digest.h>
#include <cbang/comp/Tar.h>
#include <cbang/comp/CompressionFilter.h>
#include <cbang/boost/IOStreams.h>
#include <cbang/util/WeakCallback.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Core::Core(App &app, const JSON::ValuePtr &data) :
  app(app), data(data),
  nextEvent(app.getEventBase().newEvent(this, &Core::next, 0)),
  readyEvent(app.getEventBase().newEvent(this, &Core::ready, 0)) {
  nextEvent->activate();
}


Core::~Core() {}


string   Core::getURL()  const {return data->getString("url");}
uint32_t Core::getType() const {return data->getU32("type");}
string   Core::getPath() const {return data->getString("path");}


void Core::addProgressCallback(progress_cb_t cb) {
  if (state == CORE_READY || state == CORE_INVALID) cb(1, 1);
  else progressCBs.push_back(cb);
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
  for (auto &cb: progressCBs) cb(1, 1);
  progressCBs.clear(); // Release callbacks
}


void Core::load() {
  try {
    auto db = app.getDB("cores");

    if (db.has(getURL())) {
      data = db.getJSON(getURL());
      if (getURL() != data->getString("url"))
        THROW("Core in DB does not match " << *data);

      string path = data->getString("path");
      if (!SystemUtilities::exists(path))
        THROW("Core " << path << " not found on disk");

      LOG_INFO(1, "Loaded " << path);
      readyEvent->activate();
      return;
    }
  } CATCH_ERROR;

  state = CORE_CERT;
  nextEvent->activate();
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

  // Get path
  string path = "cores" + URI(data->getString("url")).getPath();

  // Decompress
  Compression compression = compressionFromPath(path);
  if (compression != COMPRESSION_NONE) {
    pushDecompression(compression, stream);
    path = SystemUtilities::removeExtension(path);
  }

  stream.push(str);

  // Unpack
  if (!String::endsWith(path, ".tar")) THROW("Expected tar file");

  path = SystemUtilities::removeExtension(path);
  string base = SystemUtilities::basename(path);

  SystemUtilities::rmtree(path);

  Tar tar;
  while (true) {
    tar.readHeader(stream);
    if (tar.isEOF()) break;

    switch (tar.getType()) {
    case Tar::NORMAL_FILE: case Tar::CONTIGUOUS_FILE: break;
    case Tar::DIRECTORY: continue;
    default: THROW("Unexpected file type in core package: " << tar.getType());
    }

    string filename = tar.getFilename();
    if (!String::startsWith(filename, base + "/"))
      THROW("Invalid file path in core package: " << filename);

    filename = path + filename.substr(base.length());
    LOG_INFO(1, "Extracting " << filename);
    SystemUtilities::ensureDirectory(SystemUtilities::dirname(filename));
    tar.readFile(*SystemUtilities::oopen(filename, tar.getMode()), stream);
  }

  // Make core executable
  string filename = path + String::printf("/FahCore_%02x", getType());

#ifdef _WIN32
  if (SystemUtilities::exists(filename))
    SystemUtilities::rename(filename, filename + ".exe");

  filename += ".exe";
#endif

  if (!SystemUtilities::exists(filename))
    THROW("Core package tar missing " << filename);

  SystemUtilities::chmod(filename, 0755);

  // Save
  data->insert("path", filename);
  app.getDB("cores").set(getURL(), *data);

  readyEvent->activate();
}


void Core::download(const string &url) {
  if (pr.isSet()) THROW("Already downloading core");
  LOG_INFO(1, "Downloading " << url);

  // Monitor download progress
  Progress::callback_t progressCB =
    [this] (const Progress &p) {
      for (auto &cb: progressCBs)
        cb(p.getTotal(), p.getSize());
    };

  HTTP::Client::callback_t cb = [this] (HTTP::Request &req) {response(req);};
  pr = app.getClient().call(url, HTTP_GET, WeakCall(this, cb));
  auto &progress = pr->getConnection()->getReadProgress();
  progress.setCallback(WeakCall(this, progressCB), 1);
  pr->send();
}


void Core::response(HTTP::Request &req) {
  try {
    pr.release();
    if (req.getConnectionError()) THROW("No response");
    if (!req.isOk()) THROW(req.getResponseCode() << ": " << req.getInput());

    switch (state) {
    case CORE_CERT: {
      cert  = req.getInput();
      state = CORE_SIG;
      break;
    }

    case CORE_SIG:
      sig   = Base64().decode(req.getInput());
      state = CORE_DOWNLOAD;
      break;

    case CORE_DOWNLOAD: downloadResponse(req.getInput()); break;
    default: THROW("Unexpected core state: " << state);
    }

    nextEvent->activate();
    return;
  } CATCH_ERROR;

  // TODO retry on error
}
