/**
 * @file export.hpp
 * @brief Shared/static library export macros.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#if defined(COMETTEXTEL_STATIC)
#  define COMETTEXTEL_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#  if defined(COMETTEXTEL_BUILDING)
#    define COMETTEXTEL_API __declspec(dllexport)
#  else
#    define COMETTEXTEL_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define COMETTEXTEL_API __attribute__((visibility("default")))
#else
#  define COMETTEXTEL_API
#endif
