/**
 * @brief PrBoomTV version: MAJOR.MINOR.PATCH.
 *
 * MAJOR is the base project's (PrBoom's PACKAGE_VERSION), MINOR is
 * PrBoomTV's own version, set here by hand, and PATCH is the date of the
 * libretro-prboom commit this core is based on, as yymmdd (261005 is
 * 2026-10-05). MAJOR and PATCH come from the build (cmake/prboomtv.cmake).
 *
 * Netplay needs the same version on every machine: it is the core's
 * version for the frontend, the netpacket protocol version, and what a
 * session's HELLO is checked against.
 */
#ifndef DOPO_VERSION_H
#define DOPO_VERSION_H

#define DOPO_VERSION_MINOR 1

#if !defined(DOPO_VERSION_MAJOR) || !defined(DOPO_VERSION_PATCH)
#error "DOPO_VERSION_MAJOR and DOPO_VERSION_PATCH come from cmake/prboomtv.cmake"
#endif

#define DOPO_VERSION_STR2(x) #x
#define DOPO_VERSION_STR(x)  DOPO_VERSION_STR2(x)

/** @brief "MAJOR.MINOR.PATCH", e.g. "2.1.261005". */
#define DOPO_VERSION \
  DOPO_VERSION_STR(DOPO_VERSION_MAJOR) "." \
  DOPO_VERSION_STR(DOPO_VERSION_MINOR) "." \
  DOPO_VERSION_STR(DOPO_VERSION_PATCH)

#endif
