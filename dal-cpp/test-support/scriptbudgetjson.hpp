#pragma once

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include "scriptbudget.hpp"

namespace Dal::Script::TestSupport {
    inline void AssertBudgetDescription(const rapidjson::Value& json) {
        ASSERT_STREQ(json["schema"].GetString(), "dal.script-product/2");
        ASSERT_TRUE(json["payoff_index"].IsNull());
        const auto& events = json["events"];
        ASSERT_EQ(events.Size(), 11);
        ASSERT_EQ(json["input_rows"].Size(), 11);
        const auto texts = BudgetEvents();
        for (rapidjson::SizeType i = 0; i < events.Size(); ++i) {
            const auto date = Date::ToString(Date_(2027, 1, static_cast<int>(i) + 1));
            ASSERT_STREQ(events[i]["date"].GetString(), date.c_str());
            const auto& input = json["input_rows"][i];
            ASSERT_EQ(input["row"].GetUint(), i + 1);
            ASSERT_STREQ(input["date_or_definition"].GetString(), date.c_str());
            ASSERT_STREQ(input["text"].GetString(), texts[i].c_str());
            const auto& origins = events[i]["origins"];
            ASSERT_EQ(origins.Size(), 1);
            ASSERT_EQ(origins[0]["row"].GetUint(), i + 1);
            ASSERT_EQ(origins[0]["offset"].GetUint(), 0);
            ASSERT_STREQ(origins[0]["event_date"].GetString(), date.c_str());
            const auto& statements = events[i]["statements"];
            ASSERT_EQ(statements.Size(), 1);
            ASSERT_STREQ(statements[0]["kind"].GetString(), "collect");
            const auto& children = statements[0]["children"];
            const rapidjson::SizeType count = i == 10 ? 1 : 10000;
            ASSERT_EQ(children.Size(), count);
            for (rapidjson::SizeType j : {rapidjson::SizeType(0), count - 1}) {
                ASSERT_STREQ(children[j]["kind"].GetString(), "assign");
                ASSERT_STREQ(children[j]["target"]["name"].GetString(), "x");
                ASSERT_STREQ(children[j]["value"]["kind"].GetString(), "const");
                ASSERT_DOUBLE_EQ(children[j]["value"]["value"].GetDouble(), static_cast<double>(j));
            }
        }
    }
} // namespace Dal::Script::TestSupport
