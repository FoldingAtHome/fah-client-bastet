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

#include "Group.h"
#include "App.h"
#include "OS.h"
#include "Config.h"
#include "GPUResources.h"

#include <cbang/util/Resource.h>
#include <cbang/log/Logger.h>
#include <cbang/json/Reader.h>

#include <cmath>

using namespace std;
using namespace cb;
using namespace FAH::Client;

namespace FAH {namespace Client {extern const DirectoryResource resource0;}}

#undef CBANG_LOG_PREFIX
#define CBANG_LOG_PREFIX (name.empty() ? "Default" : name) << ":"


Group::Group(App &app, const string &name) :
  app(app), name(name),
  event(app.getEventBase().newEvent([this] {update();}, 0)) {
  auto &r       = FAH::Client::resource0.get("group.json");
  auto defaults = JSON::Reader::parse(r);
  config        = new Config(app, defaults);
  auto &db      = app.getDB("groups");

  if (name.empty()) {
    config->load(app.getOptions());
    config->insert("cpus", app.getOptions()["cpus"].toInteger());
  }

  if (db.has(name)) config->load(*db.getJSON(name));

  insert("config", config);

  triggerUpdate();
}


Group::Units Group::units() const {return Units(*app.getUnits(), name);}


void Group::setState(const JSON::Value &msg) {
  bool wasPaused = config->getPaused();
  config->setState(msg);
  if (wasPaused && !config->getPaused()) clearErrors();
  triggerUpdate();
}


bool Group::waitForIdle() const {
  return config->getOnIdle() && !app.getOS().isSystemIdle();
}


bool Group::waitOnBattery() const {
  return !config->getOnBattery() && app.getOS().isOnBattery();
}


bool Group::waitOnGPU() const {
  // Returns true if this group has enabled any GPUs which have not yet
  // been detected as active and supported.

  auto &gpus = *config->get("gpus");

  for (auto &id: gpus.keys())
    if (config->isGPUEnabled(id) && app.getGPUs().waitOnGPU(id))
      return true;

  return false;
}


bool Group::keepAwake() const {return config->getKeepAwake() && isActive();}


bool Group::isActive() const {
  for (auto unit: units())
    if (!unit->isPaused()) return true;

  return false;
}


bool Group::isAssigning() const {
  for (auto unit: units())
    if (unit->isAssigning()) return true;

  return false;
}


void Group::triggerUpdate() {if (!event->isPending()) event->activate();}


void Group::shutdown(function<void ()> cb) {
  shutdownCB = cb;
  triggerUpdate();
}


void Group::clearErrors() {
  lostWUs  = 0;
  failures = 0;
  setWait(0);
  insert("failed_wus", 0);
  insert("lost_wus",   0);
  insert("failed", "");
}


void Group::unitComplete(const string &reason, bool downloaded) {
  if (reason == "credited") clearErrors();
  else {
    if (reason != "dumped" && reason != "aborted") {
      insert("failed_wus", ++failures);
      setWait(std::pow(2, std::min(failures, 10U)));
    }

    if (downloaded) {
      insert("lost_wus", ++lostWUs);

      if (4 < lostWUs) {
        insert("failed", "Paused due too many failed Work Units.");
        config->setPaused(true);
      }
    }
  }

  triggerUpdate();
}



void Group::save()   {app.getDB("groups").set(name, config->toString());}
void Group::remove() {app.getDB("groups").unset(name);}


void Group::notify(const list<JSON::ValuePtr> &change) {
  // Automatically save changes to config
  bool isConfig = 2 < change.size() && change.front()->getString() == "config";

  if (isConfig) {
    save();
    triggerUpdate();
  }
}


void Group::update() {
  // Trigger unit updates
  for (auto unit: units())
    unit->triggerNext();

  // Remove completed units
  std::set<string> completed;
  for (auto unit: units())
    if (unit->getState() == UnitState::UNIT_DONE)
      completed.insert(unit->getID());

  for (auto &id: completed)
    app.getUnits()->removeUnit(id);

  // Handle graceful shutdown
  if (app.shouldQuit()) {
    for (auto unit: units())
      if (unit->isRunning())
        return event->add(0.25); // Check again later

    if (shutdownCB) {
      // Save state to DB
      for (auto unit: units())
        unit->save();

      shutdownCB();
      shutdownCB = 0;
    }

    return;
  }

  // No further action if waiting
  if (config->getPaused() || waitForIdle() || waitOnBattery() || waitOnGPU() ||
      isAssigning() || Time::now() < waitUntil)
    return event->add(0.25); // Check again later

  // Allocate resources
  unsigned         remainingCPUs = config->getCPUs();
  std::set<string> remainingGPUs = config->getGPUs();
  std::set<string> enabledWUs;

  // Allocate GPUs with minimum CPU requirements
  for (auto unit: units()) {
    if (UNIT_RUN < unit->getState()) continue;

    auto unitGPUs = unit->getGPUs();
    if (unitGPUs.empty()) continue;

    uint32_t minCPUs  = unit->getMinCPUs();
    bool     runnable = minCPUs <= remainingCPUs || minCPUs < 2;

    std::set<string> gpusWithWU = remainingGPUs;
    for (auto id: unitGPUs) runnable &= gpusWithWU.erase(id) != 0;

    if (runnable) {
      remainingGPUs = gpusWithWU;
      remainingCPUs -= min(remainingCPUs, minCPUs); // Allocate minimum CPUs
      enabledWUs.insert(unit->getID());
    }
  }

  // Allocate extra CPUs to enabled GPU WUs
  for (auto unit: units()) {
    // GPU WUs that were enabled above
    if (!enabledWUs.count(unit->getID())) continue;

    uint32_t minCPUs = unit->getMinCPUs();
    uint32_t maxCPUs = unit->getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs + minCPUs);

    unit->setCPUs(cpus);
    // minCPUs was subtracted above or remainingCPUs is already zero
    remainingCPUs -= min(remainingCPUs, cpus - minCPUs);
  }

  // Allocate remaining CPUs to existing CPU WUs
  for (auto unit: units()) {
    if (unit->hasGPUs() || remainingCPUs < unit->getMinCPUs() ||
        UNIT_RUN < unit->getState()) continue;

    uint32_t maxCPUs = unit->getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs);

    unit->setCPUs(cpus);
    remainingCPUs -= cpus;
    enabledWUs.insert(unit->getID());
  }

  // Start and stop WUs, based on resource availability
  unsigned wuCount = 0;
  for (auto unit: units()) {
    wuCount++;
    bool pause = unit->atRunState() && !enabledWUs.count(unit->getID());
    unit->setPause(pause);
    LOG_DEBUG(3, "Unit " << unit->getID() << (pause ? " paused" : " enabled"));
  }

  // Report allocation status
  LOG_DEBUG(1, "Remaining CPUs: " << remainingCPUs << ", Remaining GPUs: "
    << remainingGPUs.size() << ", Active WUs: " << enabledWUs.size());

  // Handle finish, don't add any new WUs
  if (config->getFinish()) {
    if (!wuCount) config->setPaused(true);
    return;
  }

  // Add new WU if we don't already have too many and there are some resources
  const unsigned maxWUs = config->getGPUs().size() + config->getCPUs() / 64 +
    app.getOptions()["max-uploads"].toInteger();
  if (wuCount < maxWUs && (remainingCPUs || remainingGPUs.size())) {
    app.getUnits()->add(
      new Unit(app, name, app.getNextWUID(), remainingCPUs, remainingGPUs));
    LOG_INFO(1, "Added new work unit: cpus:" << remainingCPUs << " gpus:"
      << String::join(remainingGPUs, ","));
    triggerUpdate();
  }
}


void Group::setWait(double delay) {
  waitUntil = Time::now() + delay;
  insert("wait", Time(waitUntil).toString());
}
