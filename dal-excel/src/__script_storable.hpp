//
// Created by Codex on 2026/9/15.
//

#pragma once

#include <dal/script/settings.hpp>
#include <dal/storage/storable.hpp>

namespace Dal {
    namespace Excel {
        inline const char* ScriptSettingsType(const Script::ScriptProductSettings_&) { return "ScriptProductSettings"; }
        inline const char* ScriptSettingsType(const Script::ScriptValuationSettings_&) { return "ScriptValuationSettings"; }
        inline const char* ScriptSettingsType(const Script::MonteCarloSettings_&) { return "MonteCarloSettings"; }

        template <class T_> struct StorableScriptSettings_ : Storable_ {
            const T_ val_;
            StorableScriptSettings_(const String_& name, const T_& value) : Storable_(ScriptSettingsType(value), name), val_(value) {}
            void Write(Archive::Store_&) const override {}
        };
    } // namespace Excel

    using StorableScriptProductSettings_ = Excel::StorableScriptSettings_<Script::ScriptProductSettings_>;
    using StorableScriptValuationSettings_ = Excel::StorableScriptSettings_<Script::ScriptValuationSettings_>;
    using StorableMonteCarloSettings_ = Excel::StorableScriptSettings_<Script::MonteCarloSettings_>;
} // namespace Dal
