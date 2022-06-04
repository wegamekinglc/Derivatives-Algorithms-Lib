//
// Created by wegam on 2022/4/4.
//

#pragma once

#include <memory>
#include <dal/math/vectors.hpp>
#include <dal/time/date.hpp>
#include <dal/script/node.hpp>

namespace Dal::Script {
    using Event_ = Vector_<std::unique_ptr<ScriptNode_>>;

    class Product_ {
        Vector_<Date_> eventDates_;
        Vector_<Event_> events_;
        Vector_<String_> variables_;

    public:
        const Vector_<Date_>& EventDates() const { return eventDates_; }
        const Vector_<String_>& VarNames() const { return variables_; }

    };

}
