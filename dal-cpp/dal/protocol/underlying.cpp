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
    } // namespace

    Underlying_& Underlying_::operator+=(const Underlying_ &more) {
        MergeMax(&payCcys_, more.payCcys_);
        MergeMax(&indices_, more.indices_);
        MergeMax(&credits_, more.credits_);
        return *this;
    }

    Underlying_& Underlying_::Include(const Ccy_ &ccy, const Date_ &payDate) {
        if (!payCcys_.count(ccy))
            payCcys_[ccy] = payDate;
        else
            payCcys_[ccy] = std::max(payCcys_[ccy], payDate);
        return *this;
    }

    Underlying_& Underlying_::Include(const IndexKey_ &index, const DateTime_ &fixDate) {
        if (!indices_.count(index))
            indices_[index] = fixDate;
        else
            indices_[index] = std::max(indices_[index], fixDate);
        return *this;
    }

    Underlying_& Underlying_::Include(const String_ &refName, const Date_ &payDate) {
        if (!credits_.count(refName))
            credits_[refName] = payDate;
        else
            credits_[refName] = std::max(credits_[refName], payDate);
        return *this;
    }
} // namespace Dal
