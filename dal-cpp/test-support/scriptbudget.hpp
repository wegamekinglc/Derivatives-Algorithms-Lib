#pragma once

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/cell.hpp>
#include <dal/script/node.hpp>

namespace Dal::Script::TestSupport {
    inline constexpr const char* BUDGET_LOOP = "FOR(i, 0, 10000) x = i END";
    inline constexpr const char* SMALL_BUDGET_LOOP = "FOR(i, 0, 1) x = i END";
    inline constexpr const char* NESTED_BUDGET_LOOP = "FOR(i, 0, 10000) FOR(j, 0, 10) x = j END END";
    inline constexpr const char* BUDGET_ERROR = "InvalidFor: expanded program exceeds 100000 parsed statements";

    inline Vector_<String_> BudgetEvents() {
        Vector_<String_> events(10, BUDGET_LOOP);
        events.push_back(SMALL_BUDGET_LOOP);
        return events;
    }

    inline Vector_<Cell_> BudgetDates(bool sameDate = false) {
        Vector_<Cell_> dates;
        for (int i = 0; i < 11; ++i)
            dates.emplace_back(Date_(2027, 1, sameDate ? 1 : i + 1));
        return dates;
    }

    inline String_ FullEventBudget() {
        String_ text;
        for (int i = 0; i < 10; ++i)
            text += String_(BUDGET_LOOP) + "\n";
        return text;
    }

    inline Vector_<String_> MergedBudgetErrorFields() {
        return {BUDGET_ERROR, "row=11",   "event=2027-01-01",
                "line=11",    "column=1", "offset=" + String_(std::to_string(10 * (String_(BUDGET_LOOP).size() + 1)))};
    }

    template <class F_> void AssertScriptError(F_ action, const Vector_<String_>& fields) {
        try {
            action();
            FAIL() << "expected ScriptError_";
        } catch (const ScriptError_& error) {
            for (const auto& field : fields)
                ASSERT_NE(std::string(error.what()).find(field.c_str()), std::string::npos) << error.what();
        }
    }

    inline void AssertBudgetLoop(const Node_& node, size_t count) {
        const auto* collection = dynamic_cast<const NodeCollect_*>(&node);
        ASSERT_NE(collection, nullptr);
        ASSERT_EQ(collection->arguments_.size(), count);
        if (count == 0)
            return;
        for (size_t i : {size_t(0), count - 1}) {
            const auto* assignment = dynamic_cast<const NodeAssign_*>(collection->arguments_[i].get());
            ASSERT_NE(assignment, nullptr);
            ASSERT_EQ(assignment->arguments_.size(), 2);
            const auto* target = dynamic_cast<const NodeVar_*>(assignment->arguments_[0].get());
            const auto* value = dynamic_cast<const NodeConst_*>(assignment->arguments_[1].get());
            ASSERT_NE(target, nullptr);
            ASSERT_NE(value, nullptr);
            ASSERT_EQ(target->name_, "x");
            ASSERT_DOUBLE_EQ(value->constVal_, static_cast<double>(i));
        }
    }
} // namespace Dal::Script::TestSupport
