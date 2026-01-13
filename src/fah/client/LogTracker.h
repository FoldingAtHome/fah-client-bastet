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

#pragma once

#include <cbang/log/LogLineListener.h>
#include <cbang/event/Event.h>
#include <cbang/json/List.h>

#include <list>
#include <vector>
#include <set>
#include <cstdint>


namespace FAH {
  namespace Client {
    class LogTracker : public cb::LogLineListener {
    public:
      class Listener {
      public:
        virtual ~Listener() {}
        virtual void logUpdate(
          const cb::JSON::ValuePtr &lines, uint64_t last) = 0;
      };

    private:
      cb::Event::EventPtr event;
      std::set<cb::SmartPointer<Listener>> listeners;

      static const unsigned maxLines = 1e5;
      typedef std::pair<uint64_t, std::string> entry_t;
      entry_t lines[maxLines];
      unsigned head  = 0;
      unsigned tail  = 0;
      unsigned last  = 0;
      uint64_t index = 0;

    public:
      LogTracker(cb::Event::Base &base);

      void add(const cb::SmartPointer<Listener> &listener, uint64_t last);
      void remove(const cb::SmartPointer<Listener> &listener);

    protected:
      // From cb::LogLineListener
      void writeln(const char *s) override;

      void update();
      void adv(unsigned &ptr);
    };
  }
}
