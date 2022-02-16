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

#include "PasskeyConstraint.h"

#include <cbang/Exception.h>
#include <cbang/String.h>

using namespace std;
using namespace cb;
using namespace FAH;


void PasskeyConstraint::validate(const string &value) const {
  if (value.empty()) return;
  if (value.length() != 32) THROW("Passkey must be 32 characters long");

  size_t pos = value.find_first_not_of("0123456789abcdefABCDEF");
  if (pos != string::npos)
    THROW("Found invalid character '" << String::escapeC(value.substr(pos, 1))
           << "' at position " << pos << " in passkey");
}


string PasskeyConstraint::getHelp() const {
  return "Passkey must be either empty of 32 characters long and can only "
    "contain hexadecimal characters.";
}
