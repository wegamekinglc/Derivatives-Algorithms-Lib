//
// Created by wegam on 2026/5/30.
//

#pragma once

#include <map>
#include <utility>
#include <dal/math/cell.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>

namespace Dal::Script {
    // Resolves constant variables, macros, and schedules into dated event descriptions.
    // Independent of Parser_ — never builds an AST — so the two halves can be tested in isolation.
    struct PreprocessedEvents_ {
        std::map<String_, double> constVariables_;
        std::map<Date_, String_> events_;
    };

    // Built for extension: Process orchestrates a fixed pipeline; every meaningful decision
    // is a protected virtual, so a derived class can recognise new directive kinds or
    // placeholders without re-implementing the orchestration.
    class Preprocessor_ {
    public:
        virtual ~Preprocessor_() = default;

        [[nodiscard]] PreprocessedEvents_ Process(const Vector_<std::pair<Cell_, String_>>& events) const;

    protected:
        [[nodiscard]] virtual bool IsSchedule(const String_& desc) const;
        [[nodiscard]] virtual bool IsConstVariable(const String_& value) const;
        [[nodiscard]] virtual String_ ExpandMacros(const String_& statement,
                                                   const std::map<String_, String_>& macros) const;
        [[nodiscard]] virtual String_ ExpandSchedulePlaceholders(const String_& statement,
                                                                 const Date_& begin,
                                                                 const Date_& end) const;
    };
}
