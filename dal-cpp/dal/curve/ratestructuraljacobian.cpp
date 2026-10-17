//
// Created by Codex on 2026/10/09.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cstdint>
#include <cstring>
#include <utility>

#include <dal/curve/ratestructuraljacobian_internal.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace RateStructuralJacobianInternal {
        void Canonical_::Add(size_t value) {
            static_assert(sizeof(size_t) <= sizeof(uint64_t));
            auto word = static_cast<uint64_t>(value);
            for (int byte = 0; byte < 8; ++byte) {
                bytes_->push_back(static_cast<char>(word & 0xff));
                word >>= 8;
            }
        }

        void Canonical_::Add(int value) { Add(static_cast<size_t>(static_cast<int64_t>(value))); }
        void Canonical_::Add(bool value) { bytes_->push_back(value ? '\1' : '\0'); }
        void Canonical_::Add(double value) {
            static_assert(sizeof(double) == sizeof(uint64_t));
            uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            for (int byte = 0; byte < 8; ++byte) {
                bytes_->push_back(static_cast<char>(bits & 0xff));
                bits >>= 8;
            }
        }
        void Canonical_::Add(const String_& value) {
            Add(value.size());
            bytes_->append(value.begin(), value.end());
        }
        void Canonical_::Add(const char* value) {
            Add(value != nullptr);
            if (value)
                Add(String_(value));
        }
        void Canonical_::Add(const Date_& value) { Add(Date::ToExcel(value)); }
        void Canonical_::Add(const DateTime_& value) { Fields(value.Date(), value.Frac()); }

        void AppendCurveDefinition(const CurveParameterState_& state, Canonical_* record) {
            const auto& definition = state.definition_;
            record->Fields(definition.name_, definition.ccy_, definition.parameterization_.String(), definition.logDfScheme_.String(),
                           definition.dayCount_.String(), definition.anchorDate_, definition.nodeDates_.size());
            for (const auto& date : definition.nodeDates_)
                record->Add(date);
            const auto parameters = DescribeCurveFreeParameters(definition);
            record->Fields(state.expectedParameterCount_, parameters.size());
            for (const auto& parameter : parameters)
                record->Fields(parameter.date_, parameter.component_.String());
        }

        namespace {
            void AppendIndex(const RateIndexConvention_& value, Canonical_* record) {
                record->Fields(value.spotLag_, value.fixingLag_, value.useProjectionCurve_, value.forecastTenor_.String(), value.dayBasis_.String(),
                               value.businessDayConvention_.String(), value.fixingHolidays_.String(), value.accrualHolidays_.String(),
                               value.endOfMonth_, value.collateral_.String());
            }

            void AppendLeg(const RateLegConvention_& value, Canonical_* record) {
                record->Fields(value.paymentLag_, value.paymentFrequency_.String(), value.dayBasis_.String(), value.businessDayConvention_.String(),
                               value.paymentConvention_.String(), value.accrualHolidays_.String(), value.paymentHolidays_.String(),
                               value.endOfMonth_);
            }

            void AppendFixing(const FixingIdentity_& value, Canonical_* record) {
                record->Fields(value.indexName_, value.fixingHour_, value.fixingMinute_);
            }

            void AppendTerms(const DepositTradeTerms_& value, Canonical_* record) {
                record->Fields(value.notional_, value.contractRate_, value.lend_, value.discountComponentKey_);
                AppendIndex(value.index_, record);
            }

            void AppendTerms(const FraTradeTerms_& value, Canonical_* record) {
                record->Fields(value.notional_, value.contractRate_, value.receiveFloating_, value.settleAtStart_, value.forecastComponentKey_,
                               value.discountComponentKey_);
                AppendIndex(value.index_, record);
                AppendFixing(value.fixingIdentity_, record);
            }

            void AppendTerms(const FutureTradeTerms_& value, Canonical_* record) {
                record->Fields(value.contractCount_, value.long_, value.referencePrice_, value.contractValuePerPricePoint_,
                               value.convexityAdjustment_, value.forecastComponentKey_);
                AppendIndex(value.index_, record);
                AppendFixing(value.fixingIdentity_, record);
            }

            void AppendFixedFloat(const FixedFloatTradeTerms_& value, Canonical_* record) {
                record->Fields(value.notional_, value.contractRate_, value.payFixed_, value.forecastComponentKey_, value.discountComponentKey_);
                AppendLeg(value.fixedLeg_, record);
                AppendLeg(value.floatLeg_, record);
                AppendIndex(value.floatIndex_, record);
                AppendFixing(value.fixingIdentity_, record);
            }
            void AppendTerms(const OisTradeTerms_& value, Canonical_* record) { AppendFixedFloat(value.value_, record); }
            void AppendTerms(const IrsTradeTerms_& value, Canonical_* record) { AppendFixedFloat(value.value_, record); }

            void AppendTerms(const BasisTradeTerms_& value, Canonical_* record) {
                record->Fields(value.notional_, value.contractSpread_, value.receiveReferencePaySpread_, value.spreadForecastComponentKey_,
                               value.referenceForecastComponentKey_, value.discountComponentKey_);
                AppendLeg(value.spreadLeg_, record);
                AppendLeg(value.referenceLeg_, record);
                AppendIndex(value.spreadIndex_, record);
                AppendIndex(value.referenceIndex_, record);
                AppendFixing(value.spreadFixingIdentity_, record);
                AppendFixing(value.referenceFixingIdentity_, record);
            }

            void AppendTerms(const XccyTradeTerms_& value, Canonical_* record) {
                record->Fields(value.positionCount_, value.contractSpread_, value.spreadOnForeignLeg_, value.receiveNonSpreadPaySpread_);
                const auto& config = value.config_;
                record->Fields(config.pair_.domestic_.String(), config.pair_.foreign_.String(), config.domesticNotional_, config.foreignNotional_,
                               config.notionalMode_.String());
                const auto& convention = config.convention_;
                record->Fields(convention.initialNotionalExchange_, convention.finalNotionalExchange_, convention.spreadOnForeignLeg_);
                AppendIndex(convention.domesticIndex_, record);
                AppendIndex(convention.foreignIndex_, record);
                AppendLeg(convention.domesticLeg_, record);
                AppendLeg(convention.foreignLeg_, record);
                record->Fields(config.fxReset_.fixingLag_, config.fxReset_.fixingHolidays_.String(), config.fxReset_.fixingConvention_.String(),
                               config.fxReset_.fixingHour_, config.fxReset_.fixingMinute_);
                AppendFixing(config.domesticRateFixing_, record);
                AppendFixing(config.foreignRateFixing_, record);
            }
        } // namespace

        void AppendTrade(const RateTradeDefinition_& trade, Canonical_* record) {
            record->Fields(trade.instrumentId_, trade.instrumentType_.String(), trade.tradeDate_, trade.startDate_, trade.maturityDate_,
                           trade.currencyOrPair_.String(), trade.terms_.index());
            std::visit([&](const auto& terms) { AppendTerms(terms, record); }, trade.terms_);
        }
    } // namespace RateStructuralJacobianInternal

    RateStructuralJacobianDescriptor_::RateStructuralJacobianDescriptor_(std::shared_ptr<const Data_> data) : data_(std::move(data)) {}

    const RateStructuralJacobianDescriptor_::Data_& RateStructuralJacobianDescriptor_::Data() const {
        REQUIRE(data_, "RateStructuralJacobian: descriptor must be assigned before use");
        return *data_;
    }

    bool RateStructuralJacobianDescriptor_::Available() const { return Data().reason_.empty(); }
    const String_& RateStructuralJacobianDescriptor_::Reason() const { return Data().reason_; }
    size_t RateStructuralJacobianDescriptor_::Inputs() const { return Data().inputs_; }
    size_t RateStructuralJacobianDescriptor_::Outputs() const { return Data().supports_.size(); }
    const Vector_<RateCurveParameterCoordinate_>& RateStructuralJacobianDescriptor_::InputAxis() const { return Data().inputAxis_; }
    const Vector_<String_>& RateStructuralJacobianDescriptor_::OutputAxis() const { return Data().outputAxis_; }

    const Vector_<size_t>& RateStructuralJacobianDescriptor_::RowSupport(size_t row) const {
        REQUIRE(Available(), "RateStructuralJacobian: dependency proof unavailable: " + Reason());
        REQUIRE(row < Outputs(), "RateStructuralJacobian: output row is outside the descriptor");
        return Data().supports_[row];
    }

    RateStructuralJacobianPlan_::RateStructuralJacobianPlan_(RateStructuralJacobianDescriptor_ descriptor, AAD::StructuralJacobianPlan_ numeric)
        : descriptor_(std::move(descriptor)), numeric_(std::move(numeric)) {}

    RateStructuralJacobianPlan_ PlanRateStructuralJacobian(const RateStructuralJacobianDescriptor_& descriptor,
                                                           const AAD::StructuralJacobianSettings_& settings) {
        REQUIRE(descriptor.Available(), "RateStructuralJacobian: dependency proof unavailable: " + descriptor.Reason());
        Vector_<Vector_<size_t>> supports;
        supports.reserve(descriptor.Outputs());
        for (size_t row = 0; row < descriptor.Outputs(); ++row)
            supports.push_back(descriptor.RowSupport(row));
        return RateStructuralJacobianPlan_(descriptor, AAD::PlanStructuralJacobian(descriptor.Inputs(), supports, settings));
    }

    bool SameRateStructuralJacobianStructure(const RateStructuralJacobianDescriptor_& stored, const RateStructuralJacobianDescriptor_& current) {
        return stored.Available() && current.Available() && stored.Data().inputs_ == current.Data().inputs_ &&
               stored.Data().supports_ == current.Data().supports_ && stored.Data().canonical_ == current.Data().canonical_;
    }
} // namespace Dal
