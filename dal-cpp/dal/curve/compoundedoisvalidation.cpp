#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

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
        CalibrationDiagnostic_ Row(const CalibrationDiagnostic_& base, size_t i, const String_& coupon = "") {
            auto d = base;
            d.sourceRow_ = static_cast<int>(i + 1);
            if (!coupon.empty())
                d.observationId_ = coupon;
            return d;
        }
        void Id(const String_& id, const String_& path, const CalibrationDiagnostic_& d) {
            Check(ValidId(id), INVALID, path, "explicit valid ID", id, d);
        }
        void Basis(const DayBasis_& basis, const String_& path, const CalibrationDiagnostic_& d) {
            Check(basis == DayBasis_("ACT_360") || basis == DayBasis_("ACT_365F"), UNSUPPORTED, path, "ACT_360 or ACT_365F", "", d);
        }
        void Date(const Date_& date, const String_& path, const CalibrationDiagnostic_& d) {
            Check(date.IsValid(), INVALID, path, "valid explicit date", "", d);
        }
        void
        Accrual(double actual, const Date_& start, const Date_& end, const DayBasis_& basis, const String_& path, const CalibrationDiagnostic_& d) {
            const double expected = basis(start, end, nullptr);
            Check(std::isfinite(actual) && actual > 0. && std::abs(actual - expected) <= 1e-12, INVALID, path,
                  "positive day-basis accrual " + NumberText(expected), NumberText(actual), d);
        }
        template <class T_> void Nonempty(const Vector_<T_>& rows, const String_& path, const CalibrationDiagnostic_& d) {
            Check(!rows.empty() && rows.size() < INT_MAX, INVALID, path, "nonempty table with fewer than INT_MAX rows", "", d);
        }
        void CalendarDates(const Vector_<Date_>& dates, const String_& path, const CalibrationDiagnostic_& d) {
            Check(dates.size() < INT_MAX, INVALID, path, "fewer than INT_MAX dates", "", d);
            for (size_t i = 0; i < dates.size(); ++i) {
                const auto context = Row(d, i);
                Date(dates[i], path + "/" + OrdinalText(i), context);
                Check(i == 0 || dates[i - 1] < dates[i], INVALID, path + "/" + OrdinalText(i), "strictly increasing unique dates", "", context);
            }
        }
        void Calendar(const CompoundedOisScriptSpec_& s, const CalibrationDiagnostic_& d) {
            Check(s.calendar_.has_value(), INVALID, "/calendar", "explicit resolved calendar", "", d);
            const auto& c = *s.calendar_;
            Id(c.center_, "/calendar/id", d);
            CalendarDates(c.holidays_, "/calendar/holidays", d);
            CalendarDates(c.workWeekends_, "/calendar/work_weekends", d);
            for (size_t i = 0; i < c.workWeekends_.size(); ++i) {
                const auto day = c.workWeekends_[i];
                Check(Date::IsWeekEnd(day) && !std::binary_search(c.holidays_.begin(), c.holidays_.end(), day), INVALID,
                      "/calendar/work_weekends/" + OrdinalText(i), "weekend date without conflicting holiday", "", Row(d, i));
            }
        }
        bool IsBusiness(const HolidayCenterData_& c, const Date_& day) {
            return std::binary_search(c.workWeekends_.begin(), c.workWeekends_.end(), day) ||
                   (!Date::IsWeekEnd(day) && !std::binary_search(c.holidays_.begin(), c.holidays_.end(), day));
        }
        Date_ NextBusiness(const HolidayCenterData_& c, const Date_& start, const String_& path, const CalibrationDiagnostic_& d) {
            auto day = start;
            do {
                Check(day < Date::Maximum(), INVALID, path, "next business day in the core Date range", "", d);
                ++day;
            } while (!IsBusiness(c, day));
            return day;
        }
        void Header(const CompoundedOisScriptSpec_& s, const CalibrationDiagnostic_& d) {
            Id(s.instrumentId_, "/instrument_id", d);
            Check(Bytes(s.ruleId_) == "dal.compounded-ois-cashflows/1", UNSUPPORTED, "/rule", "dal.compounded-ois-cashflows/1", s.ruleId_, d);
            Check(std::isfinite(s.notional_) && s.notional_ > 0., INVALID, "/notional", "finite positive currency amount", NumberText(s.notional_),
                  d);
            Check(std::isfinite(s.coupon_), INVALID, "/coupon", "finite decimal Rate", NumberText(s.coupon_), d);
            Check(std::isfinite(s.minAbsAnnuity_) && s.minAbsAnnuity_ > 0., INVALID, "/min_abs_annuity", "finite positive annuity floor", "", d);
            Check(Bytes(s.direction_) == "PayFixed" || Bytes(s.direction_) == "ReceiveFixed", INVALID, "/direction", "PayFixed or ReceiveFixed",
                  s.direction_, d);
            Token(s.currency_, INVALID, "/currency", d);
            Token(s.collateralCurrency_, INVALID, "/collateral_currency", d);
            Check(s.currency_ == s.collateralCurrency_, UNSUPPORTED, "/collateral_currency", "same-currency collateral", "", d);
            Check(s.collateralType_ == CollateralType_::Value_::OIS, UNSUPPORTED, "/collateral_type", "explicit OIS collateral", "", d);
            Id(s.discountRoute_, "/discount_route", d);
            Id(s.projectionRoute_, "/projection_route", d);
            Id(s.fixingIndex_, "/fixing_index", d);
            Basis(s.observationBasis_, "/observation_basis", d);
            Basis(s.fixedBasis_, "/fixed_basis", d);
        }
        void Valuation(const Script::ScriptValuationSettings_& v, const CalibrationDiagnostic_& d) {
            Check(v.evaluationDate_ && v.evaluationDate_->IsValid(), INVALID, "/valuation/evaluation_date", "explicit valid evaluation date", "", d);
            Check(bool(v.fixings_), INVALID, "/valuation/fixings", "explicit immutable snapshot, including empty", "", d);
            Token(v.todayFixingPolicy_, INVALID, "/valuation/today_fixing_policy", d);
            Check(v.fixings_->Values().size() < INT_MAX, INVALID, "/valuation/fixings", "fewer than INT_MAX fixing indices", "", d);
            size_t row = 0;
            for (const auto& [index, history] : v.fixings_->Values()) {
                const auto context = Row(d, row);
                const String_ path = "/valuation/fixings/" + OrdinalText(row++);
                Text(index, INVALID, path + "/index", context);
                Check(history.size() < INT_MAX, INVALID, path + "/history", "fewer than INT_MAX fixing values", "", context);
            }
        }
        template <class T_> void Periods(const Vector_<T_>& rows, const String_& path, const CalibrationDiagnostic_& d) {
            Nonempty(rows, path, d);
            for (size_t i = 0; i < rows.size(); ++i) {
                const auto& r = rows[i];
                const auto context = Row(d, i);
                const String_ p = path + "/" + OrdinalText(i);
                Date(r.start_, p + "/start", context);
                Date(r.end_, p + "/end", context);
                Date(r.payment_, p + "/payment", context);
                Check(r.start_ < r.end_, INVALID, p + "/end", "end after start", "", context);
                Check(r.payment_ >= r.end_, INVALID, p + "/payment", "payment at or after end", "", context);
                Check(i == 0 || rows[i - 1].end_ == r.start_, INVALID, p + "/start", "contiguous periods in input order", "", context);
            }
        }
        std::map<std::string, size_t> CouponRows(const CompoundedOisScriptSpec_& s, const CalibrationDiagnostic_& d) {
            std::map<std::string, size_t> rows;
            for (size_t i = 0; i < s.coupons_.size(); ++i) {
                const auto& id = s.coupons_[i].couponId_;
                const String_ p = "/coupons/" + OrdinalText(i) + "/coupon_id";
                const auto context = Row(d, i, id);
                Id(id, p, context);
                Check(rows.emplace(Bytes(id), i).second, INVALID, p, "unique case-sensitive coupon ID", id, context);
            }
            return rows;
        }
        void ObservationInterval(const CompoundedOisScriptSpec_& s,
                                 const CompoundedOisObservationRow_& r,
                                 const CompoundedOisCouponRow_& c,
                                 const String_& p,
                                 const CalibrationDiagnostic_& d) {
            Date(r.start_, p + "/start", d);
            Date(r.end_, p + "/end", d);
            Date(r.rateStart_, p + "/rate_start", d);
            Date(r.rateEnd_, p + "/rate_end", d);
            Check(IsBusiness(*s.calendar_, r.rateStart_), INVALID, p + "/rate_start", "business day in frozen calendar", "", d);
            Check(r.rateEnd_ == NextBusiness(*s.calendar_, r.rateStart_, p + "/rate_end", d), INVALID, p + "/rate_end", "next business day", "", d);
            Check(r.fixingTime_.IsValid() && r.fixingTime_.Frac() == 0. && r.fixingTime_.Date() == r.rateStart_, UNSUPPORTED, p + "/fixing_time",
                  "midnight at rate_start without observation shift", "", d);
            Check(r.start_ == std::max(r.rateStart_, c.start_), INVALID, p + "/start", "rate interval/coupon intersection start", "", d);
            Check(r.end_ == std::min(r.rateEnd_, c.end_) && r.start_ < r.end_, INVALID, p + "/end", "nonempty rate interval/coupon intersection end",
                  "", d);
            Accrual(r.accrual_, r.start_, r.end_, s.observationBasis_, p + "/accrual", d);
        }
        void Observations(const CompoundedOisScriptSpec_& s, const CalibrationDiagnostic_& d) {
            Nonempty(s.observations_, "/observations", d);
            const auto couponRows = CouponRows(s, d);
            Vector_<Date_> cursors;
            for (const auto& c : s.coupons_)
                cursors.push_back(c.start_);
            size_t previousCoupon = 0;
            for (size_t i = 0; i < s.observations_.size(); ++i) {
                const auto& r = s.observations_[i];
                const String_ p = "/observations/" + OrdinalText(i);
                const auto context = Row(d, i, r.couponId_);
                const auto found = couponRows.find(Bytes(r.couponId_));
                Check(found != couponRows.end(), INVALID, p + "/coupon_id", "declared coupon ID", r.couponId_, context);
                const auto c = found->second;
                Check(c >= previousCoupon, INVALID, p + "/coupon_id", "observations in coupon table order", "", context);
                ObservationInterval(s, r, s.coupons_[c], p, context);
                Check(r.start_ == cursors[c], INVALID, p + "/start", "continuous coverage without duplicate, gap or overlap", "", context);
                cursors[c] = r.end_;
                previousCoupon = c;
            }
            for (size_t c = 0; c < cursors.size(); ++c)
                Check(cursors[c] == s.coupons_[c].end_, INVALID, "/coupons/" + OrdinalText(c) + "/end", "complete observation coverage", "",
                      Row(d, c, s.coupons_[c].couponId_));
        }
    } // namespace
    void Validate(const CompoundedOisScriptSpec_& s, const Script::ScriptValuationSettings_& v) {
        const auto d = Diagnostic(s);
        Header(s, d);
        Calendar(s, d);
        Valuation(v, d);
        Periods(s.fixedLeg_, "/fixed_leg", d);
        Periods(s.coupons_, "/coupons", d);
        for (size_t i = 0; i < s.fixedLeg_.size(); ++i) {
            const auto& r = s.fixedLeg_[i];
            Accrual(r.accrual_, r.start_, r.end_, s.fixedBasis_, "/fixed_leg/" + OrdinalText(i) + "/accrual", Row(d, i));
        }
        Observations(s, d);
    }
} // namespace Dal::CompoundedOisInternal
