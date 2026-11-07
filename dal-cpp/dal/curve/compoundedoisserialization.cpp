#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/curve/compoundedois_internal.hpp>

namespace Dal::CompoundedOisInternal {
    using namespace CalibrationInternal;
    namespace {
        Json_ DateJson(const Date_& date) { return Str(Date::ToString(date)); }
        Json_ TimeJson(const DateTime_& time) { return Obj({{"date", DateJson(time.Date())}, {"fraction", Num(time.Frac())}}); }
    } // namespace
    Json_ CalendarJson(const HolidayCenterData_& c) {
        return Obj({{"id", Str(c.center_)}, {"holidays", Array(c.holidays_, DateJson)}, {"work_weekends", Array(c.workWeekends_, DateJson)}});
    }
    Json_ ContractJson(const CompoundedOisScriptSpec_& s) {
        return Obj({{"instrument_id", Str(s.instrumentId_)},
                    {"rule", Str(s.ruleId_)},
                    {"notional", Num(s.notional_)},
                    {"coupon", Num(s.coupon_)},
                    {"direction", Str(s.direction_)},
                    {"currency", Str(s.currency_.String())},
                    {"collateral_currency", Str(s.collateralCurrency_.String())},
                    {"collateral_type", Str(s.collateralType_.String())},
                    {"discount_route", Str(s.discountRoute_)},
                    {"projection_route", Str(s.projectionRoute_)},
                    {"fixing_index", Str(s.fixingIndex_)},
                    {"observation_basis", Str(s.observationBasis_.String())},
                    {"fixed_basis", Str(s.fixedBasis_.String())},
                    {"min_abs_annuity", Num(s.minAbsAnnuity_)},
                    {"calendar", CalendarJson(*s.calendar_)},
                    {"settlement", Str("IncludeUnsettledAtValuation")},
                    {"fixed_leg", Array(s.fixedLeg_,
                                        [](const auto& r) {
                                            return Obj({{"start", DateJson(r.start_)},
                                                        {"end", DateJson(r.end_)},
                                                        {"payment", DateJson(r.payment_)},
                                                        {"accrual", Num(r.accrual_)}});
                                        })},
                    {"coupons", Array(s.coupons_,
                                      [](const auto& r) {
                                          return Obj({{"coupon_id", Str(r.couponId_)},
                                                      {"start", DateJson(r.start_)},
                                                      {"end", DateJson(r.end_)},
                                                      {"payment", DateJson(r.payment_)}});
                                      })},
                    {"observations", Array(s.observations_, [](const auto& r) {
                         return Obj({{"start", DateJson(r.start_)},
                                     {"end", DateJson(r.end_)},
                                     {"fixing_time", TimeJson(r.fixingTime_)},
                                     {"accrual", Num(r.accrual_)},
                                     {"coupon_id", Str(r.couponId_)},
                                     {"rate_start", DateJson(r.rateStart_)},
                                     {"rate_end", DateJson(r.rateEnd_)}});
                     })}});
    }
    Json_ ValuationJson(const Script::ScriptValuationSettings_& v) {
        Json_ fixings = Json_::Array();
        for (const auto& [index, history] : v.fixings_->Values()) {
            Json_ values = Json_::Array();
            for (const auto& [time, value] : history)
                values.array_.push_back(Obj({{"time", TimeJson(time)}, {"value", Num(value)}}));
            fixings.array_.push_back(Obj({{"index", Str(index)}, {"history", values}}));
        }
        return Obj(
            {{"evaluation_date", DateJson(*v.evaluationDate_)}, {"today_fixing_policy", Str(v.todayFixingPolicy_.String())}, {"fixings", fixings}});
    }
} // namespace Dal::CompoundedOisInternal

namespace Dal {
    String_ DumpPreparedCompoundedOisJson(const Handle_<PreparedCompoundedOis_>& prepared) {
        using namespace CalibrationInternal;
        Check(bool(prepared), "InvalidCompoundedOisTable", "/prepared", "nonnull compounded OIS preparation");
        return String_(Jcs(Obj({{"schema", Str("dal.compounded-ois-preparation/1")},
                                {"spec", CompoundedOisInternal::ContractJson(prepared->Spec())},
                                {"valuation", CompoundedOisInternal::ValuationJson(prepared->Valuation())}})));
    }
} // namespace Dal
