//
// Created by wegam on 2022/12/9.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/protocol/underlying.hpp>

namespace Dal {
    namespace {
        template<class T_>
        void MergeMax(T_ *base, const T_ &other) {
            for (const auto &kv: other) {
                if (base->count(kv.first))
                    (*base)[kv.first] = std::max((*base)[kv.first], kv.second);
                else
                    (*base)[kv.first] = kv.second;
            }
        }
    }

    Underlying_& Underlying_::operator+=(const Underlying_ &more) {
        MergeMax(&payCcys_, more.payCcys_);
        MergeMax(&indices_, more.indices_);
        MergeMax(&credits_, more.credits_);
        return *this;
    }

    Underlying_& Underlying_::Include(const Ccy_ &ccy, const Date_ &pay_date) {
        if (!payCcys_.count(ccy))
            payCcys_[ccy] = pay_date;
        else
            payCcys_[ccy] = std::max(payCcys_[ccy], pay_date);
        return *this;
    }

    Underlying_& Underlying_::Include(const IndexKey_ &index, const DateTime_ &fix_date) {
        if (!indices_.count(index))
            indices_[index] = fix_date;
        else
            indices_[index] = std::max(indices_[index], fix_date);
        return *this;
    }

    Underlying_& Underlying_::Include(const String_ &ref_name, const Date_ &pay_date) {
        if (!credits_.count(ref_name))
            credits_[ref_name] = pay_date;
        else
            credits_[ref_name] = std::max(credits_[ref_name], pay_date);
        return *this;
    }
}
