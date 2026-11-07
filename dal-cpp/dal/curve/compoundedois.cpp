#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/curve/compoundedois_internal.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    using namespace CompoundedOisInternal;
    namespace {
        Script::ScriptValuationSettings_ CopyValuation(const Script::ScriptValuationSettings_& v) {
            auto copy = v;
            copy.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(v.fixings_->Values()));
            return copy;
        }
        template <class T_> Vector_<CompoundedOisPaymentStatus_> Payments(const Vector_<T_>& rows, const Date_& today) {
            Vector_<CompoundedOisPaymentStatus_> result;
            for (const auto& row : rows)
                result.push_back({row.payment_ >= today, row.payment_ < today});
            return result;
        }
    } // namespace
    PreparedCompoundedOis_::PreparedCompoundedOis_(const CompoundedOisScriptSpec_& s, const Script::ScriptValuationSettings_& v)
        : spec_(s), valuation_(CopyValuation(v)), fixedPayments_(Payments(s.fixedLeg_, *v.evaluationDate_)),
          couponPayments_(Payments(s.coupons_, *v.evaluationDate_)) {
        std::map<std::string, size_t> couponRows;
        for (size_t i = 0; i < s.coupons_.size(); ++i)
            couponRows.emplace(Bytes(s.coupons_[i].couponId_), i);
        for (size_t i = 0; i < s.observations_.size(); ++i) {
            const auto& row = s.observations_[i];
            const auto couponRow = couponRows.at(Bytes(row.couponId_));
            const bool historical =
                row.fixingTime_.Date() < *v.evaluationDate_ ||
                (row.fixingTime_.Date() == *v.evaluationDate_ && v.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL);
            observations_.push_back({couponRow, "observations/" + OrdinalText(i), s.observationBasis_(row.rateStart_, row.rateEnd_, nullptr),
                                     couponPayments_[couponRow].included_ && historical});
        }
        contractIdentity_ = Fingerprint(ContractJson(s));
        calendarIdentity_ = Fingerprint(CalendarJson(*s.calendar_));
        preparationIdentity_ = Fingerprint(Obj({{"contract", ContractJson(s)}, {"valuation", ValuationJson(valuation_)}}));
    }
    Handle_<PreparedCompoundedOis_> PrepareCompoundedOis(const CompoundedOisScriptSpec_& spec, const Script::ScriptValuationSettings_& valuation) {
        Validate(spec, valuation);
        return Handle_<PreparedCompoundedOis_>(new PreparedCompoundedOis_(spec, valuation));
    }
} // namespace Dal
