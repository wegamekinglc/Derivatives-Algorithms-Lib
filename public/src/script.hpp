//
// Created by wegam on 2022/11/19.
//

#pragma once

#include <sstream>
#include <dal/script/event.hpp>

namespace Dal {
    using Dal::Script::ScriptProductData_;

    FORCE_INLINE Handle_<ScriptProductData_> NewScriptProduct(const String_& name,
                                                          const Matrix_<Cell_>& dates,
                                                          const Vector_<String_>& events) {
        return Handle_<ScriptProductData_>(new ScriptProductData_(name, dates, events));
    }

    FORCE_INLINE String_ DebugScriptProduct(const Handle_<ScriptProductData_>& product) {
        std::ostringstream out;
        product->Product().Debug(out);
        return String_(out.str());
    }
}



