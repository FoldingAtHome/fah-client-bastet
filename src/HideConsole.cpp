/******************************************************************************\

                     Copyright 2017-2020. foldingathome.org.
                    Copyright 2001-2017. Stanford University.
                               All Rights Reserved.

                  For information regarding this software email:
                                 Joseph Coffland
                          joseph@cauldrondevelopment.com

\******************************************************************************/

#include <cbang/os/Subprocess.h>
#include <cbang/Catch.h>

#include <vector>
#include <string>

using namespace std;
using namespace cb;


int main(int argc, char *argv[]) {
  try {
    if (argc < 2) THROW("Missing arguments");

    // Args
    vector<string> args;
    for (int i = 1; i < argc; i++)
      args.push_back(argv[i]);

    Subprocess().exec(args, Subprocess::W32_HIDE_WINDOW);
    return 0;

  } CATCH_ERROR;

  return 1;
}
