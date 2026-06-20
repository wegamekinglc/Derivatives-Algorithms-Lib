//
// Created by wegam on 2023/6/25.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/globals.hpp>
#include <dal-public/src/global.hpp>

namespace Dal {
    void InitGlobalData(int nThreads) {
        RegisterAll_::Init(nThreads);
    }

    void SetEvaluationDate(const Date_& d) {
        Global::Dates_::SetEvaluationDate(d);
    }

    Date_ GetEvaluationDate() {
        return Global::Dates_::EvaluationDate();
    }
} // namespace Dal