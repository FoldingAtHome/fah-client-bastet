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

#include "fah/client/PasskeyConstraint.h"
#include "fah/client/ExitCode.h"
#include "fah/client/UnitRetryPolicy.h"

#include <cbang/Exception.h>
#include <cbang/json/String.h>

#include <functional>
#include <iostream>
#include <string>


using namespace FAH;
using namespace FAH::Client;


namespace {
  unsigned failures = 0;


  void fail(const std::string &msg) {
    std::cerr << "FAIL: " << msg << std::endl;
    failures++;
  }


  void expect(bool condition, const std::string &msg) {
    if (!condition) fail(msg);
  }


  void expectThrows(const std::function<void()> &fn,
                    const std::string &expected) {
    try {
      fn();
      fail("Expected exception containing: " + expected);

    } catch (const cb::Exception &e) {
      std::string msg = e.getMessage();
      if (msg.find(expected) == std::string::npos)
        fail("Exception mismatch. Expected substring '" + expected +
             "', got '" + msg + "'");
    }
  }


  void testPasskeyConstraint() {
    // This is a good Phase 0 target because it exercises project-owned
    // validation logic without requiring network, filesystem, or process setup.
    PasskeyConstraint constraint;

    constraint.validate(cb::JSON::String(""));
    constraint.validate(cb::JSON::String("0123456789abcdef0123456789abcdef"));
    constraint.validate(cb::JSON::String("ABCDEF0123456789ABCDEF0123456789"));

    expectThrows(
      [&] {constraint.validate(cb::JSON::String("abcd"));},
      "32 characters long");
    expectThrows(
      [&] {
        constraint.validate(cb::JSON::String("0123456789abcdef0123456789abcdeg"));
      },
      "invalid character");
    expect(constraint.getHelp().find("32 characters long") != std::string::npos,
           "Passkey help text should mention 32 characters");
  }


  void testExitCode() {
    // The generated enum helpers are simple but correctness-critical glue for
    // process-exit handling elsewhere in the client.
    expect(ExitCode::parse("FINISHED_UNIT") == ExitCode::FINISHED_UNIT,
           "ExitCode should parse FINISHED_UNIT");
    expect(ExitCode::parse("WU_STALLED") == ExitCode::WU_STALLED,
           "ExitCode should parse WU_STALLED");
    expect(std::string(ExitCode(ExitCode::GPU_UNAVAILABLE_ERROR).toString()) ==
             "GPU_UNAVAILABLE_ERROR",
           "ExitCode should stringify GPU_UNAVAILABLE_ERROR");
    expect(!ExitCode::isValid((ExitCode::enum_t)42),
           "ExitCode value 42 should be invalid");
  }


  void testUnitRetryPolicy() {
    // This is the first extracted policy seam from Unit.cpp. It is pure logic
    // and captures retry thresholds that have direct user-visible behavior.
    auto assign = UnitRetryPolicy::evaluate(UnitState::UNIT_ASSIGN, 25);
    expect(!assign.fail, "Assignment retries should stay retryable past 10");
    expect(assign.delay == 512, "Assignment retry delay should cap at 512s");

    auto run = UnitRetryPolicy::evaluate(UnitState::UNIT_RUN, 9);
    expect(!run.fail, "Run retries below 10 should keep retrying");
    expect(run.delay == 512, "Ninth retry should use the capped delay");

    auto runFail = UnitRetryPolicy::evaluate(UnitState::UNIT_RUN, 10);
    expect(runFail.fail, "Run retries at 10 should fail the WU");
    expect(runFail.delay == 0, "Failure decisions should not carry a delay");

    auto upload = UnitRetryPolicy::evaluate(UnitState::UNIT_UPLOAD, 50);
    expect(!upload.fail, "Upload retries should allow a larger ceiling");
    expect(upload.delay == 512, "Upload retry delay should also cap at 512s");

    auto uploadFail = UnitRetryPolicy::evaluate(UnitState::UNIT_UPLOAD, 51);
    expect(uploadFail.fail, "Upload retries above 50 should fail the WU");
  }
}


int main() {
  testPasskeyConstraint();
  testExitCode();
  testUnitRetryPolicy();

  if (failures) {
    std::cerr << failures << " test assertion(s) failed" << std::endl;
    return 1;
  }

  std::cout << "All Phase 0 tests passed" << std::endl;
  return 0;
}
