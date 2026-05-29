//
// Created by Cheng Li on 2017/12/20.
//

#pragma once

#ifdef _MSC_VER

#pragma warning(disable : 4996)

#pragma warning(error : 4129)
#pragma warning(error : 4150)
#pragma warning(error : 4172)
#pragma warning(error : 4244)
#pragma warning(error : 4715)
#pragma warning(error : 4717)
#pragma warning(error : 4800)

#endif

namespace Dal {
    template <class T_> inline int AsInt(const T_& t) { return static_cast<int>(t); }
} // namespace Dal
