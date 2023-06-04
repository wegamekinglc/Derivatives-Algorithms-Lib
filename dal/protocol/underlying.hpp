//
// Created by wegam on 2022/12/9.
//

#pragma once

#include <map>
#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/currency/currency.hpp>
#include <dal/indice/index.hpp>

namespace Dal {

    struct Underlying_ {
        class Parent_ {
        public:
            virtual ~Parent_() = default;
        };
        Handle_<Parent_> parent_;
        std::map<Ccy_, Date_> payCcys_;
        std::map<IndexKey_, DateTime_> indices_;
        std::map<String_, Date_> credits_;

        Underlying_& operator+=(const Underlying_& more);
        Underlying_& Include(const Ccy_& ccy, const Date_& pay_date);
        Underlying_& Include(const IndexKey_& index, const DateTime_& fix_date);
        Underlying_& Include(const String_& ref_name, const Date_& pay_date);
    };
}
