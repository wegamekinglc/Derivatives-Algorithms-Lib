//
// Created by wegam on 2022/11/19.
//

#pragma once

#include <sstream>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>

namespace Dal {
    using Dal::Script::ScriptProductData_;

    FORCE_INLINE Handle_<ScriptProductData_> NewScriptProduct(const String_& name,
                                                          const Vector_<Cell_>& dates,
                                                          const Vector_<String_>& events) {
        return Handle_<ScriptProductData_>(new ScriptProductData_(name, dates, events));
    }

    FORCE_INLINE String_ DebugScriptProduct(const Handle_<ScriptProductData_>& product) {
        std::ostringstream out;
        product->Product().Debug(out);
        String_ rtn(out.str());
        REQUIRE2(rtn.size() != 0, "empty script product description", ScriptError_);
        return rtn;
    }

    namespace Detail {
        //  IndexVariables fills the variable and constant tables (and the payoff
        //  slot) without restructuring the AST, so the dump mirrors the script
        //  as written but with resolved indices
        FORCE_INLINE Script::ScriptProduct_ ProductForDump(const Handle_<ScriptProductData_>& product) {
            auto parsed = product->Product();
            parsed.IndexVariables();
            return parsed;
        }
    } // namespace Detail

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



