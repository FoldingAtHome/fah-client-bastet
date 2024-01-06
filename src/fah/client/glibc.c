/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2024, foldingathome.org
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

// Replace GLIBC functions or force linking to older versions
#if defined(__GNUC__) && !defined(__clang__) && !defined(_WIN32)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glob.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#if defined(__linux__)
#include <unistd.h>
#include <sys/syscall.h>
#endif

#if defined(__aarch64__)
#define GLIBC_SYMVER "2.17"
#else
#define GLIBC_SYMVER "2.2.5"
#endif

__asm__(".symver glob,glob@GLIBC_"     GLIBC_SYMVER);
__asm__(".symver logf,logf@GLIBC_"     GLIBC_SYMVER);
__asm__(".symver log,log@GLIBC_"       GLIBC_SYMVER);
__asm__(".symver expf,expf@GLIBC_"     GLIBC_SYMVER);
__asm__(".symver exp,exp@GLIBC_"       GLIBC_SYMVER);
__asm__(".symver powf,powf@GLIBC_"     GLIBC_SYMVER);
__asm__(".symver pow,pow@GLIBC_"       GLIBC_SYMVER);
__asm__(".symver fcntl64,fcntl@GLIBC_" GLIBC_SYMVER);


int getentropy(void *buf, size_t length) {
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd) {
    int ret = read(fd, buf, length);
    close(fd);
    return ret == length ? 0 : -1;
  }

  errno = ENOSYS;
  return -1;
}


ssize_t getrandom(void *buf, size_t length, unsigned flags) {
#if defined(__linux__)
  return syscall(SYS_getrandom, buf, length, flags);

#else
  errno = ENOSYS;
  return -1;
#endif
}


typedef int (*errfunc_t)(const char *, int);
int __wrap_glob(const char *pattern, int flags, errfunc_t func, glob_t *pglob) {
  return glob(pattern, flags, func, pglob);
}


float  __wrap_logf(float x) {return logf(x);}
double __wrap_log(double x) {return  log(x);}
float  __wrap_expf(float x) {return expf(x);}
double __wrap_exp(double x) {return  exp(x);}
float  __wrap_powf(float x, float  y) {return powf(x, y);}
double __wrap_pow(double x, double y) {return  pow(x, y);}


int __wrap_fcntl64(int fd, int cmd, ...) {
  int result;
  va_list va;
  va_start(va, cmd);

  switch (cmd) {
  case F_GETFD:
  case F_GETFL:
  case F_GETOWN:
  case F_GETSIG:
  case F_GETLEASE:
  case F_GETPIPE_SZ:
  case F_GET_SEALS:
    va_end(va);
    return fcntl64(fd, cmd);

  case F_SETFD:
  case F_SETFL:
  case F_SETOWN:
  case F_SETSIG:
  case F_SETLEASE:
  case F_NOTIFY:
  case F_SETPIPE_SZ:
  case F_ADD_SEALS:
    result = fcntl64(fd, cmd, va_arg(va, int));
    va_end(va);
    return result;

  case F_GETLK:
    va_arg(va, struct flock *)->l_type = F_UNLCK;
    va_end(va);

  case F_SETLK:
  case F_SETLKW:
    return 0; // Silently disables file locking

  case F_OFD_SETLK:
  case F_OFD_SETLKW:
  case F_OFD_GETLK:
    errno = ENOTSUP;
    return -1;

  case F_GETOWN_EX:
  case F_SETOWN_EX:
    result = fcntl64(fd, cmd, va_arg(va, struct f_owner_ex *));
    va_end(va);
    return result;

  case F_GET_RW_HINT:
  case F_SET_RW_HINT:
  case F_GET_FILE_RW_HINT:
  case F_SET_FILE_RW_HINT:
    result = fcntl64(fd, cmd, va_arg(va, uint64_t*));
    va_end(va);
    return result;

  default:
    errno = EINVAL;
    return -1;
  }
}
#endif // __GNUC__
