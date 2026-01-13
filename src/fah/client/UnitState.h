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

#ifndef CBANG_ENUM
#ifndef FAH_UNIT_STATE_H
#define FAH_UNIT_STATE_H

#define CBANG_ENUM_NAME UnitState
#define CBANG_ENUM_NAMESPACE FAH
#define CBANG_ENUM_NAMESPACE2 Client
#define CBANG_ENUM_PATH fah/client
#define CBANG_ENUM_PREFIX 5
#include <cbang/enum/MakeEnumeration.def>

#endif // FAH_UNIT_STATE_H
#else // CBANG_ENUM

CBANG_ENUM(UNIT_ASSIGN)
CBANG_ENUM(UNIT_DOWNLOAD)
CBANG_ENUM(UNIT_CORE)
CBANG_ENUM(UNIT_RUN)
CBANG_ENUM(UNIT_UPLOAD)
CBANG_ENUM(UNIT_DUMP)
CBANG_ENUM(UNIT_DONE)

#endif // CBANG_ENUM
