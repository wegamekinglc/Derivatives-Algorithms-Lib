//
// Created by wegam on 2022/11/19.
//

#include <dal-public/src/script.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/diagnostics.hpp>

namespace Dal {
    String_ DescribeScriptProduct(const Handle_<ScriptProductData_>& product) {
        REQUIRE2(product, "InvalidSetting: product=null; expected a non-null product", ScriptError_);
        return Script::DescribeScriptProductData(*product);
    }
    Handle_<ScriptProductData_>
    NewScriptProduct(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events, const ScriptProductSettings_& settings) {
        return Handle_<ScriptProductData_>(new ScriptProductData_(name, dates, events, settings));
    }
} // namespace Dal
