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

    FORCE_INLINE Handle_<ScriptProductData_> NewScriptProduct(const String_& name,
                                                          const Vector_<Cell_>& dates,
                                                          const Vector_<String_>& events) {
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
