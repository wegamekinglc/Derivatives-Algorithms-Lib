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
    // Outcome of preprocessing a raw events table.
    //
    // It carries the two "definition" products that the payoff parser consumes:
    //  - constVariables_: named constant values declared in the table,
    //  - events_:         macro/schedule-resolved descriptions keyed by date.
    struct PreprocessedEvents_ {
        std::map<String_, double> constVariables_;
        std::map<Date_, String_> events_;
    };

    // Front-end of the script engine.
    //
    // Preprocessor_ resolves the "definition" half of an events table -- constant
    // variables, textual macros and schedules -- into concrete dated event
    // descriptions. It is deliberately independent of Parser_ (the payoff
    // back-end): it never builds an AST and knows nothing about nodes, so the two
    // halves can be developed and unit-tested in isolation.
    //
    // The class is built for extension. Process orchestrates the fixed pipeline
    // while every meaningful decision is delegated to a protected virtual, so a
    // derived preprocessor can recognise new directive kinds or new placeholders
    // without re-implementing the orchestration.
    class Preprocessor_ {
    public:
        virtual ~Preprocessor_() = default;

        // Resolve a raw (date-or-name, description) events table into constant
        // variables and dated event descriptions.
        [[nodiscard]] PreprocessedEvents_ Process(const Vector_<std::pair<Cell_, String_>>& events) const;

    protected:
        // True when a non-date directive description denotes a schedule.
        [[nodiscard]] virtual bool IsSchedule(const String_& desc) const;

        // True when a non-date directive value defines a constant variable rather
        // than a textual macro.
        [[nodiscard]] virtual bool IsConstVariable(const String_& value) const;

        // Replace every registered macro name found in statement with its body.
        [[nodiscard]] virtual String_ ExpandMacros(const String_& statement,
                                                   const std::map<String_, String_>& macros) const;

        // Replace schedule placeholders (PeriodBegin / PeriodEnd) for one period.
        [[nodiscard]] virtual String_ ExpandSchedulePlaceholders(const String_& statement,
                                                                 const Date_& begin,
                                                                 const Date_& end) const;
    };
}
