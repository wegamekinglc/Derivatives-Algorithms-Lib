#pragma once

#include <dal/string/strings.hpp>

namespace Dal::Examples::Autocallables {
    struct Case_ {
        String_ title_ = "Classic snowball";
        double coupon_ = 0.12;
        double knockIn_ = 0.65;
        double knockOut_ = 1.0;
        int firstCall_ = 1;
        double stepDown_ = 0.0;
        bool phoenix_ = false;
        bool memory_ = false;
        bool airbag_ = false;
        bool protected_ = false;
        double participation_ = 0.0;
        bool worstOf_ = false;
    };

    int Run(int argc, char* argv[], const Case_& terms);
} // namespace Dal::Examples::Autocallables
