/* SPDX-FileCopyrightText: 1991, 1992, 1993, 1996, 1997 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */
#ifdef WIN32

/* Maintained by GLIBC. */
/* clang-format off */

/* Enable GNU extensions in fnmatch.h. */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE    1
#endif

#include <ctype.h>
#include <errno.h>

#include "BLI_fnmatch.h"


/* Comment out all this code if we are using the GNU C Library, and are not
 * actually compiling the library itself.  This code is part of the GNU C
 * Library, but also included in many other GNU distributions.  Compiling
 * and linking in this code is a waste when using the GNU C library
 * (especially if it is a shared library).  Rather than having every GNU
 * program understand `configure --with-gnu-libc' and omit the object files,
 * it is simpler to just do this in the source for each such file. */

#if defined _LIBC || !defined __GNU_LIBRARY__


# if defined STDC_HEADERS || !defined isascii
#  define ISASCII(c) 1
# else
#  define ISASCII(c) isascii(c)
# endif

# define ISUPPER(c) (ISASCII (c) && isupper (c))


# ifndef errno
extern int errno;
# endif

/* Match STRING against the filename pattern PATTERN, returning zero if
   it matches, nonzero if not.  */
int
fnmatch (const char *pattern, const char *string, int flags)
{
  const char *p = pattern, *n = string;
  char c;

/* Note that this evaluates C many times.  */
# define FOLD(c) ((flags & FNM_CASEFOLD) && ISUPPER (c) ? tolower (c) : (c))

  while ((c = *p++) != '\0')
  {
    c = FOLD (c);

    switch (c)
  {
  case '?':
    if (*n == '\0')
    return FNM_NOMATCH;
    else if ((flags & FNM_FILE_NAME) && *n == '/')
    return FNM_NOMATCH;
    else if ((flags & FNM_PERIOD) && *n == '.' &&
       (n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
    return FNM_NOMATCH;
    break;

  case '\\':
    if (!(flags & FNM_NOESCAPE))
    {
      c = *p++;
      if (c == '\0')
    /* Trailing \ loses.  */
    return FNM_NOMATCH;
      c = FOLD (c);
    }
    if (FOLD (*n) != c)
    return FNM_NOMATCH;
    break;

  case '*':
    if ((flags & FNM_PERIOD) && *n == '.' &&
      (n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
    return FNM_NOMATCH;

    for (c = *p++; c == '?' || c == '*'; c = *p++)
    {
      if ((flags & FNM_FILE_NAME) && *n == '/')
    /* A slash does not match a wildcard under FNM_FILE_NAME.  */
    return FNM_NOMATCH;
      else if (c == '?')
    {
      /* A ? needs to match one character.  */
      if (*n == '\0')
      /* There isn't another character; no match.  */
      return FNM_NOMATCH;
      else
      /* One character of the string is consumed in matching
         this ? wildcard, so *??? won't match if there are
         less than three characters.  */
      ++n;
    }
    }

    if (c == '\0')
    return 0;

    {
    char c1 = (!(flags & FNM_NOESCAPE) && c == '\\') ? *p : c;
    c1 = FOLD (c1);
    for (--p; *n != '\0'; ++n)
      if ((c == '[' || FOLD (*n) == c1) &&
      fnmatch (p, n, flags & ~FNM_PERIOD) == 0)
    return 0;
    return FNM_NOMATCH;
    }

  case '[':
    {
    /* Nonzero if the sense of the character class is inverted.  */

    if (*n == '\0')
      return FNM_NOMATCH;

    if ((flags & FNM_PERIOD) && *n == '.' &&
    (n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
      return FNM_NOMATCH;

    bool is_not = (*p == '!' || *p == '^');
    if (is_not)
      ++p;

    c = *p++;
    for (;;)
      {
    char cstart = c, cend = c;

    if (!(flags & FNM_NOESCAPE) && c == '\\')
      {
      if (*p == '\0')
        return FNM_NOMATCH;
      cstart = cend = *p++;
      }

    cstart = cend = FOLD (cstart);

    if (c == '\0')
      /* [ (unterminated) loses.  */
      return FNM_NOMATCH;

    c = *p++;
    c = FOLD (c);

    if ((flags & FNM_FILE_NAME) && c == '/')
      /* [/] can never match.  */
      return FNM_NOMATCH;

    if (c == '-' && *p != ']')
      {
      cend = *p++;
      if (!(flags & FNM_NOESCAPE) && cend == '\\')
        cend = *p++;
      if (cend == '\0')
        return FNM_NOMATCH;
      cend = FOLD (cend);

      c = *p++;
      }

    if (FOLD (*n) >= cstart && FOLD (*n) <= cend)
      goto matched;

    if (c == ']')
      break;
      }
    if (!is_not)
      return FNM_NOMATCH;
    break;

    matched:;
    /* Skip the rest of the [...] that already matched.  */
    while (c != ']')
      {
    if (c == '\0')
      /* [... (unterminated) loses.  */
      return FNM_NOMATCH;

    c = *p++;
    if (!(flags & FNM_NOESCAPE) && c == '\\')
      {
      if (*p == '\0')
        return FNM_NOMATCH;
      /* XXX 1003.2d11 is unclear if this is right.  */
      ++p;
      }
      }
    if (is_not)
      return FNM_NOMATCH;
    }
    break;

  default:
    if (c != FOLD (*n))
    return FNM_NOMATCH;
  }

    ++n;
  }

  if (*n == '\0')
  return 0;

  if ((flags & FNM_LEADING_DIR) && *n == '/')
  /* The FNM_LEADING_DIR flag says that "foo*" matches "foobar/frobozz". */
  return 0;

  return FNM_NOMATCH;

# undef FOLD
}

#endif /* _LIBC or not __GNU_LIBRARY__. */

/* clang-format on */

#else

/* intentionally empty for UNIX */

#endif /* WIN32 */
