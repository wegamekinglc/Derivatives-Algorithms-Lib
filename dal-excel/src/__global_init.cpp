//
// Created by wegam on 2026/6/20.
//

#pragma once

#include "__platform.hpp"
#include <dal-public/src/global.hpp>

/*IF--------------------------------------------------------------------------
public InitGlobalData
    Initialize the DAL global environment (thread pool, repository, etc.)
&optional
nThreads is integer
    number of threads (0 = auto-detect, default)
&outputs
ok is boolean
    true if initialization succeeded
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        void InitGlobalDataImpl(int nThreads, bool* ok) {
            InitGlobalData(nThreads);
            *ok = true;
        }
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_InitGlobalData_public.inc>
#endif
}
