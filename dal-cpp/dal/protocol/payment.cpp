//
// Created by wegam on 2022/12/10.
//

#include <utility>
#include <dal/platform//platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/protocol/payment.hpp>

namespace Dal {
    Payment::Conditions_::Conditions_()
    : exerciseCondition_(Exercise_::UNCONDITIONAL) {}

    Payment::Info_::Info_(String_ des,
                          const DateTime_ &known,
                          const Conditions_ &conditions,
                          const AccrualPeriod_ *accrual)
    : description_(std::move(des)), knownTime_(known), conditions_(conditions) {
        if (accrual)
            period_ = *accrual;
    }

    Payment_::Payment_(const DateTime_ &et,
                       const Ccy_ &ccy,
                       const Date_ &dt,
                       String_ s,
                       Payment::Info_ tag,
                       const Date_ &cd)
    : eventTime_(et), ccy_(ccy), date_(dt), stream_(std::move(s)), tag_(std::move(tag)), commitDate_(cd) {}

    const Handle_<Payment::Tag_>& Payment::Null() {
        RETURN_STATIC(const Handle_<Tag_>);
    }
}
