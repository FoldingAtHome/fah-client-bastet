/******************************************************************************\

                    Copyright 2001-2016. Stanford University.
                              All Rights Reserved.

                  For information regarding this software email:
                                 Joseph Coffland
                          joseph@cauldrondevelopment.com

\******************************************************************************/

#ifndef CBANG_ENUM
#ifndef FAH_POWER_H
#define FAH_POWER_H

#define CBANG_ENUM_NAME Power
#define CBANG_ENUM_NAMESPACE FAH
#define CBANG_ENUM_NAMESPACE2 Client
#define CBANG_ENUM_PATH fah/client
#define CBANG_ENUM_PREFIX 6
#include <cbang/enum/MakeEnumeration.def>

#endif // FAH_POWER_H
#else // CBANG_ENUM

CBANG_ENUM(POWER_CUSTOM)
CBANG_ENUM(POWER_LIGHT)
CBANG_ENUM(POWER_MEDIUM)
CBANG_ENUM(POWER_FULL)

#endif // CBANG_ENUM
