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

#include "LogTracker.h"

#include <cbang/log/Logger.h>
#include <cbang/thread/SmartLock.h>

using namespace std;
using namespace cb;
using namespace FAH::Client;


LogTracker::LogTracker(Event::Base &base) :
  event(base.newEvent(this, &LogTracker::update, 0)) {}


void LogTracker::add(const cb::SmartPointer<Listener> &listener,
                     uint64_t lastLine) {
  listeners.insert(listener);

  auto newLines = SmartPtr(new JSON::List);
  {
    SmartLock lock(&Logger::instance());

    for (unsigned ptr = head; ptr != last; adv(ptr))
      if (lastLine < lines[ptr].first) {
        newLines->append(lines[ptr].second);
        lastLine = lines[ptr].first;
      }
  }

  if (!newLines->empty()) listener->logUpdate(newLines, lastLine);
}


void LogTracker::remove(const cb::SmartPointer<Listener> &listener) {
  listeners.erase(listener);
}


void LogTracker::writeln(const char *s) {
  lines[tail] = entry_t(index++, s);
  adv(tail);
  if (tail == head) adv(head);
  if (tail == last) adv(last);
  if (!event->isPending()) event->add(0.25);
}


void LogTracker::update() {
  uint64_t lastIndex = 0;
  auto newLines = SmartPtr(new JSON::List);
  {
    SmartLock lock(&Logger::instance());

    for (; last != tail; adv(last)) {
      newLines->append(lines[last].second);
      lastIndex = lines[last].first;
    }
  }

  if (newLines->size())
    for (auto &l: listeners)
      l->logUpdate(newLines, lastIndex);
}


void LogTracker::adv(unsigned &ptr) {if (++ptr == maxLines) ptr = 0;}
