#!/usr/bin/env python3

import os
import subprocess
import sys


def main():
    env = os.environ.copy()
    subprocess.check_call(['scons', 'build/tests/fah-client-tests'], env=env)
    sys.exit(subprocess.call(['./build/tests/fah-client-tests'], env=env))


if __name__ == '__main__':
    main()
