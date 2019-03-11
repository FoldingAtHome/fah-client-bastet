/******************************************************************************\

                    Copyright 2001-2016. Stanford University.
                              All Rights Reserved.

                  For information regarding this software email:
                                 Joseph Coffland
                          joseph@cauldrondevelopment.com

\******************************************************************************/

#ifndef CBANG_ENUM
#ifndef FAH_COMPUTE_STATE_H
#define FAH_COMPUTE_STATE_H

#define CBANG_ENUM_NAME ComputeState
#define CBANG_ENUM_NAMESPACE FAH
#define CBANG_ENUM_NAMESPACE2 Client
#define CBANG_ENUM_PATH fah/client
#define CBANG_ENUM_PREFIX 8
#include <cbang/enum/MakeEnumeration.def>

#endif // FAH_COMPUTE_STATE_H
#else // CBANG_ENUM

CBANG_ENUM(COMPUTE_DISABLE)
CBANG_ENUM(COMPUTE_DELETE)
CBANG_ENUM(COMPUTE_IDLE)
CBANG_ENUM(COMPUTE_ASSIGN)
CBANG_ENUM(COMPUTE_RUN)
CBANG_ENUM(COMPUTE_FINISH)
CBANG_ENUM(COMPUTE_PAUSE)

#endif // CBANG_ENUM
