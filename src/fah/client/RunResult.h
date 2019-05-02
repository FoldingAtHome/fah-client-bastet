/******************************************************************************\

                    Copyright 2001-2016. Stanford University.
                              All Rights Reserved.

                  For information regarding this software email:
                                 Joseph Coffland
                          joseph@cauldrondevelopment.com

\******************************************************************************/

#ifndef CBANG_ENUM
#ifndef FAH_RUN_RESULT_H
#define FAH_RUN_RESULT_H

#define CBANG_ENUM_NAME RunResult
#define CBANG_ENUM_NAMESPACE FAH
#define CBANG_ENUM_NAMESPACE2 Client
#define CBANG_ENUM_PREFIX 4
#define CBANG_ENUM_PATH fah/client
#include <cbang/enum/MakeEnumeration.def>

#endif // FAH_RUN_RESULT_H
#else // CBANG_ENUM

CBANG_ENUM(RUN_FINISHED)
CBANG_ENUM(RUN_FAILED)
CBANG_ENUM(RUN_RESTART)
CBANG_ENUM(RUN_INTERRUPTED)
CBANG_ENUM(RUN_CORE_ERROR)
CBANG_ENUM(RUN_FATAL_ERROR)
CBANG_ENUM(RUN_ERROR)
CBANG_ENUM(RUN_CRASHED)
CBANG_ENUM(RUN_KILLED)

#endif // CBANG_ENUM
