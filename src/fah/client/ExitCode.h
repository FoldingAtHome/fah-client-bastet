/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2023, foldingathome.org
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
#ifndef FAH_EXIT_CODE
#define FAH_EXIT_CODE

/***
 * Core exit codes
 *
 * The version related information is from information gathered from source
 * code from the following clients: v322, v400, v500, v504, v600 & v623.
 *
 * Client actions:
 *   RETURN   - v322-v623: Return results to WS.
 *                         No FAILED indication from client.
 *                         Error counts set to zero.
 *              FINISHED_UNIT, BAD_WORK_UNIT
 *
 *   ERROR    - v322-v623: Increase core error count.
 *                         If error count > MAX_ERROR (2 or 3) UPDATE.
 *                         Otherwise retry.
 *              CORE_STARTUP_ERROR, SPECIAL_EXIT, BAD_ARGUMENTS
 *
 *   UPDATE   - v322-v623: Update core and restart unit.
 *              CORE_OUTDATED, CORE_IS_ABSENT
 *
 *   EXIT     - v322-v623: Client exits.
 *              SMP_MISMATCH, INTERRUPTED
 *
 *   RESTART  - v322-v623: Restarts core no error.
 *              CORE_RESTART
 *
 *   DUMP     - v322-v623: Delete unit and get a new one.
 *              BAD_FILE_FORMAT, BAD_FRAME_CHECKSUM, BAD_CORE_FILES,
 *              MISSING_WORK_FILES, FILE_IO_ERROR
 *
 *   DEFAULT  - v322-v600: DUMP and ERROR.
 *              v623:      If SMP then DUMP and ERROR else EXIT.
 *              CLIENT_DIED, BAD_WORK_CHECKSUM, MALLOC_ERROR, UNKNOWN_ERROR
 *
 *   FAIL     - v623:      Return results to WS as FAILED by client.
 *                         Error counts set to zero.
 *              FAILED_1, FAILED_2, FAILED_3, EARLY_UNIT_END
 *
 *   UNSTABLE - v623:      Return unit to WS no FAILED indication from client.
 *                         Increase EUE count.
 *                         If EUE count > 5 pause for 24 hours.
 *              UNSTABLE_MACHINE
 *
 * Return codes that are not supported by a particular client version will
 * hit the DEFAULT case.
 */

#define CBANG_ENUM_NAME ExitCode
#define CBANG_ENUM_PATH fah/client
#define CBANG_ENUM_NAMESPACE FAH
#define CBANG_ENUM_NAMESPACE2 Client
#include <cbang/enum/MakeEnumeration.def>

#endif // FAH_EXIT_CODE_H
#else // CBANG_ENUM

/// v623, FAIL:
CBANG_ENUM_VALUE(FAILED_1,                  0)

/// v623, FAIL:
CBANG_ENUM_VALUE(FAILED_2,                  1)

/// v600-v623, EXIT: Incorrect SMP settings for work unit.
CBANG_ENUM_VALUE(SMP_MISMATCH,              97)

/// v322-v623, RESTART: Settings changed: priority, cpu, etc.
CBANG_ENUM_VALUE(CORE_RESTART,              98)

/// v322-v623, ERROR: Error trying to start core
CBANG_ENUM_VALUE(CORE_STARTUP_ERROR,        99)

/// v322-v623, RETURN: Work unit completed
CBANG_ENUM_VALUE(FINISHED_UNIT,            100)

/// v322-v623, ERROR: Finished a special function, e.g. delete files
CBANG_ENUM_VALUE(SPECIAL_EXIT,             101)

/// v322-v623, EXIT: Interrupt signal received
CBANG_ENUM_VALUE(INTERRUPTED,              102)

/// v623-v623, DEFAULT: Lifeline from client no longer detected
CBANG_ENUM_VALUE(CLIENT_DIED,              103)

/// v322-v623, UPDATE: Core out of date
CBANG_ENUM_VALUE(CORE_OUTDATED,            110)

/// v322-v623, DUMP: Work file format not recognized
CBANG_ENUM_VALUE(BAD_FILE_FORMAT,          111)

/// v322-v623, DUMP: Frame file checksums are incorrect
CBANG_ENUM_VALUE(BAD_FRAME_CHECKSUM,       112)

/// v322-v623, DUMP: Core files are corrupt
CBANG_ENUM_VALUE(BAD_CORE_FILES,           113)

/// v322-v623, RETURN: Error on received work unit
CBANG_ENUM_VALUE(BAD_WORK_UNIT,            114)

/// v322-v623, ERROR: Bad command line arguments
CBANG_ENUM_VALUE(BAD_ARGUMENTS,            115)

/// v322-v623, DUMP: Work files with given path & suffix not found
CBANG_ENUM_VALUE(MISSING_WORK_FILES,       116)

/// v322-v623, DUMP: Problem opening, closing or deleting a file
CBANG_ENUM_VALUE(FILE_IO_ERROR,            117)

/// v322-v623, DEFAULT: Received work packet has bad checksum
CBANG_ENUM_VALUE(BAD_WORK_CHECKSUM,        118)

/// v322-v623, DEFAULT: Failed to allocate memory
CBANG_ENUM_VALUE(MALLOC_ERROR,             119)

/// v322-v623, UPDATE: Core is not present or has zero size.
CBANG_ENUM_VALUE(CORE_IS_ABSENT,           120)

/// v322-v623, DEFAULT: Unknown error
CBANG_ENUM_VALUE(UNKNOWN_ERROR,            121)

/// v623, UNSTABLE: Cannot get to 1% on WUs
CBANG_ENUM_VALUE(UNSTABLE_MACHINE,         122)

/// v623, FAIL:
CBANG_ENUM_VALUE(EARLY_UNIT_END,           123)

/// v710, FAIL:
CBANG_ENUM_VALUE(GPU_MEMTEST_ERROR,        124)

/// v710, FAIL:
CBANG_ENUM_VALUE(GPU_INITIALIZATION_ERROR, 125)

/// v710, FAIL:
CBANG_ENUM_VALUE(GPU_UNAVAILABLE_ERROR,    126)

/// v745, FAIL:
CBANG_ENUM_VALUE(WU_STALLED,               127)

/// v623, FAIL:
CBANG_ENUM_VALUE(FAILED_3,                 255)

#endif // CBANG_ENUM
