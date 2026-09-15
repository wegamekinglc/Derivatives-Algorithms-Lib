//
// Created by wegam on 2022/11/19.
//

#pragma once

#include <sstream>

#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/settings.hpp>

namespace Dal {
    using Dal::Script::ScriptProductData_;
    using Script::ScriptProductSettings_;
    String_ DescribeScriptProduct(const Handle_<ScriptProductData_>& product);

    Handle_<ScriptProductData_>
    NewScriptProduct(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events, const ScriptProductSettings_& settings);

    FORCE_INLINE Handle_<ScriptProductData_> NewScriptProduct(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events) {
        return Handle_<ScriptProductData_>(new ScriptProductData_(name, dates, events));
    }

    namespace Detail {
        FORCE_INLINE Script::ScriptProduct_ ProductForDump(const Handle_<ScriptProductData_>& product, bool indexVariables = true) {
            const auto evaluationDate = Script::CaptureScriptEvaluationDate();
            auto parsed = product->Product();
            parsed.PartitionEvents(evaluationDate);
            // Legacy text keeps unresolved variables; JSON/tree enrich without folding syntax.
            if (indexVariables)
                parsed.IndexVariables();
            return parsed;
        }
    } // namespace Detail

    FORCE_INLINE String_ DebugScriptProduct(const Handle_<ScriptProductData_>& product) {
        std::ostringstream out;
        Detail::ProductForDump(product, false).Debug(out);
        String_ rtn(out.str());
        REQUIRE2(rtn.size() != 0, "empty script product description", ScriptError_);
        return rtn;
    }

    FORCE_INLINE String_ DebugScriptProductJson(const Handle_<ScriptProductData_>& product) {
        REQUIRE2(product, "InvalidSetting: product=null; expected a non-null product", ScriptError_);
        REQUIRE2(product->Settings().defaultIndex_.empty(),
                 "DebugSchemaUnsupported: dal.script-product/1 does not support default_index; use DescribeScriptProduct (dal.script-product/2)",
                 ScriptError_);
        std::ostringstream out;
        Detail::ProductForDump(product).DebugJson(out);
        return String_(out.str());
    }

    FORCE_INLINE String_ DebugScriptProductTree(const Handle_<ScriptProductData_>& product, bool ascii = false, int width = 125) {
        std::ostringstream out;
        Detail::ProductForDump(product).DebugTree(out, ascii, width);
        return String_(out.str());
    }
} // namespace Dal
