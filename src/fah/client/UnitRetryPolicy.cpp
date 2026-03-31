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

#include "UnitRetryPolicy.h"

#include <algorithm>
#include <cmath>

using namespace FAH::Client;


UnitRetryDecision UnitRetryPolicy::evaluate(UnitState state, unsigned retries) {
  UnitRetryDecision decision;

  // Assignment can retry indefinitely because no downloaded WU state is at
  // risk, while upload gets a much larger retry ceiling to protect completed
  // results from transient network failures.
  if (retries < 10 || state == UnitState::UNIT_ASSIGN ||
      (retries <= 50 && UnitState::UNIT_UPLOAD <= state)) {
    decision.delay = std::pow(2, std::min(9U, retries));
    return decision;
  }

  decision.fail = true;
  return decision;
}
