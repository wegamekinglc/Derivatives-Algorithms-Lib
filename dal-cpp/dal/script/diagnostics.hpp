//
// Created by Codex on 2026/9/15.
//

#pragma once

#include <dal/string/strings.hpp>

namespace Dal::Script {
    class ScriptProductData_;
    class PreparedScript_;
    String_ DescribeScriptProductData(const ScriptProductData_& data);
    String_ ExplainPreparedScript(const PreparedScript_& prepared);
} // namespace Dal::Script
