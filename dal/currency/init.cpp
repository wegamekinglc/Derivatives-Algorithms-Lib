//
// Created by wegam on 2020/11/28.
//

#include <dal/platform/platform.hpp>
#include <dal/currency/init.hpp>
#include <dal/platform/strict.hpp>
#include <dal/currency/currencydata.hpp>

namespace Dal {

    bool CcyFacts_::init_ = false;
    std::mutex CcyFacts_::mutex_;

    void CcyFacts_::Init() {
        std::lock_guard<std::mutex> l(mutex_);
        if (!init_) {
            Ccy::Conventions::LiborFixDays().XWrite().SetDefault(2);
            Ccy::Conventions::LiborFixDays().XWrite()(Ccy_("CNY"), 1);
            init_ = true;
        }
    }
} // namespace Dal