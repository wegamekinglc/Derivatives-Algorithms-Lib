//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>

namespace Dal {

    FORCE_INLINE void SetEvaluationDate(const Date_& d) {
        Global::Dates_().SetEvaluationDate(d);
    }

    FORCE_INLINE Date_ GetEvaluationDate() {
        return Global::Dates_().EvaluationDate();
    }

}