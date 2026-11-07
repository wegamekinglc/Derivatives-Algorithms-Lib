#pragma once

#include <limits>

#include <dal/calibration/protocol.hpp>
#include <dal/script/settings.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidaydata.hpp>

namespace Dal {
    struct CompoundedOisFixedRow_ {
        Date_ start_, end_, payment_;
        double accrual_ = 0.;
    };
    struct CompoundedOisCouponRow_ {
        String_ couponId_;
        Date_ start_, end_, payment_;
    };
    struct CompoundedOisObservationRow_ {
        Date_ start_, end_;
        DateTime_ fixingTime_;
        double accrual_ = 0.;
        String_ couponId_;
        Date_ rateStart_, rateEnd_;
    };
    struct CompoundedOisScriptSpec_ {
        String_ instrumentId_, direction_, discountRoute_, projectionRoute_, fixingIndex_;
        double notional_ = std::numeric_limits<double>::quiet_NaN();
        double coupon_ = std::numeric_limits<double>::quiet_NaN();
        double minAbsAnnuity_ = 1e-12;
        Ccy_ currency_, collateralCurrency_;
        CollateralType_ collateralType_;
        DayBasis_ observationBasis_, fixedBasis_;
        std::optional<HolidayCenterData_> calendar_;
        String_ ruleId_ = "dal.compounded-ois-cashflows/1";
        Vector_<CompoundedOisFixedRow_> fixedLeg_;
        Vector_<CompoundedOisCouponRow_> coupons_;
        Vector_<CompoundedOisObservationRow_> observations_;
    };
    struct CompoundedOisPaymentStatus_ {
        bool included_ = false, settled_ = false;
    };
    struct CompoundedOisObservationInfo_ {
        size_t couponRow_ = 0;
        String_ sourceKey_;
        double rateAccrual_ = 0.;
        bool historicalRequired_ = false;
    };
    class PreparedCompoundedOis_ final {
        const CompoundedOisScriptSpec_ spec_;
        const Script::ScriptValuationSettings_ valuation_;
        Vector_<CompoundedOisPaymentStatus_> fixedPayments_, couponPayments_;
        Vector_<CompoundedOisObservationInfo_> observations_;
        String_ contractIdentity_, preparationIdentity_, calendarIdentity_;
        PreparedCompoundedOis_(const CompoundedOisScriptSpec_&, const Script::ScriptValuationSettings_&);
        friend Handle_<PreparedCompoundedOis_> PrepareCompoundedOis(const CompoundedOisScriptSpec_&, const Script::ScriptValuationSettings_&);

    public:
        [[nodiscard]] const CompoundedOisScriptSpec_& Spec() const { return spec_; }
        [[nodiscard]] const Script::ScriptValuationSettings_& Valuation() const { return valuation_; }
        [[nodiscard]] const String_& RuleId() const { return spec_.ruleId_; }
        [[nodiscard]] const String_& ContractIdentity() const { return contractIdentity_; }
        [[nodiscard]] const String_& PreparationIdentity() const { return preparationIdentity_; }
        [[nodiscard]] const String_& CalendarIdentity() const { return calendarIdentity_; }
        [[nodiscard]] const Vector_<CompoundedOisPaymentStatus_>& FixedPayments() const { return fixedPayments_; }
        [[nodiscard]] const Vector_<CompoundedOisPaymentStatus_>& CouponPayments() const { return couponPayments_; }
        [[nodiscard]] const Vector_<CompoundedOisObservationInfo_>& Observations() const { return observations_; }
    };
    Handle_<PreparedCompoundedOis_> PrepareCompoundedOis(const CompoundedOisScriptSpec_& spec, const Script::ScriptValuationSettings_& valuation);
    String_ DumpPreparedCompoundedOisJson(const Handle_<PreparedCompoundedOis_>& prepared);
    Handle_<PreparedCompoundedOis_> LoadPreparedCompoundedOisJson(const String_& json);
} // namespace Dal
