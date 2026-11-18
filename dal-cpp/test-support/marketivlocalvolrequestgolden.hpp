#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>

#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal::Test {
    inline String_ MarketIvLocalVolRequestGolden(const String_& modelIdentity, bool noon) {
        const auto path = std::filesystem::path(__FILE__).parent_path() / "marketivlocalvolrequestgolden.json";
        std::ifstream file(path);
        REQUIRE(file.is_open(), "Missing local-vol request golden fixture");
        const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        rapidjson::Document fixtures;
        fixtures.Parse(text.c_str());
        REQUIRE(!fixtures.HasParseError() && fixtures.IsObject(), "Invalid local-vol request golden fixture");
        const auto& fixture = fixtures[noon ? "noon" : "empty"];
        const std::string canonical = fixture["canonical"].GetString();
        REQUIRE(CanonicalJson::FingerprintCanonical(canonical) == String_(fixture["fingerprint"].GetString()), "Request golden changed");
        rapidjson::Document request;
        request.Parse(canonical.c_str());
        REQUIRE(!request.HasParseError() && request.IsObject(), "Invalid canonical request golden");
        auto root = CanonicalJson::FromRapidJson(request);
        // Calibration output bits may differ across libm/compiler implementations.
        root.object_["model_identity"] = CanonicalJson::Json_::String(modelIdentity);
        return CanonicalJson::Fingerprint(root);
    }
} // namespace Dal::Test
