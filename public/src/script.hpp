//
// Created by wegam on 2022/11/19.
//

#pragma once

#include <sstream>
#include <dal/script/event.hpp>

namespace Dal {
    using Dal::Script::ScriptProduct_;

    FORCE_INLINE Handle_<ScriptProduct_> NewScriptProduct(const String_& name,
                                                          const Vector_<Date_>& dates,
                                                          const Vector_<String_>& events) {
        return Handle_<ScriptProduct_>(new ScriptProduct_(name, dates, events));
    }

    FORCE_INLINE String_ DebugScriptProduct(const Handle_<ScriptProduct_>& product) {
        std::ostringstream out;
        product->Debug(out);
        return String_(out.str());
    }
}



