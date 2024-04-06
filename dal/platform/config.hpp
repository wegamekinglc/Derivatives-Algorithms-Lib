//
// Created by wegam on 2023/2/18.
//

#pragma once

#ifndef USE_AADET
#ifndef USE_CODI
#ifndef USE_XAD
#define USE_AADET
#endif
#endif
#endif

#ifndef USE_CODI
// #define USE_CODI
#endif

#ifndef USE_XAD
// #define XAD_USE_STRONG_INLINE
// #define USE_XAD
#endif

#ifndef DAL_USE_REQUIRE
#define DAL_USE_REQUIRE
#endif

#ifndef DAL_USE_NOTE
#define DAL_USE_NOTE
#endif

#ifdef WIN32
#define USE_EXCEL_REPORT
#endif