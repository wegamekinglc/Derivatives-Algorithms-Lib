//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include <dal-public/src/global.hpp>

/*IF--------------------------------------------------------------------------
public Init_GlobalData
    Initialize the DAL global environment (thread pool, repository, etc.)
&inputs
nThreads is integer
    number of threads (0 = auto-detect, default)
&outputs
ok is boolean
    true if initialization succeeded
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        void Init_GlobalData(int nThreads, bool* ok) {
            Dal::InitGlobalData(nThreads);
            *ok = true;
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Init_GlobalData_public.inc>
#endif
}
