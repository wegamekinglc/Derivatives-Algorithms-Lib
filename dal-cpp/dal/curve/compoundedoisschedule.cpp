#include <dal/platform/platform.hpp>

#include <climits>

#include <dal/curve/compoundedois_internal.hpp>

namespace Dal::CompoundedOisInternal {
    using namespace CalibrationInternal;
    namespace {
        const String_ INVALID = "InvalidCompoundedOisTable", UNSUPPORTED = "UnsupportedCompoundedOisRule";
        CalibrationDiagnostic_ Diagnostic(const CompoundedOisScriptSpec_& s) {
            CalibrationDiagnostic_ d;
            d.instrumentId_ = s.instrumentId_;
            d.stage_ = CalibrationStage_::Value_::Prepare;
            return d;
        }
        Date_ StepBusiness(const HolidayCenterData_& calendar, Date_ day, int direction, const CalibrationDiagnostic_& d) {
            do {
                Check(direction > 0 ? day < Date::Maximum() : day > Date::Minimum(), INVALID, "/observation_convention/lookback_business_days",
                      "business date within the core date range", "", d);
                day = day.AddDays(direction);
            } while (!IsBusiness(calendar, day));
            return day;
        }
        Vector_<Date_> BusinessAxis(const CompoundedOisScriptSpec_& s) {
            const auto d = Diagnostic(s);
            const auto first = s.coupons_.front().start_, last = s.coupons_.back().end_;
            const int lookback = s.observationConvention_->lookbackBusinessDays_;
            Check(lookback <= first - Date::Minimum(), INVALID, "/observation_convention/lookback_business_days",
                  "lookback within the available date range", NumberText(lookback), d);
            auto start = first;
            for (int i = 0; i < lookback; ++i)
                start = StepBusiness(*s.calendar_, start, -1, d);
            const size_t bound = static_cast<size_t>(last - start) + 1;
            Check(bound < INT_MAX && bound <= std::numeric_limits<size_t>::max() / sizeof(CompoundedOisResolvedObservation_),
                  "CompoundedOisResourceLimit", "/observation_schedule", "representable row count and allocation size", "", d);
            Vector_<Date_> result;
            result.reserve(bound);
            result.push_back(start);
            while (result.back() < last)
                result.push_back(StepBusiness(*s.calendar_, result.back(), 1, d));
            return result;
        }
        struct RowDates_ {
            Date_ start_, end_, rateStart_, rateEnd_;
            bool shifted_ = false;
            std::optional<size_t> reused_;
        };
        CompoundedOisResolvedObservation_ ResolvedRow(
            const CompoundedOisScriptSpec_& s, const Script::ScriptValuationSettings_& v, const RowDates_& dates, size_t coupon, size_t ordinal) {
            CompoundedOisResolvedObservation_ r;
            r.accrualStart_ = dates.start_;
            r.accrualEnd_ = dates.end_;
            r.rateStart_ = dates.rateStart_;
            r.rateEnd_ = dates.rateEnd_;
            r.weightStart_ = dates.shifted_ ? dates.rateStart_ : dates.start_;
            r.weightEnd_ = dates.shifted_ ? dates.rateEnd_ : dates.end_;
            r.accrual_ = s.observationBasis_(r.accrualStart_, r.accrualEnd_, nullptr);
            r.rateAccrual_ = s.observationBasis_(r.rateStart_, r.rateEnd_, nullptr);
            r.compoundingAccrual_ = s.observationBasis_(r.weightStart_, r.weightEnd_, nullptr);
            r.fixingTime_ = DateTime_(r.rateStart_);
            r.fixingIndex_ = s.fixingIndex_;
            r.couponId_ = s.coupons_[coupon].couponId_;
            r.couponRow_ = coupon;
            r.row_ = ordinal;
            r.sourceKey_ = (IsVersionTwo(s) ? "observation_schedule/" : "observations/") + OrdinalText(ordinal);
            r.reusedFromRow_ = dates.reused_;
            r.included_ = s.coupons_[coupon].payment_ >= *v.evaluationDate_;
            r.settled_ = !r.included_;
            const bool past = r.rateStart_ < *v.evaluationDate_;
            const bool todayHistorical = r.rateStart_ == *v.evaluationDate_ && v.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
            r.historicalRequired_ = r.included_ && (past || todayHistorical);
            return r;
        }
        Vector_<CompoundedOisResolvedObservation_> LegacySchedule(const CompoundedOisScriptSpec_& s, const Script::ScriptValuationSettings_& v) {
            std::map<std::string, size_t> coupons;
            for (size_t c = 0; c < s.coupons_.size(); ++c)
                coupons.emplace(Bytes(s.coupons_[c].couponId_), c);
            Vector_<CompoundedOisResolvedObservation_> result;
            result.reserve(s.observations_.size());
            for (const auto& row : s.observations_) {
                auto resolved =
                    ResolvedRow(s, v, {row.start_, row.end_, row.rateStart_, row.rateEnd_}, coupons.at(Bytes(row.couponId_)), result.size());
                resolved.accrual_ = resolved.compoundingAccrual_ = row.accrual_;
                result.push_back(std::move(resolved));
            }
            return result;
        }
        void AppendCoupon(const CompoundedOisScriptSpec_& s,
                          const Script::ScriptValuationSettings_& v,
                          const Vector_<Date_>& axis,
                          size_t coupon,
                          Vector_<CompoundedOisResolvedObservation_>* rows) {
            const auto& c = s.coupons_[coupon];
            auto d = Diagnostic(s);
            d.observationId_ = c.couponId_;
            const size_t first = std::lower_bound(axis.begin(), axis.end(), c.start_) - axis.begin();
            const size_t last = std::lower_bound(axis.begin(), axis.end(), c.end_) - axis.begin();
            const auto& convention = *s.observationConvention_;
            const size_t lockout = static_cast<size_t>(convention.lockoutBusinessDays_);
            Check(lockout < last - first, INVALID, "/observation_convention/lockout_business_days", "0 <= k < coupon observation count",
                  OrdinalText(lockout), d);
            const auto anchor = last - lockout - 1;
            const auto firstRow = rows->size();
            for (size_t i = first; i < last; ++i) {
                const bool reused = i > anchor;
                const auto sample = (reused ? anchor : i) - static_cast<size_t>(convention.lookbackBusinessDays_);
                RowDates_ dates{axis[i], axis[i + 1], axis[sample], axis[sample + 1],
                                convention.mode_ == CompoundedOisObservationMode_::Value_::ObservationShift};
                if (reused)
                    dates.reused_ = firstRow + anchor - first;
                rows->push_back(ResolvedRow(s, v, dates, coupon, rows->size()));
            }
        }
    } // namespace
    bool IsVersionTwo(const CompoundedOisScriptSpec_& s) { return Bytes(s.ruleId_) == "dal.compounded-ois-cashflows/2"; }
    bool IsBusiness(const HolidayCenterData_& c, const Date_& day) {
        return std::binary_search(c.workWeekends_.begin(), c.workWeekends_.end(), day) ||
               (!Date::IsWeekEnd(day) && !std::binary_search(c.holidays_.begin(), c.holidays_.end(), day));
    }
    void ValidateObservationConvention(const CompoundedOisScriptSpec_& s) {
        const auto d = Diagnostic(s);
        if (!IsVersionTwo(s)) {
            Check(!s.observationConvention_, UNSUPPORTED, "/observation_convention", "no convention on /1", "", d);
            return;
        }
        Check(s.observationConvention_.has_value(), UNSUPPORTED, "/observation_convention", "explicit /2 convention", "", d);
        const auto& c = *s.observationConvention_;
        Token(c.mode_, INVALID, "/observation_convention/mode", d);
        const bool lookback =
            c.mode_ == CompoundedOisObservationMode_::Value_::Lookback || c.mode_ == CompoundedOisObservationMode_::Value_::ObservationShift;
        Check(c.lookbackBusinessDays_ >= 0 && lookback == (c.lookbackBusinessDays_ > 0), INVALID, "/observation_convention/lookback_business_days",
              "positive for Lookback/ObservationShift, otherwise zero", NumberText(c.lookbackBusinessDays_), d);
        Check(c.lockoutBusinessDays_ >= 0 && (c.mode_ == CompoundedOisObservationMode_::Value_::Lockout) == (c.lockoutBusinessDays_ > 0), INVALID,
              "/observation_convention/lockout_business_days", "positive for Lockout, otherwise zero", NumberText(c.lockoutBusinessDays_), d);
        Check(s.observations_.empty(), INVALID, "/observations", "empty manual observations for /2", "", d);
    }
    Vector_<CompoundedOisResolvedObservation_> ResolveSchedule(const CompoundedOisScriptSpec_& s, const Script::ScriptValuationSettings_& v) {
        if (!IsVersionTwo(s))
            return LegacySchedule(s, v);
        const auto axis = BusinessAxis(s);
        Vector_<CompoundedOisResolvedObservation_> result;
        result.reserve(axis.size() - 1 - s.observationConvention_->lookbackBusinessDays_);
        for (size_t c = 0; c < s.coupons_.size(); ++c)
            AppendCoupon(s, v, axis, c, &result);
        return result;
    }
} // namespace Dal::CompoundedOisInternal
