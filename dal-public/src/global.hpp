//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <dal/time/date.hpp>

namespace Dal {

    void SetEvaluationDate(const Date_& d);
    Date_ GetEvaluationDate();
}