//
// Created by wegam on 2020/10/25.
//
#pragma once

namespace Dal {
    class String_;
    class Date_;

    namespace Date {
        bool IsDateString(const String_& src); // predicts whether FromString will work -- examines format only
        Date_ FromString(const String_& src);  // tries our best to recognize the string -- rejects both mm/dd/yyyy and dd/mm/yyyy due to ambiguity
        int MonthFromFutureCode(char code);
    }
}