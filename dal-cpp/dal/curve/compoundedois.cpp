#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/curve/compoundedois_internal.hpp>

namespace Dal {
#include <dal/auto/MG_CompoundedOisObservationMode_enum.inc>
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
        observationSchedule_ = ResolveSchedule(s, valuation_);
        for (const auto& row : observationSchedule_)
            observations_.push_back({row.couponRow_, row.sourceKey_, row.rateAccrual_, row.historicalRequired_});
        auto contract = ContractJson(s);
        calendarIdentity_ = Fingerprint(CalendarJson(*s.calendar_));
        const auto schedule = ScheduleJson(observationSchedule_);
        auto scheduleContext = Obj({{"calendar", CalendarJson(*s.calendar_)}, {"rule", Str(s.ruleId_)}, {"rows", schedule}});
        if (IsVersionTwo(s)) {
            scheduleContext.object_.emplace("observation_convention", ConventionJson(*s.observationConvention_));
            contract.object_.emplace("observation_schedule", schedule);
        }
        observationScheduleIdentity_ = Fingerprint(scheduleContext);
        contractIdentity_ = Fingerprint(contract);
        preparationIdentity_ = Fingerprint(Obj({{"contract", contract}, {"valuation", ValuationJson(valuation_)}}));
    }
    Handle_<PreparedCompoundedOis_> PrepareCompoundedOis(const CompoundedOisScriptSpec_& spec, const Script::ScriptValuationSettings_& valuation) {
        Validate(spec, valuation);
        return Handle_<PreparedCompoundedOis_>(new PreparedCompoundedOis_(spec, valuation));
    }
} // namespace Dal
