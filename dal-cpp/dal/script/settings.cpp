//
// Created by Codex on 2026/9/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/settings.hpp>
#include <dal/storage/globals.hpp>

namespace Dal {
#include <dal/auto/MG_TodayFixingPolicy_enum.inc>

    namespace Script {
        Date_ CaptureScriptEvaluationDate() { return Global::Dates_::EvaluationDate(); }
    } // namespace Script
} // namespace Dal
