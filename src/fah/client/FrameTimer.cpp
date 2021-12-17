/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2020, foldingathome.org
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

#include "FrameTimer.h"

#include "Unit.h"

#include <cbang/Exception.h>
#include <cbang/Math.h>
#include <cbang/log/Logger.h>
#include <cbang/time/Time.h>
#include <cbang/time/TimeInterval.h>
#include <cbang/config/Options.h>

using namespace cb;
using namespace FAH::Client;


FrameTimer::FrameTimer(Unit &unit) :
  unit(unit), totalFrames(0), currentFrame(0), currentTime(0), lastUpdate(0),
  lastTimeUpdate(0), lastFrameUpdate(0), running(false) {}


uint64_t FrameTimer::getCurrentFrameTime() const {
  uint64_t now = Time::now();
  if (running) {
    return currentTime + now - lastTimeUpdate;
  }
  return currentTime;
}


bool FrameTimer::isStalled() const {
  // TODO: Check frame is stalled.
  return false;
}


double FrameTimer::getCurrentFrameEstimatedProgress() const {
  double estimate = unit.getRunTimeEstimate();
  double frameProgress = estimate ? getCurrentFrameTime() / estimate: 0;
  if (0.01 < frameProgress) return 0.01;
  return frameProgress;
}


double FrameTimer::getProgress() const {
  if (!totalFrames) return 0;

  double progress = getKnownProgress() + getCurrentFrameEstimatedProgress();
  if (0.9999 < progress) progress = 0.9999; // Not more than 100%

  return progress;
}


double FrameTimer::getKnownProgress() const {
  return (totalFrames && currentFrame <= totalFrames) ?
    (double)currentFrame / (double)totalFrames : 0;
}

void FrameTimer::start() {
  if (running) LOG_ERROR("Frame timer already running");
  running = true;

  startTime = Time::now();
  currentTime = lastUpdate = lastTimeUpdate = lastFrameUpdate = 0;
}


void FrameTimer::stop() {
  if (!running) LOG_ERROR("Frame timer not running");
  running = false;
  if (lastTimeUpdate) {
    uint64_t now = Time::now();
    currentTime += now - lastTimeUpdate;
    lastTimeUpdate = now;
  }
}


void FrameTimer::update(uint64_t frame, uint64_t total) {
  if (!running) {
    LOG_ERROR("Frame timer not running, starting it");
    start();
    return;
  }
  if (!total || total < frame) return;
  uint64_t now = Time::now();
  totalFrames = total;

  // Check for start
  if (!lastUpdate) lastUpdate = lastTimeUpdate = now;
  else {
    // Detect and adjust for clock skew
    int64_t delta = (int64_t)now - lastUpdate;
    if (60 < delta) {
      LOG_WARNING("Detected clock skew (" << TimeInterval(delta)
                  << "), I/O delay, laptop hibernation or other slowdown "
                  "noted, adjusting time estimates");
      currentTime += lastUpdate - lastTimeUpdate;
      lastTimeUpdate = now;
    }
    lastUpdate = now;
  }

  if (currentFrame != frame) {
    lastFrameUpdate = now;
    unit.adjustRuntimeEstimate(getCurrentFrameTime() * 100);
    currentFrame = frame;
    lastTimeUpdate = now;
    currentTime = 0;
  }
}
