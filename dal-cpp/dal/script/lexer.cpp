//
// Created by wegam on 2026/5/30.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/lexer.hpp>
#include <regex>

namespace Dal::Script {
    Vector_<String_> Tokenize(const String_& str) {
        static const std::regex r(R"([\w.]+|[/-]|,|;|:|[\(\)\+\*\^]|!=|>=|<=|[<>=])");
        Vector_<String_> v;
        for (std::regex_iterator<String_::const_iterator> it(str.begin(), str.end(), r), end; it != end; ++it)
            v.push_back(String_((*it)[0]));
        return v;
    }
}
