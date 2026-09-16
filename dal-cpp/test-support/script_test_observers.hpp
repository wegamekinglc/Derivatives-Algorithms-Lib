//
// Created by Codex on 2026/9/16.
//

#pragma once

#include <cstddef>
#include <map>

#include <dal/indice/detail/fixingobserver.hpp>
#include <dal/math/cell.hpp>
#include <dal/script/detail/simulationobserver.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>

//  Observer scaffolding shared by the script fixing/observation tests across
//  dal-cpp, dal-public and dal-excel; not part of any shipped target.
namespace Dal::Script::TestSupport {
    //  Counting FixingReadObserver_: total history lookups and fixing reads
    struct FixingReadCounter_ : Dal::Detail::FixingReadObserver_ {
        size_t histories_ = 0;
        size_t fixings_ = 0;
        void BeforeHistory(const String_&) override { ++histories_; }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override { ++fixings_; }
    };

    //  Same, with history lookups counted per canonical index name
    struct NamedFixingReadCounter_ : Dal::Detail::FixingReadObserver_ {
        std::map<String_, size_t> histories_;
        size_t fixings_ = 0;
        void BeforeHistory(const String_& name) override { ++histories_[name]; }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override { ++fixings_; }
    };

    //  Throws on any read; install for phases that must not touch fixings
    struct RejectFixingReads_ : Dal::Detail::FixingReadObserver_ {
        size_t historyCalls_ = 0;
        size_t fixingCalls_ = 0;
        const char* historyMessage_;
        const char* fixingMessage_;
        explicit RejectFixingReads_(const char* historyMessage = "unexpected history read", const char* fixingMessage = "unexpected fixing read")
            : historyMessage_(historyMessage), fixingMessage_(fixingMessage) {}
        void BeforeHistory(const String_&) override {
            ++historyCalls_;
            THROW(historyMessage_);
        }
        void BeforeFixing(const Index_&, const Environment_*, const DateTime_&) override {
            ++fixingCalls_;
            THROW(fixingMessage_);
        }
    };

    //  Counting SimulationObserver_
    struct SubmissionCounter_ : Dal::Script::Detail::SimulationObserver_ {
        size_t submissions_ = 0;
        void AfterSubmission() override { ++submissions_; }
    };

    //  Throws on any worker submission
    struct RejectSubmissions_ : Dal::Script::Detail::SimulationObserver_ {
        size_t calls_ = 0;
        const char* message_;
        explicit RejectSubmissions_(const char* message = "unexpected worker submission") : message_(message) {}
        void AfterSubmission() override {
            ++calls_;
            THROW(message_);
        }
    };

    //  One-event script product from a single statement text
    inline ScriptProductData_ ScriptTestProduct(const String_& text, const Date_& date = Date_(2026, 9, 22)) { return {"", {Cell_(date)}, {text}}; }

    //  Overwrite one fixing in the global store
    inline void StoreScriptTestFixing(const String_& name, double value, const DateTime_& time = DateTime_(Date_(2026, 9, 11), 0.0)) {
        FixHistory_ history;
        history.vals_ = {{time, value}};
        XGLOBAL::StoreFixings(name, history, false);
    }
} // namespace Dal::Script::TestSupport
