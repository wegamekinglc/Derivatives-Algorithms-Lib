#ifdef _WIN32

#define _Regex_traits _Regex_traits_Excel
#include <algorithm>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <public/_excel.hpp>
#include <public/_xlcall.hpp>
#include <functional>
#include <deque>

#include <dal/math/cellutils.hpp>
#include <public/_repository.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/numerics.hpp>

///***************************************************************************
// File:	FRAMEWRK.C
//
// Purpose:	Framework library for Microsoft Excel.
//
//     This library provides some basic functions
//     that help in writing Excel DLLs. It includes
//     simple functions for managing memory with XLOPER12s,
//     creating temporary XLOPER12s, robustly calling
//     Excel12(), and outputting debugging strings
//     to the debugger for the current application.
//
//     The main purpose of this library is to help
//     you to write cleaner C code for calling Excel.
//     For example, using the framework library you
//     can write
//
//         Excel12f(xlcDisplay, 0, 2, TempMissing12(), TempBool12(0));
//
//     instead of the more verbose
//
//         XLOPER12 xMissing, xBool;
//         xMissing.xltype = xltypeMissing;
//         xBool.xltype = xltypeBool;
//         xBool.val.xbool = 0;
//         Excel12(xlcDisplay, 0, 2, (LPXLOPER12) &xMissing, (LPXLOPER12) &xBool);
//
//
//     The library is non-reentrant.
//
//     Define _DEBUG to use the debugging functions.
//
//     Source code is provided so that you may
//     enhance this library or optimize it for your
//     own application.
//
// Platform:    Microsoft Windows
//
// Functions:
//              debugPrintf
//              GetTempMemory
//              FreeAllTempMemory
//              Excel
//              Excel12f
//              TempNum
//              TempNum12
//              TempStr
//              TempStrConst
//              TempStr12
//              TempBool
//              TempBool12
//              TempInt
//              TempInt12
//              TempErr
//              TempErr12
//
//***************************************************************************

#include <cstdarg>
#include <cwchar>
#include <iterator>
#include <malloc.h>
#include <map>
#include <mutex>
#include <regex>
#include <Windows.h>

#include "dal/math/matrix/matrixutils.hpp"
#include "dal/platform/optionals.hpp"
#include "dal/string/strings.hpp"
#include "dal/utilities/algorithms.hpp"
#include "dal/utilities/dictionary.hpp"

namespace Dal {
    using Matrix::M1x1;

    ///***************************************************************************
    // GetTempMemory()
    //
    // Purpose:
    //       Allocates temporary memory. Temporary memory
    //       can only be freed in one chunk, by calling
    //       FreeAllTempMemory(). This is done by Excel12f().
    //
    // Parameters:
    //
    //      size_t cBytes      How many bytes to allocate
    //
    // Returns:
    //
    //      LPSTR           A pointer to the allocated memory,
    //                      or 0 if more memory cannot be
    //                      allocated. If this fails,
    //                      check that MEMORYSIZE is big enough
    //                      in MemoryPool.h and that you have
    //                      remembered to call FreeAllTempMemory
    //
    // Comments:
    //
    //	Algorithm:
    //
    //      The memory allocation algorithm is extremely
    //      simple: on each call, allocate the next cBytes
    //      bytes of a static memory buffer. If the buffer
    //      becomes too full, simply fail. To free memory,
    //      simply reset the pointer (vOffsetMemBlock)
    //      back to zero. This memory scheme is very fast
    //      and is optimized for the assumption that the
    //      only thing you are using temporary memory
    //      for is to hold arguments while you call Excel12f().
    //      We rely on the fact that you will free all the
    //      temporary memory at the same time. We also
    //      assume you will not need more memory than
    //      the amount required to hold a few arguments
    //      to Excel12f().
    //
    //      Note that the memory allocation algorithm
    //      now supports multithreaded applications by
    //      giving each unique thread its own static
    //      block of memory. This is handled in the
    //      MemoryManager.cpp file automatically.
    //
    ///***************************************************************************

    namespace {
        static std::deque<String_>& TheTempMemory() { RETURN_STATIC(std::deque<String_>); }

        template <class T_> void FreeContents(T_& cell, bool force_free = false) {
            if (!(cell.xltype & xlbitDLLFree) && !force_free)
                return; // don't try to free cells returned by Empty()
            if (cell.xltype & xltypeMulti) {
                const int n = cell.val.array.rows * cell.val.array.columns;
                for (int ii = 0; ii < n; ++ii)
                    FreeContents(cell.val.array.lparray[ii], true);
                free(cell.val.array.lparray);
            }
            if (cell.xltype & xltypeStr)
                free(cell.val.str);
        }

        bool IsBlank(const OPER_& src) {
            if (src.xltype & (xltypeMissing | xltypeNil))
                return true;
            if (src.xltype & xltypeStr)
                return src.val.str[0] == 0;
            return false;
        }

        bool RowIsBlank(const OPER_* po, int i_row, int cols) {
            const OPER_* pStop = po + (i_row + 1) * cols;
            return std::find_if_not(&po[i_row * cols], pStop, IsBlank) == pStop;
        }
    } // namespace

    LPSTR GetTempMemory(size_t cBytes) {
        TheTempMemory().emplace_back(cBytes + 1, 'A');
        return const_cast<LPSTR>(TheTempMemory().back().c_str());
    }

    LPSTR GetMemoryForExcel(size_t bytes) { return (char*)malloc(bytes); }

    extern "C" __declspec(dllexport) void xlAutoFree(LPXLOPER p) {
        assert(p->xltype & xlbitDLLFree);
        FreeContents(*p);
        free(p);
    }

    extern "C" __declspec(dllexport) void xlAutoFree12(LPXLOPER12 p) {
        assert(p->xltype & xlbitDLLFree);
        FreeContents(*p);
        free(p);
    }

    ///***************************************************************************
    // FreeAllTempMemory()
    //
    // Purpose:
    //
    //      Frees all temporary memory that has been allocated
    //      for the current thread
    //
    // Parameters:
    //
    // Returns:
    //
    // Comments:
    //
    //
    ///***************************************************************************

    void FreeAllTempMemory(void) {
        std::deque<String_> empty;
        TheTempMemory().swap(empty);
    }

    ///***************************************************************************
    // Excel()
    //
    // Purpose:
    //      A fancy wrapper for the Excel4() function. It also
    //      does the following:
    //
    //      (1) Checks that none of the LPXLOPER arguments are 0,
    //          which would indicate that creating a temporary XLOPER
    //          has failed. In this case, it doesn't call Excel
    //          but it does print a debug message.
    //      (2) If an error occurs while calling Excel,
    //          print a useful debug message.
    //      (3) When done, free all temporary memory.
    //
    //      #1 and #2 require _DEBUG to be defined.
    //
    // Parameters:
    //
    //      int xlfn            Function number (xl...) to call
    //      LPXLOPER pxResult   Pointer to a place to stuff the result,
    //                          or 0 if you don't care about the result.
    //      int count           Number of arguments
    //      ...                 (all LPXLOPERs) - the arguments.
    //
    // Returns:
    //
    //      A return code (Some of the xlret... values, as defined
    //      in XLCALL.H, OR'ed together).
    //
    // Comments:
    //
    ///***************************************************************************

    ///***************************************************************************
    // TempNum()
    //
    // Purpose:
    //      Creates a temporary numeric (IEEE floating point) XLOPER.
    //
    // Parameters:
    //
    //      double d        The value
    //
    // Returns:
    //
    //      LPXLOPER        The temporary XLOPER, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    ///***************************************************************************

    OPER_* TempNum(double d) {
        OPER_* lpx = (OPER_*)GetTempMemory(sizeof(OPER_));
        if (lpx) {
            lpx->xltype = xltypeNum;
            lpx->val.num = d;
        }
        return lpx;
    }

    ///***************************************************************************
    // TempNum12()
    //
    // Purpose:
    //      Creates a temporary numeric (IEEE floating point) XLOPER12.
    //
    // Parameters:
    //
    //      double d        The value
    //
    // Returns:
    //
    //      LPXLOPER12      The temporary XLOPER12, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    //
    ///***************************************************************************

    LPXLOPER12 TempNum12(double d) {
        LPXLOPER12 lpx;

        lpx = (LPXLOPER12)GetTempMemory(sizeof(XLOPER12));

        if (!lpx) {
            return 0;
        }

        lpx->xltype = xltypeNum;
        lpx->val.num = d;

        return lpx;
    }

    ///***************************************************************************
    // TempStr()
    //
    // Purpose:
    //      Creates a temporary String_ XLOPER
    //
    // Parameters:
    //
    //      LPSTR lpstr     The String_, as a null-terminated
    //                      C String_, with the first byte
    //                      undefined. This function will
    //                      count the bytes of the String_
    //                      and insert that count in the
    //                      first byte of lpstr. Excel cannot
    //                      handle strings longer than 255
    //                      characters.
    //
    // Returns:
    //
    //      LPXLOPER        The temporary XLOPER, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    //      (1) This function has the side effect of inserting
    //          the byte count as the first character of
    //          the created String_.
    //
    //      (2) For highest speed, with constant strings,
    //          you may want to manually count the length of
    //          the String_ before compiling, and then avoid
    //          using this function.
    //
    //      (3) Behavior is undefined for non-null terminated
    //          input or strings longer than 255 characters.
    //
    // Note: If lpstr passed into TempStr is readonly, TempStr
    // will crash your XLL as it will try to modify a read only
    // String_ in place. strings declared on the stack as described below
    // are read only by default in VC++
    //
    // char *str = " I am a String_"
    //
    // Use extreme caution while calling TempStr on such strings. Refer to
    // VC++ documentation for complier options to ensure that these strings
    // are compiled as read write or use TempStrConst instead.
    //
    // TempStr is provided mainly for backwards compatability and use of
    // TempStrConst is encouraged going forward.
    //
    ///***************************************************************************

    LPXLOPER TempStr(LPSTR lpstr) {
        LPXLOPER lpx = (LPXLOPER)GetTempMemory(sizeof(XLOPER));
        if (lpx) {
            lpstr[0] = (BYTE)strlen(lpstr + 1);
            lpx->xltype = xltypeStr;
            lpx->val.str = lpstr;
        }
        return lpx;
    }

    namespace {
        void WriteToOper(const String_& src, OPER_* lpx, bool temp_memory = false) {
            static const int LIMIT = 1 << (8 * sizeof(OPER_::char_type));
            static const int MAX_SIZE = LIMIT - 1;
            const int size = Min(MAX_SIZE, static_cast<int>(src.size()));
            const int bytes = Max(size + 1, 256) * sizeof(OPER_::char_type); // don't allocate <256 characters
            lpx->val.str = (OPER_::char_type*)(temp_memory ? GetTempMemory(bytes) : GetMemoryForExcel(bytes));
            lpx->val.str[0] = (OPER_::char_type)(size);
            for (int ii = 0; ii < size; ++ii)
                lpx->val.str[ii + 1] = src[ii];
            lpx->xltype = xltypeStr;
        }
    } // namespace

    OPER_* TempStr(const String_& src) {
        OPER_* lpx = (OPER_*)GetTempMemory(sizeof(OPER_));
        if (lpx)
            WriteToOper(src, lpx, true);
        return lpx;
    }

    ///***************************************************************************
    // TempStrConst()
    //
    // Purpose:
    //      Creates a temporary String_ XLOPER from a
    //      const String_ with a local copy in temp memory
    //
    // Parameters:
    //
    //      LPSTR lpstr     The String_, as a null-terminated
    //                      C String_. This function will
    //                      count the bytes of the String_
    //                      and insert that count in the
    //                      first byte of the temp String_.
    //                      Excel cannot handle strings
    //                      longer than 255 characters.
    //
    // Returns:
    //
    //      LPXLOPER        The temporary XLOPER, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    //      Will take a String_ of the form "abc\0" and make a
    //      temp XLOPER of the form "\003abc"
    //
    //
    ///***************************************************************************

    LPXLOPER TempStrConst(const LPSTR lpstr) {
        LPXLOPER lpx;
        LPSTR lps;
        size_t len;

        len = strlen(lpstr);

        lpx = (LPXLOPER)GetTempMemory(sizeof(XLOPER) + len + 1);

        if (!lpx) {
            return 0;
        }

        lps = (LPSTR)lpx + sizeof(XLOPER);

        lps[0] = (BYTE)len;
        // can't strcpy_s because of removal of null-termination
        memcpy_s(lps + 1, len + 1, lpstr, len);
        lpx->xltype = xltypeStr;
        lpx->val.str = lps;

        return lpx;
    }

    ///***************************************************************************
    // TempStr12()
    //
    // Purpose:
    //      Creates a temporary String_ XLOPER12 from a
    //      unicode const String_ with a local copy in
    //      temp memory
    //
    // Parameters:
    //
    //      XCHAR lpstr     The String_, as a null-terminated
    //                      unicode String_. This function will
    //                      count the bytes of the String_
    //                      and insert that count in the
    //                      first byte of the temp String_.
    //
    // Returns:
    //
    //      LPXLOPER12      The temporary XLOPER12, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    //      (1) Fix for const String_ pointers being passed in to TempStr.
    //          Note it assumes NO leading space
    //
    //      (2) Also note that XLOPER12 now uses unicode for the String_
    //          operators
    //
    //      (3) Will remove the null-termination on the String_
    //
    //
    //
    // Note: TempStr12 is different from TempStr and is more like TempStrConst
    // in its behavior. We have consciously made this choice and deprecated the
    // behavior of TempStr going forward. Refer to the note in comment section
    // for TempStr to better understand this design decision.
    ///***************************************************************************

    LPXLOPER12 TempStr12(const XCHAR* lpstr) {
        LPXLOPER12 lpx;
        XCHAR* lps;
        int len;

        len = lstrlenW(lpstr);

        lpx = (LPXLOPER12)GetTempMemory(sizeof(XLOPER12) + (len + 1) * 2);

        if (!lpx) {
            return 0;
        }

        lps = (XCHAR*)((CHAR*)lpx + sizeof(XLOPER12));

        lps[0] = (BYTE)len;
        // can't wcscpy_s because of removal of null-termination
        wmemcpy_s(lps + 1, len + 1, lpstr, len);
        lpx->xltype = xltypeStr;
        lpx->val.str = lps;

        return lpx;
    }

    ///***************************************************************************
    // TempBool()
    //
    // Purpose:
    //      Creates a temporary logical (true/false) XLOPER.
    //
    // Parameters:
    //
    //      int b           0 - for a FALSE XLOPER
    //                      Anything else - for a TRUE XLOPER
    //
    // Returns:
    //
    //      LPXLOPER        The temporary XLOPER, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    ///***************************************************************************

    LPXLOPER TempBool(int b) {
        LPXLOPER lpx;

        lpx = (LPXLOPER)GetTempMemory(sizeof(XLOPER));

        if (!lpx) {
            return 0;
        }

        lpx->xltype = xltypeBool;
#ifdef __cplusplus
        lpx->val.xbool = b ? 1 : 0;
#else
        lpx->val.bool = b ? 1 : 0;
#endif

        return lpx;
    }

    ///***************************************************************************
    // TempBool12()
    //
    // Purpose:
    //      Creates a temporary logical (true/false) XLOPER12.
    //
    // Parameters:
    //
    //      BOOL b          0 - for a FALSE XLOPER12
    //                      Anything else - for a TRUE XLOPER12
    //
    // Returns:
    //
    //      LPXLOPER12      The temporary XLOPER12, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    ///***************************************************************************

    LPXLOPER12 TempBool12(BOOL b) {
        LPXLOPER12 lpx;

        lpx = (LPXLOPER12)GetTempMemory(sizeof(XLOPER12));

        if (!lpx) {
            return 0;
        }

        lpx->xltype = xltypeBool;
        lpx->val.xbool = b ? 1 : 0;

        return lpx;
    }

    ///***************************************************************************
    // TempInt()
    //
    // Purpose:
    //      Creates a temporary integer XLOPER.
    //
    // Parameters:
    //
    //      short int i     The integer
    //
    // Returns:
    //
    //      LPXLOPER        The temporary XLOPER, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    ///***************************************************************************

    LPXLOPER TempInt(short int i) {
        LPXLOPER lpx;

        lpx = (LPXLOPER)GetTempMemory(sizeof(XLOPER));

        if (!lpx) {
            return 0;
        }

        lpx->xltype = xltypeInt;
        lpx->val.w = i;

        return lpx;
    }

    ///***************************************************************************
    // TempInt12()
    //
    // Purpose:
    //          Creates a temporary integer XLOPER12.
    //
    // Parameters:
    //
    //      int i           The integer
    //
    // Returns:
    //
    //      LPXLOPER12      The temporary XLOPER12, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    //      Note that the int oper has increased in size from
    //      short int up to int in the 12 opers
    //
    ///***************************************************************************

    LPXLOPER12 TempInt12(int i) {
        LPXLOPER12 lpx;

        lpx = (LPXLOPER12)GetTempMemory(sizeof(XLOPER12));

        if (!lpx) {
            return 0;
        }

        lpx->xltype = xltypeInt;
        lpx->val.w = i;

        return lpx;
    }

    ///***************************************************************************
    // TempErr()
    //
    // Purpose:
    //      Creates a temporary error XLOPER.
    //
    // Parameters:
    //
    //      WORD err        The error code. One of the xlerr...
    //                      constants, as defined in XLCALL.H.
    //                      See the Excel user manual for
    //                      descriptions about the interpretation
    //                      of various error codes.
    //
    // Returns:
    //
    //      LPXLOPER        The temporary XLOPER, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    ///***************************************************************************

    LPXLOPER TempErr(WORD err) {
        LPXLOPER lpx;

        lpx = (LPXLOPER)GetTempMemory(sizeof(XLOPER));

        if (!lpx) {
            return 0;
        }

        lpx->xltype = xltypeErr;
        lpx->val.err = err;

        return lpx;
    }

    ///***************************************************************************
    // TempErr12()
    //
    // Purpose:
    //      Creates a temporary error XLOPER12.
    //
    // Parameters:
    //
    //      int err         The error code. One of the xlerr...
    //                      constants, as defined in XLCALL.H.
    //                      See the Excel user manual for
    //                      descriptions about the interpretation
    //                      of various error codes.
    //
    // Returns:
    //
    //      LPXLOPER12      The temporary XLOPER12, or 0
    //                      if GetTempMemory() failed.
    //
    // Comments:
    //
    //      Note the paramater has changed from a WORD to an int
    //      in the new 12 operators
    //
    ///***************************************************************************

    LPXLOPER12 TempErr12(int err) {
        LPXLOPER12 lpx;

        lpx = (LPXLOPER12)GetTempMemory(sizeof(XLOPER12));

        if (!lpx) {
            return 0;
        }

        lpx->xltype = xltypeErr;
        lpx->val.err = err;

        return lpx;
    }

    //----------------------------------------------------------------------------

    double Excel::ToDouble(const OPER_* src) {
        switch (src->xltype) {
        case xltypeInt:
        case xltypeNum:
            return src->val.num;
        case xltypeMulti:
            break; //
        default:
            THROW("Invalid input type -- number expected");
        }
        // ok, it's a multi
        REQUIRE(src->val.array.rows * src->val.array.columns == 1, "Invalid input size -- scalar expected");
        return ToDouble(&src->val.array.lparray[0]);
    }

    boost::optional<double> Excel::ToDouble(const OPER_* src, bool) {
        if (!(src->xltype & (xltypeMissing | xltypeNil)))
            return ToDouble(src);
        return boost::optional<double>();
    }

    int Excel::ToInt(const OPER_* src) {
        switch (src->xltype) {
        case xltypeInt:
        case xltypeNum:
            break;
        case xltypeMulti:
            REQUIRE(src->val.array.rows * src->val.array.columns == 1, "Invalid input size -- scalar expected");
            return ToInt(&src->val.array.lparray[0]);
        default:
            THROW("Invalid input type -- integer expected");
        }
        const int retval = AsInt(src->val.num);
        REQUIRE(IsZero(src->val.num - retval), "Non-integer value where integer was expected");
        return retval;
    }

    boost::optional<int> Excel::ToInt(const OPER_* src, bool) {
        boost::optional<int> retval;
        if (!(src->xltype & (xltypeMissing | xltypeNil)))
            retval = ToInt(src);
        return retval;
    }

    bool Excel::ToBool(const OPER_* src) {
        switch (src->xltype) {
        case xltypeBool:
            return src->val.xbool != 0;
        case xltypeInt:
        case xltypeNum:
            REQUIRE(src->val.num == 0 || src->val.num == 1, "Can't convert a number (except 0 or 1) to boolean");
            return src->val.num != 0;
            break;
        case xltypeMulti:
            REQUIRE(src->val.array.rows * src->val.array.columns == 1, "Invalid input size -- scalar expected");
            return ToBool(&src->val.array.lparray[0]);
        default:
            THROW("Invalid input type -- boolean expected");
        }
    }

    boost::optional<bool> Excel::ToBool(const OPER_* src, bool) {
        boost::optional<bool> retval;
        if (!(src->xltype & (xltypeMissing | xltypeNil)))
            retval = ToBool(src);
        return retval;
    }

    Date_ Excel::ToDate(const OPER_* src) { return ToDate(src, false).get(); }
    boost::optional<Date_> Excel::ToDate(const OPER_* src, bool optional) {
        Cell_ c = ToCell(src, optional);
        if (Cell::IsEmpty(c)) {
            REQUIRE(optional, "Missing input date");
            return Date_();
        }
        if (Cell::IsDouble(c)) {
            double d = Cell::ToDouble(c);
            const int id = AsInt(d);
            REQUIRE(id == d, "Can't construct a date from a non-integer number");
            return Date::FromExcel(id);
        }
        return Cell::ToDate(c);
    }

    DateTime_ Excel::ToDateTime(const OPER_* src, bool optional) {
        Cell_ c = ToCell(src, optional);
        if (Cell::IsEmpty(c)) {
            REQUIRE(optional, "Missing input datetime");
            return DateTime_();
        }
        if (Cell::IsDouble(c)) {
            double d = Cell::ToDouble(c);
            const int id = AsInt(d);
            return DateTime_(Date::FromExcel(id), d - id);
        }
        return Cell::ToDateTime(c);
    }

    namespace {
        String_ FromPascalString(char* src) {
            char buf[256];
            memcpy(buf, src + 1, 255);
            const int size = *(reinterpret_cast<unsigned char*>(src));
            buf[size] = 0;
            return String_(buf);
        }
        String_ FromPascalString(wchar_t* src) {
            const int size = *src;
            char buf[65536];
            for (int ii = 0; ii < size; ++ii)
                buf[ii] = char(src[ii + 1]);
            buf[size] = 0;
            return String_(buf);
        }
    } // namespace

    String_ Excel::ToString(const OPER_* src, bool optional) {
        String_ retval;
        switch (src->xltype) {
        case xltypeMissing:
        case xltypeNil:
            REQUIRE(optional, "Missing input string");
            return String_();
        case xltypeStr:
            return FromPascalString(src->val.str);
        case xltypeMulti:
            break; //
        default:
            THROW("Invalid input type -- string expected");
        }
        // ok, it's a multi
        REQUIRE(src->val.array.rows * src->val.array.columns == 1, "Invalid input size -- scalar expected");
        return ToString(&src->val.array.lparray[0]);
    }

    Cell_ Excel::ToCell(const OPER_* src, bool optional) {
        if (IsBlank(*src)) {
            REQUIRE(optional, "Missing input cell");
            return Cell_();
        }

        switch (src->xltype) {
        case xltypeStr:
            return Cell_(FromPascalString(src->val.str));
        case xltypeInt:
        case xltypeNum:
            return Cell_(src->val.num);
        case xltypeBool:
            return Cell_(src->val.xbool != 0);
        case xltypeMulti:
            break; //
        default:
            THROW("Invalid input type -- can't form a cell");
        }
        // ok, it's a multi
        REQUIRE(src->val.array.rows * src->val.array.columns == 1, "Invalid input size -- scalar expected");
        return ToCell(&src->val.array.lparray[0]);
    }

    Dictionary_ Excel::ToDictionary(const OPER_* src, bool optional) {
        Dictionary_ retval;
        switch (src->xltype) {
        case xltypeMissing:
        case xltypeNil:
            return retval; // even a mandatory dictionary can be empty
        case xltypeMulti:
            break;
        default:
            THROW("Invalid input -- dictionary must be a two-column array of keys and values");
        }
        // ok, it's a multi
        REQUIRE(src->val.array.columns == 2, "Invalid input size -- dictionary must have two columns (keys, values)");
        for (int ir = 0; ir < src->val.array.rows; ++ir) {
            const OPER_& key = src->val.array.lparray[2 * ir];
            const OPER_& val = src->val.array.lparray[2 * ir + 1];
            if (!IsBlank(val))
                retval.Insert(ToString(&key), ToCell(&val));
            else if (IsBlank(key))
                break; // stop at a blank row
        }
        return retval;
    }

    Handle_<Storable_> Excel::ToHandleBase(_ENV, const OPER_* src, bool optional) {
        const String_ tag = ToString(src, optional);
        if (tag.empty()) {
            REQUIRE(optional, "Missing input handle");
            return Handle_<Storable_>();
        }
        auto repo = Environment::Find<ObjectAccess_>(_env);
        assert(repo);
        return repo->Fetch(tag);
    }

    namespace {
        template <class T_>
        auto ReadVector(const OPER_* src, bool optional, T_ read_scalar) -> Vector_<decltype(read_scalar(src))> {
            typedef decltype(read_scalar(src)) element_t;
            if (IsBlank(*src)) {
                REQUIRE(optional, "Missing input vector");
                return Vector_<element_t>();
            }
            if (!(src->xltype & xltypeMulti))
                return Vector::V1(read_scalar(src));

            // ok, it's a multi
            REQUIRE(src->val.array.rows == 1 || src->val.array.columns == 1,
                    "Invalid input size -- one-dimensional vector expected");
            Vector_<element_t> retval(src->val.array.rows * src->val.array.columns);
            for (int ii = 0; ii < retval.size(); ++ii) {
                if (IsBlank(src->val.array.lparray[ii])) {
                    retval.Resize(ii);
                    break;
                }
                retval[ii] = read_scalar(&src->val.array.lparray[ii]);
            }
            REQUIRE(!retval.empty() || optional, "Missing contents of input vector");
            return retval;
        }
    } // namespace

    Vector_<> Excel::ToDoubleVector(const OPER_* src, bool optional) {
        return ReadVector(src, optional, [](const OPER_* src) { return ToDouble(src); });
    }
    Vector_<int> Excel::ToIntVector(const OPER_* src, bool optional) {
        return ReadVector(src, optional, [](const OPER_* src) { return ToInt(src); });
    }
    Vector_<bool> Excel::ToBoolVector(const OPER_* src, bool optional) {
        return ReadVector(src, optional, [](const OPER_* src) { return ToBool(src); });
    }
    Vector_<Date_> Excel::ToDateVector(const OPER_* src, bool optional) {
        return ReadVector(src, optional, [](const OPER_* src) { return ToDate(src); });
    }
    Vector_<String_> Excel::ToStringVector(const OPER_* src, bool optional) {
        return ReadVector(src, optional, [](const OPER_* src) { return ToString(src); });
    }
    Vector_<Cell_> Excel::ToCellVector(const OPER_* src, bool optional) {
        return ReadVector(src, optional, [](const OPER_* src) { return ToCell(src); });
    }
    Vector_<Handle_<Storable_>> Excel::ToHandleBaseVector(_ENV, const OPER_* src, bool optional) {
        return ReadVector(src, optional, [&](const OPER_* src) { return ToHandleBase(_env, src, false); });
    }

    namespace {
        pair<int, int> NonblankSize(const OPER_* src) {
            const int nr = src->val.array.rows;
            const int nc = src->val.array.columns;
            auto data = src->val.array.lparray;

            // handle some special cases first
            if (nr * nc == 0)
                return make_pair(0, 0);
            if (nr == 1 || nc == 1) {
                auto size = AsInt(std::find_if(data, data + nr * nc, IsBlank) - data);
                return nr == 1 ? make_pair(1, size) : make_pair(size, 1);
            }
            // OK, it's at least 2x2
            if (IsBlank(data[0]) && IsBlank(data[1]) && IsBlank(data[nc])) // top corner and both adjacent are blank
                return make_pair(0, 0);
            int rows = 1, cols = 1;
            for (;;) {
                auto oldRows = rows, oldCols = cols;
                // increase rows as long as there is something visible, with the current column size
                for (rows = oldRows; rows < nr; ++rows) {
                    auto stop = &data[rows * nc + cols];
                    if (std::find_if_not(&data[rows * nc], stop, IsBlank) == stop)
                        break;
                }
                // increase columns
                for (cols = oldCols; cols < nc; ++cols) {
                    bool blank = true;
                    for (int ir = 0; blank && ir < rows; ++ir)
                        blank = IsBlank(data[ir * nc + cols]);
                    if (blank)
                        break;
                }
                if (rows == oldRows && cols == oldCols)
                    return make_pair(rows, cols);
            }
        }

        template <class E_>
        Matrix_<E_> ToMatrix(const OPER_* src, bool optional, bool greedy, E_ (*construct_element)(const OPER_*)) {
            Matrix_<E_> retval;
            switch (src->xltype) {
            case xltypeMulti:
                break;
            case xltypeMissing:
            case xltypeNil:
                REQUIRE(optional, "Missing input matrix");
                return retval; // empty
            default:
                retval.Resize(1, 1);
                retval(0, 0) = construct_element(src);
                return retval;
            }
            // ok, it's a multi
            auto size = greedy ? pair<int, int>(src->val.array.rows, src->val.array.columns) : NonblankSize(src);
            if (size.first * size.second == 0) {
                REQUIRE(optional, "Missing contents of matrix");
                return Matrix_<E_>();
            }
            retval.Resize(size.first, size.second);
            for (int ii = 0; ii < size.first; ++ii) {
                for (int jj = 0, kk = ii * src->val.array.columns; jj < size.second; ++jj, ++kk) {
                    retval(ii, jj) = construct_element(&src->val.array.lparray[kk]);
                }
            }
            return retval;
        }

        String_ StringMatrixEntry(const OPER_* src) { return Excel::ToString(src, true); }
        Cell_ CellMatrixEntry(const OPER_* src) { return Excel::ToCell(src, true); }

        String_ XError(const String_& msg, const char* arg_name) {
            static const String_ KNOCK_ON = "#Error:  Can't find object '#";
            String_ display("#Error:  " + msg);
            if (display.substr(0, KNOCK_ON.size()) == KNOCK_ON) {
                display =
                    display.substr(KNOCK_ON.size() - 1); // throw away the restatement of the fact that it's an error
                display.pop_back();                      // pop the trailing single-quote too
            } else if (arg_name)
                display += "(at input '" + String_(arg_name) + "')";
            return display;
        }
    } // namespace

    Matrix_<> Excel::ToDoubleMatrix(const OPER_* src, bool optional) {
        return ToMatrix<double>(src, optional, false, Excel::ToDouble);
    }

    Matrix_<int> Excel::ToIntMatrix(const OPER_* src, bool optional) {
        return ToMatrix<int>(src, optional, false, Excel::ToInt);
    }

    Matrix_<String_> Excel::ToStringMatrix(const OPER_* src, bool optional) {
        return ToMatrix<String_>(src, optional, false,
                                 StringMatrixEntry); // strings within a matrix are thus allowed to be empty
    }

    Matrix_<Cell_> Excel::ToCellMatrix(const OPER_* src, bool optional, bool greedy) {
        return ToMatrix<Cell_>(src, optional, greedy, CellMatrixEntry);
    }

    int Excel::RowReader_::Rows() const {
        if (!src_ || IsBlank(*src_))
            return 0;
        REQUIRE(src_->xltype & xltypeMulti, "Can't read records from a scalar");
        return NonblankSize(src_).first;
    }

    bool Excel::RowReader_::Exists(int i_col) const {
        assert(src_->xltype & xltypeMulti && iRow_ < src_->val.array.rows);
        return i_col < src_->val.array.columns &&
               !IsBlank(src_->val.array.lparray[i_col + src_->val.array.columns * iRow_]);
    }

    template <typename T_>
    auto ReaderExtract(const OPER_* src, int i_row, int i_col, T_ transform) -> decltype(transform(src)) {
        assert(src->xltype & xltypeMulti && i_row < src->val.array.rows);
        REQUIRE(i_col < src->val.array.columns, "Record index out of range");
        return transform(&src->val.array.lparray[i_col + src->val.array.columns * i_row]);
    }
    template <typename T_, typename D_>
    D_ ReaderExtractOrDefault(const OPER_* src, int i_row, int i_col, T_ transform, D_ def_val) {
        assert(src->xltype & xltypeMulti && i_row < src->val.array.rows);
        if (i_col >= src->val.array.columns)
            return def_val;
        auto cell = &src->val.array.lparray[i_col + src->val.array.columns * i_row];
        return IsBlank(*cell) ? def_val : transform(cell);
    }

    double Excel::RowReader_::ExtractDouble(int i_col) const {
        return ReaderExtract(src_, iRow_, i_col, [](const OPER_* src) { return Excel::ToDouble(src); });
    }
    double Excel::RowReader_::ExtractDouble(int i_col, double def_val) const {
        return ReaderExtractOrDefault(
            src_, iRow_, i_col, [](const OPER_* src) { return Excel::ToDouble(src); }, def_val);
    }
    int Excel::RowReader_::ExtractInt(int i_col) const {
        return ReaderExtract(src_, iRow_, i_col, [](const OPER_* src) { return Excel::ToInt(src); });
    }
    int Excel::RowReader_::ExtractInt(int i_col, int def_val) const {
        return ReaderExtractOrDefault(
            src_, iRow_, i_col, [](const OPER_* src) { return Excel::ToInt(src); }, def_val);
    }
    bool Excel::RowReader_::ExtractBool(int i_col) const {
        return ReaderExtract(src_, iRow_, i_col, [](const OPER_* src) { return Excel::ToBool(src); });
    }
    String_ Excel::RowReader_::ExtractString(int i_col) const {
        return ReaderExtract(src_, iRow_, i_col, [](const OPER_* src) { return Excel::ToString(src); });
    }
    Date_ Excel::RowReader_::ExtractDate(int i_col) const {
        return ReaderExtract(src_, iRow_, i_col, [](const OPER_* src) { return Excel::ToDate(src); });
    }
    Handle_<Storable_> Excel::RowReader_::ExtractHandleBase(_ENV, int i_col) const {
        return ReaderExtract(src_, iRow_, i_col, [&](const OPER_* src) { return Excel::ToHandleBase(_env, src); });
    }

    //----------------------------------------------------------------------------
    // end inputs, start outputs

    OPER_* Excel::Error(const String_& msg, const char* arg_name) {
        Retval_ task;
        task.Load(XError(msg, arg_name));
        return task.ToXloper();
    }

    namespace {
        template <class E_> Matrix_<Cell_> ToCells(const Vector_<E_>& src) {
            if (src.empty())
                return Matrix_<Cell_>();

            Matrix_<Cell_> retval(src.size(), 1);
            auto col = retval.Col(0);
            Copy(src, &col);
            return retval;
        }

        template <class E_> Matrix_<Cell_> ToCells(const Matrix_<E_>& src) {
            Matrix_<Cell_> retval(src.Rows(), src.Cols());
            // type this out to avoid the apparently unmaskable compiler warning
            for (int ir = 0; ir < src.Rows(); ++ir) {
                auto dst = retval.Row(ir);
                Copy(src.Row(ir), &dst);
            }
            return retval;
        }

        struct WriteExcelCell_ {
            OPER_* dst_;
            void operator()(const String_& s) const {
                WriteToOper(s, dst_);
            }
            void operator()(int i) const {
                dst_->xltype = xltypeNum;
                dst_->val.num = i;
            }	// allow promotion
            void operator()(double d) const {
                dst_->xltype = xltypeNum;
                dst_->val.num = d;
            }
            void operator()(bool b) const {
                dst_->xltype = xltypeBool;
                dst_->val.xbool = b ? 1 : 0;
            }
            void operator()(const Date_& dt) const {
                dst_->xltype = xltypeNum;
                dst_->val.num = Date::ToExcel(dt);
            }
            void operator()(const DateTime_& dt) const {
                dst_->xltype = xltypeNum;
                dst_->val.num = Date::ToExcel(dt.Date()) + dt.Frac();
            }
            void operator()(std::monostate) const {
                static const String_ EMPTY;
                WriteToOper(EMPTY, dst_);
            }
            template<class T_> void operator()(T_) const {
                THROW("Unrecognizable cell type");
            }
        };

        struct ExcelWriter_ {
            OPER_* dst_;
            ExcelWriter_(OPER_* dst) : dst_(dst) {}

            void operator()(const Cell_& c) {
                WriteExcelCell_ visitor;
                visitor.dst_ = dst_;
                c.Visit(visitor);
            }
        };
    } // namespace

    void Excel::Retval_::Load(double s) {
        scalars_.push_back(values_.size());
        values_.push_back(M1x1(Cell_(s)));
    }

    void Excel::Retval_::Load(const String_& s) {
        scalars_.push_back(values_.size());
        values_.push_back(M1x1(Cell_(s)));
    }

    void Excel::Retval_::Load(const Cell_& s) {
        scalars_.push_back(values_.size());
        values_.push_back(M1x1(s));
    }

    void Excel::Retval_::LoadBase(_ENV, const Handle_<Storable_>& s) {
        static const RepositoryErase_ ERASE; // defaults to NAME_NONEMPTY
        REQUIRE(s, "Output handle is NULL");
        auto repo = Environment::Find<ObjectAccess_>(_env);
        assert(repo);
        Load(repo->Add(s, ERASE)); // Excel can't tell the difference
    }

    void Excel::Retval_::Load(const Vector_<>& v) {
        vectors_.push_back(values_.size());
        values_.push_back(ToCells(v));
    }

    void Excel::Retval_::Load(const Vector_<int>& v) {
        vectors_.push_back(values_.size());
        values_.push_back(ToCells(v));
    }

    void Excel::Retval_::Load(const Vector_<String_>& v) {
        vectors_.push_back(values_.size());
        values_.push_back(ToCells(v));
    }

    void Excel::Retval_::Load(const Vector_<Cell_>& v) {
        vectors_.push_back(values_.size());
        values_.push_back(ToCells(v));
    }

    void Excel::Retval_::LoadBase(_ENV, const Vector_<Handle_<Storable_>>& v) {
        static const RepositoryErase_ ERASE; // defaults to NAME_NONEMPTY
        auto repo = Environment::Find<ObjectAccess_>(_env);
        assert(repo);
        Vector_<Cell_> toStore(v.size());
        Transform(
            v, [&](const Handle_<Storable_>& h) -> String_ { return repo->Add(h, ERASE); }, &toStore);
        Load(toStore);
    }

    void Excel::Retval_::Load(const Matrix_<>& m) {
        matrices_.push_back(values_.size());
        values_.push_back(ToCells(m));
    }
    void Excel::Retval_::Load(const Matrix_<int>& m) {
        matrices_.push_back(values_.size());
        values_.push_back(ToCells(m));
    }
    void Excel::Retval_::Load(const Matrix_<String_>& m) {
        matrices_.push_back(values_.size());
        values_.push_back(ToCells(m));
    }

    void Excel::Retval_::Load(const Matrix_<Cell_>& m) {
        matrices_.push_back(values_.size());
        values_.push_back(m);
    }

    OPER_ Empty() {
        struct Mine_ {
            OPER_ val_;
            Mine_() { val_.xltype = xltypeMissing; }
        };
        static const Mine_ MINE;
        return MINE.val_;
    }

    OPER_* Excel::Retval_::ToXloper() const {
        OPER_* lpx = (OPER_*)GetMemoryForExcel(sizeof(OPER_));
        lpx->xltype = xltypeMulti | xlbitDLLFree;
        int nRows = scalars_.size();
        for (const auto& v : vectors_)
            nRows = Max<int>(nRows, values_[v].Rows());
        int nCols = (scalars_.empty() ? 0 : 1) + vectors_.size();
        for (const auto& m : matrices_) {
            nRows = Max<int>(nRows, values_[m].Rows());
            nCols += values_[m].Cols();
        }
        lpx->val.array.rows = nRows;
        lpx->val.array.columns = nCols;
        lpx->val.array.lparray = (OPER_*)GetMemoryForExcel(nRows * nCols * sizeof(OPER_));
        // initialize to NULL
        for (int ic = 0; ic < nRows * nCols; ++ic)
            lpx->val.array.lparray[ic] = Empty();
        // write scalars to output
        for (int is = 0; is < scalars_.size(); ++is) {
            ExcelWriter_ (&lpx->val.array.lparray[is * nCols])(values_[scalars_[is]](0, 0));
        }
        // write vectors to output
        int offset = scalars_.empty() ? 0 : 1;
        for (const auto& v : vectors_) {
            auto rv = values_[v];
            for (int ie = 0; ie < rv.Rows(); ++ie)
                ExcelWriter_ (&lpx->val.array.lparray[offset + ie * nCols])(rv(ie, 0));
            ++offset;
        }
        // write matrices to output
        for (const auto& m : matrices_) {
            auto rm = values_[m];
            for (int ir = 0; ir < rm.Rows(); ++ir)
                for (int ic = 0; ic < rm.Cols(); ++ic)
                    ExcelWriter_ (&lpx->val.array.lparray[offset + ic + ir * nCols])(rm(ir, ic));
            offset += rm.Cols();
        }

        return lpx;
    }

    template <class T_> const T_* AddressOf(const T_& e) { return &e; }

    OPER_* Excel::Retval_::ToXloper(const String_& format) const {
        if (format.empty())
            return ToXloper();
        Retval_ helper;
        helper.Load(Matrix::Format(Apply(AddressOf<Matrix_<Cell_>>, values_), format));
        return helper.ToXloper();
    }

    // Function registering table
    struct XLFunc_ {
        String_ dllName_;
        String_ cName_; // c-style, no count byte
        String_ xlName_;
        String_ help_;
        String_ argTypes_;
        String_ argNames_;
        Vector_<String_> argHelp_;
        bool volatile_;
        XLFunc_(const String_& dll_name,
                const String_& c_name,
                const String_& xl_name,
                const String_& help,
                const String_& arg_types,
                const String_& arg_names,
                const Vector_<String_>& arg_help,
                bool is_volatile)
            : dllName_(dll_name), cName_(c_name), xlName_(xl_name), help_(help), argTypes_(arg_types),
              argNames_(arg_names), argHelp_(arg_help), volatile_(is_volatile) {}
    };

    static Vector_<XLFunc_>& TheFunctions() { RETURN_STATIC(Vector_<XLFunc_>); }

    void Excel::Register(const String_& dll_name,
                         const String_& c_name,
                         const String_& xl_name,
                         const String_& help,
                         const String_& arg_types,
                         const String_& arg_names,
                         const Vector_<String_>& arg_help,
                         bool is_volatile) {
        TheFunctions().emplace_back(dll_name, c_name, xl_name, help, arg_types, arg_names, arg_help, is_volatile);
    }

    // Menu table
    struct XLMenuItem_ {
        String_ head_; // c-style, no count byte
        String_ subhead_;
        String_ text_;
        XLMenuItem_(const String_& head, const String_& subhead, const String_& text)
            : head_(head), subhead_(subhead), text_(text) {}
    };

    static Vector_<XLMenuItem_> TheMenuItems() {
        static Vector_<XLMenuItem_> RETVAL;
        static bool FIRST = true;
        if (FIRST) {
            // form RETVAL here
            FIRST = false;
        }
        return RETVAL;
    }

    // Initialization routine
    extern "C" __declspec(dllexport) int xlAutoOpen(void) {
        // Get XLL file name
        static OPER_ xDll;
        (void)XL_CALLBACK(xlGetName, &xDll, 0, 0); // assumes just a single DLL; ignores registered property

        // Loop through the function list, and register the functions
        static const String_ MACRO_TYPE("1");
        static const String_ CATEGORY("DAL");
        static const String_ BLANK;
        for (int ii = 0; ii < TheFunctions().size(); ++ii) {
            const XLFunc_& func = TheFunctions()[ii];
            OPER_ result;

            Vector_<OPER_*> xl4args(10); // we do not own this memory
            // populate arguments
            xl4args[0] = &xDll;
            // proc, type_text, function_text, arg, macro_type, category,
            // shortcut_text, help_topic, function_help
            xl4args[1] = TempStr(func.cName_);
            xl4args[2] = TempStr(func.argTypes_);
            xl4args[3] = TempStr(func.xlName_);
            xl4args[4] = TempStr(func.argNames_);
            xl4args[5] = TempNum(1);
            xl4args[6] = TempStr(CATEGORY);
            xl4args[7] = xl4args[8] = TempStr(BLANK);
            xl4args[9] = TempStr(func.help_);
            for (const auto& h : func.argHelp_)
                xl4args.push_back(TempStr(h));

            const int err = XL_CALLBACK(xlfRegister, &result, xl4args.size(), &xl4args[0]);
            if (err != xlretSuccess) {
                // uncomment these lines to get a non-maskable error message on load failure
                // Excel error codes are not informative, so don't have high hopes
                // wchar_t buf[8192];
                // wsprintf(buf, L"xlfRegister for function %d, err = %d", ii, err);
                // AfxMessageBox(buf, MB_SETFOREGROUND);
            }
        }

        // Free XLL file name from the xlGetName call made earlier
        OPER_* freeMe = &xDll;
        (void)XL_CALLBACK(xlFree, 0, 1, &freeMe);

        // Finished
        FreeAllTempMemory();
        return 1;
    }

    // Cleanup routine
    extern "C" __declspec(dllexport) BOOL xlAutoClose(void) {
        //::MessageBox(NULL, L"xlAutoClose()", L"Debug", MB_SETFOREGROUND );

        // Delete menu
        // Excel(xlfDeleteMenu, 0, 2, TempNum(1), TempStr(" MyMenu"));
        ENV_SEED_TYPE(ObjectAccess_);
        auto repo = Environment::Find<ObjectAccess_>(_env);
        assert(repo);
        repo->Erase(""); // everything should match empty pattern

        Vector_<XLFunc_>().Swap(&TheFunctions());
        TheFunctions().clear();
#ifdef _CRTDBG_MAP_ALLOC
        int leaks = _CrtDumpMemoryLeaks();
#endif
        return 1;
    }

    namespace {
/*IF--------------------------------------------------------------------------
public Format
        Combine data according to a user - supplied format string
-java
-python
&inputs
format is string
        The rule for combining arguments : e.g. 5, 1, (3, 4)T means join(from left to right) argument 5, then argument 1,
        then the transpose of the result of joining(from left to right) arguments 3 and 4.  The special argument 0
        represents a single empty cell.
arg1 is cell[][]
        A table
&optional
arg2 is cell[][]
        A table
arg3 is cell[][]
        A table
arg4 is cell[][]
        A table
arg5 is cell[][]
        A table
arg6 is cell[][]
        A table
arg7 is cell[][]
        A table
arg8 is cell[][]
        A table
arg9 is cell[][]
        A table
&outputs
result is cell[][]
        A combined table created according to the format instructions
-IF-------------------------------------------------------------------------*/

        void Format(const String_& format,
                    const Matrix_<Cell_>& arg1,
                    const Matrix_<Cell_>& arg2,
                    const Matrix_<Cell_>& arg3,
                    const Matrix_<Cell_>& arg4,
                    const Matrix_<Cell_>& arg5,
                    const Matrix_<Cell_>& arg6,
                    const Matrix_<Cell_>& arg7,
                    const Matrix_<Cell_>& arg8,
                    const Matrix_<Cell_>& arg9,
                    Matrix_<Cell_>* result) {
            Vector_<const Matrix_<Cell_>*> temp;
            temp.push_back(&arg1);
            temp.push_back(&arg2);
            temp.push_back(&arg3);
            temp.push_back(&arg4);
            temp.push_back(&arg5);
            temp.push_back(&arg6);
            temp.push_back(&arg7);
            temp.push_back(&arg8);
            temp.push_back(&arg9);
            *result = Matrix::Format(temp, format);
        }

/*IF--------------------------------------------------------------------------
public PasteWithArgs
        Shows all the argument names of a function
-java
-python
&inputs
func_name is string
        The function for which to find arguments
&outputs
func_with_args is string
        The text of the function call with argument names inserted
-IF-------------------------------------------------------------------------*/

        void PasteWithArgs(const String_& func_name, String_* text) {
            for (const auto& func : TheFunctions()) {
                if (func_name == func.xlName_) {
                    *text = '=' + func.xlName_ + '(' + func.argNames_ + ')';
                    return;
                }
            }
            THROW("Can't find that function");
        }
    } // namespace

#include <public/auto/MG_Format_public.inc>
#include <public/auto/MG_PasteWithArgs_public.inc>
}

#endif