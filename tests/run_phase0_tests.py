#!/usr/bin/env python3

import os
import subprocess
import sys


def main():
    env = os.environ.copy()
    scons = env.get('SCONS_EXECUTABLE', 'scons')
    # Build first so `scons test` works from a clean checkout and in GitHub
    # Actions without requiring a separate manual build step.
    # Clean the Phase 0 target first so local toolchain or architecture changes
    # do not leave behind incompatible object files between runs.
    subprocess.check_call([scons, '-c', 'build/tests/fah-client-tests'], env=env)
    subprocess.check_call([scons, 'build/tests/fah-client-tests'], env=env)
    sys.exit(subprocess.call(['./build/tests/fah-client-tests'], env=env))


if __name__ == '__main__':
    main()
