//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration TodayFixingPolicy
    Source for a fixing on the evaluation date
switchable
alternative MODEL
alternative REQUIREHISTORICAL
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_TodayFixingPolicy_enum.hpp>

    namespace Script {
        struct ScriptValuationSettings_ {
            TodayFixingPolicy_ todayFixingPolicy_ = TodayFixingPolicy_::Value_::MODEL;
        };

        Date_ CaptureScriptEvaluationDate();
    } // namespace Script
} // namespace Dal
