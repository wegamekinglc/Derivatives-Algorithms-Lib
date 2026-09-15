//
// Created by Codex on 2026/9/15.
//

#include <gtest/gtest.h>
#include <rapidjson/document.h>

#include <dal-public/src/global.hpp>
#include <dal-public/src/script.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

TEST(ScriptArchiveTest, TestIndexVersioning) {
    InitGlobalData(1);
    const String_ golden = R"json({"~type":"ScriptProductData_v1","name":"legacy","dates":["2026-09-22"],"events":["pay PAYS SPOT()"]})json";
    const auto legacy = handle_cast<ScriptProductData_>(JSON::ReadString(golden, false));
    ASSERT_TRUE(legacy);
    ASSERT_TRUE(legacy->Settings().defaultIndex_.empty());
    ASSERT_EQ(legacy->EventTexts(), Vector_<String_>{"pay PAYS SPOT()"});
    ASSERT_NE(JSON::WriteString(*legacy).find("ScriptProductData_v2"), String_::npos);

    const auto original = NewScriptProduct("raw\"contract", {Cell_("OBS"), Cell_(Date_(2026, 9, 22))},
                                           {"FIX(eq[MiXeD]@2026-12-31, 2026-09-11)", "pay PAYS OBS + SPOT()"}, {"eq[MiXeD]"});
    const auto blob = JSON::WriteString(*original);
    rapidjson::Document json;
    json.Parse(blob.c_str());
    ASSERT_FALSE(json.HasParseError());
    ASSERT_EQ(json.MemberCount(), 5u);
    ASSERT_STREQ(json["~type"].GetString(), "ScriptProductData_v2");
    ASSERT_STREQ(json["default_index"].GetString(), "eq[MiXeD]");
    const auto restored = handle_cast<ScriptProductData_>(JSON::ReadString(blob, false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(std::string(restored->Name().c_str()), "raw\"contract");
    ASSERT_EQ(restored->Dates().size(), original->Dates().size());
    for (size_t i = 0; i < original->Dates().size(); ++i)
        ASSERT_TRUE(restored->Dates()[i] == original->Dates()[i]);
    for (size_t i = 0; i < original->EventTexts().size(); ++i)
        ASSERT_EQ(std::string(restored->EventTexts()[i].c_str()), std::string(original->EventTexts()[i].c_str()));
    ASSERT_EQ(std::string(restored->Settings().defaultIndex_.c_str()), "eq[MiXeD]");

    for (const String_ field : {String_(), String_(R"(,"default_index":"")")}) {
        const String_ emptyDefault = R"({"~type":"ScriptProductData_v2","dates":[],"events":[])" + field + "}";
        const auto empty = handle_cast<ScriptProductData_>(JSON::ReadString(emptyDefault, false));
        ASSERT_TRUE(empty);
        ASSERT_TRUE(empty->Settings().defaultIndex_.empty());
    }
}
