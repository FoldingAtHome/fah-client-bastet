/******************************************************************************\

                    Copyright 2001-2016. Stanford University.
                              All Rights Reserved.

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

CBANG_ENUM(UNIT_DOWNLOAD)
CBANG_ENUM(UNIT_CORE)
CBANG_ENUM(UNIT_RUN)
CBANG_ENUM(UNIT_UPLOAD)
CBANG_ENUM(UNIT_DUMP)
CBANG_ENUM(UNIT_DELETE)

#endif // CBANG_ENUM
