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

#pragma once

#include <cbang/StdTypes.h>
#include <cbang/SmartPointer.h>


namespace FAH {

  namespace Client {
    class Unit;

    class FrameTimer {
      Unit &unit;
      unsigned totalFrames; ///< Total number of frames in WU
      unsigned currentFrame; ///< The current frame
      uint64_t currentTime; ///< The accumulated time on this frame
      uint64_t startTime; ///< Last time the frame timer was started
      uint64_t lastUpdate; ///< Last time update() was called
      uint64_t lastTimeUpdate; ///< Last time currentTime was updated
      uint64_t lastFrameUpdate; ///< Last time the frame count changed
      bool running;

    public:
      FrameTimer(Unit &unit);

      uint64_t getCurrentFrameTime() const;

      bool isRunning() const {return running;}
      bool isStalled() const;
      double getCurrentFrameEstimatedProgress() const;
      double getKnownProgress() const;
      double getProgress() const;

      void start();
      void stop();
      void update(uint64_t frame, uint64_t total);
    };
  }
}

