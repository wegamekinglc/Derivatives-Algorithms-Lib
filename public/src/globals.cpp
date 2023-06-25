//
// Created by wegam on 2023/6/25.
//

#include <public/src/global.hpp>
#include <dal/storage/globals.hpp>

namespace Dal {
    void SetEvaluationDate(const Date_& d) {
        Global::Dates_::SetEvaluationDate(d);
    }

    Date_ GetEvaluationDate() {
        return Global::Dates_::EvaluationDate();
    }
}