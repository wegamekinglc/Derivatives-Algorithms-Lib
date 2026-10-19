#pragma once

#include <limits>
#include <memory>
#include <optional>

#include <dal/model/ivs.hpp>
#include <dal/model/surface/lvmodel.hpp>
#include <dal/protocol/optiontype.hpp>
#include <dal/time/date.hpp>

/*IF--------------------------------------------------------------------------
enumeration MarketIvQuoteType
    Market quote units
switchable
alternative PRICE
alternative DECIMAL_IV
-IF-------------------------------------------------------------------------*/
/*IF--------------------------------------------------------------------------
enumeration MarketIvQuantity
    Market surface refit output
switchable
alternative TOTAL_VARIANCE
alternative IMPLIED_VOL
alternative CALL_PRICE
alternative PUT_PRICE
alternative LOCAL_VOL
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_MarketIvQuantity_enum.hpp>
#include <dal/auto/MG_MarketIvQuoteType_enum.hpp>

    struct MarketIvTerm_ {
        Date_ expiry_;
        double forward_ = std::numeric_limits<double>::quiet_NaN(), discount_ = std::numeric_limits<double>::quiet_NaN();
    };
    class MarketIvMarket_ {
        Date_ evaluationDate_;
        double spot_;
        Vector_<MarketIvTerm_> terms_;
        bool flat_ = false;
        double rate_ = 0., dividendYield_ = 0.;
        friend MarketIvMarket_ NewFlatMarketIvMarket(const Date_&, double, double, double, const Vector_<Date_>&);

    public:
        MarketIvMarket_(const Date_& evaluationDate, double spot, const Vector_<MarketIvTerm_>& terms);
        [[nodiscard]] Date_ EvaluationDate() const { return evaluationDate_; }
        [[nodiscard]] double Spot() const { return spot_; }
        [[nodiscard]] const Vector_<MarketIvTerm_>& Terms() const { return terms_; }
        [[nodiscard]] bool IsFlatCarry() const { return flat_; }
        [[nodiscard]] double Rate() const { return rate_; }
        [[nodiscard]] double DividendYield() const { return dividendYield_; }
        [[nodiscard]] String_ CarryMode() const { return flat_ ? "FLAT_CARRY" : "FORWARD_DISCOUNT_KNOTS"; }
    };
    struct MarketIvQuote_ {
        String_ id_;
        Date_ expiry_;
        double strike_ = std::numeric_limits<double>::quiet_NaN();
        OptionType_ optionType_;
        MarketIvQuoteType_ quoteType_;
        double mid_ = std::numeric_limits<double>::quiet_NaN();
        std::optional<double> bid_, ask_;
        double weight_ = 1.;
    };
    struct MarketIvFitSettings_ {
        double rhoLimit_ = .999, constraintMargin_ = 1e-8, thetaFloor_ = 1e-10, maxVarianceIncrement_ = 100., initialVolatility_ = .20;
        double projectedGradientTolerance_ = 1e-8, parameterStepTolerance_ = 1e-10, objectiveTieTolerance_ = 1e-12;
        double normalizedPriceTolerance_ = 1e-4, decimalIvTolerance_ = 5e-4, normalizedPriceScaleFloor_ = 1e-4, feasibilityTolerance_ = 1e-10;
        int maxIterationsPerStart_ = 200, maxLineSearchTrials_ = 16;
        size_t maxModelRowEvaluations_ = 5000000, maxQuotes_ = 10000, maxExpiries_ = 64, maxMarketTerms_ = 256;
        size_t numericPayloadBudgetBytes_ = 67108864, workspaceBudgetBytes_ = 268435456;
    };
    struct MarketIvEvaluationSettings_ {
        double densityFactorFloor_ = 1e-10, minQueryTime_ = 1e-8, minTotalVariance_ = 1e-12;
    };
    struct MarketIvFitRequest_ {
        MarketIvMarket_ market_;
        Vector_<MarketIvQuote_> quotes_;
        MarketIvFitSettings_ settings_;
    };
    struct MarketIvQuery_ {
        double strike_ = std::numeric_limits<double>::quiet_NaN(), time_ = std::numeric_limits<double>::quiet_NaN();
    };
    struct MarketIvExecution_ {
        size_t plannedNumericPayloadBytes_ = 0, plannedWorkspaceBytes_ = 0, numericPayloadBytes_ = 0, peakWorkspaceBytes_ = 0;
        size_t modelRowEvaluations_ = 0, quoteConversions_ = 0, outputEvaluations_ = 0;
        size_t quoteCapacity_ = 0, iterationCapacity_ = 0, jacobianCapacity_ = 0;
    };
    struct MarketIvIteration_ {
        int start_ = 0, iteration_ = 0, trials_ = 0;
        double objective_ = 0., projectedGradient_ = 0., gradientThreshold_ = 0., damping_ = 0.;
    };
    struct MarketIvStart_ {
        String_ termination_ = "NOT_RUN";
        String_ failure_;
        bool projectedSeed_ = false, converged_ = false, certified_ = false, qualityAccepted_ = false;
        int iterations_ = 0, rank_ = 0;
        std::optional<double> objective_, projectedGradient_, conditionEstimate_;
        Vector_<> parameters_;
    };
    struct MarketIvQuoteDiagnostic_ {
        size_t inputRow_ = 0, expiryIndex_ = 0;
        double time_ = 0., forward_ = 0., discount_ = 0., logMoneyness_ = 0.;
        double normalizedMid_ = 0., normalizedBid_ = 0., normalizedAsk_ = 0., scale_ = 0., normalizedWeight_ = 0.;
        std::optional<double> fittedNormalizedCall_, normalizedResidual_, nativeFitted_, nativeResidual_, bandViolation_, parityGap_;
        bool withinBand_ = false;
        int bandBranch_ = 0;
    };
    struct MarketIvCertificate_ {
        String_ method_ = "SSVI_GammaHalf_MonotoneTheta_v1", domain_ = "All real k; all t > 0; deterministic proportional carry";
        double rhoMargin_ = 0., etaMargin_ = 0., butterflyBound_ = 0., wingSlopeBound_ = 0.;
        Vector_<> derivativeLowerBounds_, increments_;
        Vector_<int> limiterBranches_;
        bool certified_ = false;
    };
    namespace MarketIvDetail {
        struct FitData_;
        struct PointData_;
        struct DupireData_;
        struct RiskData_;
        struct Access_;
        struct PointAccess_;
    } // namespace MarketIvDetail
    class MarketIvCalibrationSnapshot_ {
        std::shared_ptr<const MarketIvDetail::FitData_> data_;
        explicit MarketIvCalibrationSnapshot_(std::shared_ptr<const MarketIvDetail::FitData_> data) : data_(std::move(data)) {}
        friend class MarketIvFitResult_;
        friend const MarketIvDetail::FitData_& MarketIvData(const MarketIvCalibrationSnapshot_&);

    public:
        [[nodiscard]] const MarketIvFitRequest_& Request() const;
        [[nodiscard]] const Vector_<>& Times() const;
        [[nodiscard]] const Vector_<>& Theta() const;
        [[nodiscard]] const Vector_<>& Slopes() const;
        [[nodiscard]] const MarketIvCertificate_& Certificate() const;
        [[nodiscard]] double Rho() const;
        [[nodiscard]] double Eta() const;
        [[nodiscard]] const Vector_<int>& RegimeSignature() const;
        [[nodiscard]] const MarketIvExecution_& Execution() const;
    };
    class MarketIvFitResult_ {
        std::shared_ptr<const MarketIvDetail::FitData_> data_;
        explicit MarketIvFitResult_(std::shared_ptr<const MarketIvDetail::FitData_> data) : data_(std::move(data)) {}
        friend MarketIvFitResult_ FitMarketIvSurface(const MarketIvFitRequest_&);
        friend struct MarketIvDetail::Access_;

    public:
        [[nodiscard]] bool IsSuccess() const;
        [[nodiscard]] const String_& Status() const;
        [[nodiscard]] const String_& Failure() const;
        [[nodiscard]] String_ Method() const { return "ConstrainedSSVIFit"; }
        [[nodiscard]] MarketIvCalibrationSnapshot_ Snapshot() const;
        [[nodiscard]] const MarketIvFitRequest_& Request() const;
        [[nodiscard]] const Vector_<MarketIvQuoteDiagnostic_>& Quotes() const;
        [[nodiscard]] const Vector_<MarketIvIteration_>& Iterations() const;
        [[nodiscard]] const Vector_<MarketIvStart_>& Starts() const;
        [[nodiscard]] const MarketIvExecution_& Execution() const;
        [[nodiscard]] int SelectedStart() const;
        [[nodiscard]] bool Underidentified() const;
        [[nodiscard]] bool NearTie() const;
    };
    class MarketIvPointResult_ {
        String_ valueStatus_ = "NUMERIC_RANGE", derivativeStatus_ = "NUMERIC_RANGE", dupireStatus_ = "NUMERIC_RANGE", carryDerivative_;
        std::optional<double> totalVariance_, varianceK_, varianceKK_, varianceT_, impliedVol_, callPrice_, putPrice_;
        std::optional<double> densityFactor_, localVariance_, localVol_, callK_, callKK_, callT_, fixedKVarianceT_;
        friend MarketIvPointResult_
        EvaluateMarketIvSurface(const MarketIvCalibrationSnapshot_&, const MarketIvQuery_&, const MarketIvEvaluationSettings_&);
        friend struct MarketIvDetail::PointAccess_;

    public:
        [[nodiscard]] const String_& ValueStatus() const { return valueStatus_; }
        [[nodiscard]] const String_& DerivativeStatus() const { return derivativeStatus_; }
        [[nodiscard]] const String_& DupireStatus() const { return dupireStatus_; }
        [[nodiscard]] const String_& CarryDerivative() const { return carryDerivative_; }
        [[nodiscard]] std::optional<double> TotalVariance() const { return totalVariance_; }
        [[nodiscard]] std::optional<double> VarianceK() const { return varianceK_; }
        [[nodiscard]] std::optional<double> VarianceKK() const { return varianceKK_; }
        [[nodiscard]] std::optional<double> VarianceT() const { return varianceT_; }
        [[nodiscard]] std::optional<double> FixedKVarianceT() const { return fixedKVarianceT_; }
        [[nodiscard]] std::optional<double> ImpliedVol() const { return impliedVol_; }
        [[nodiscard]] std::optional<double> CallPrice() const { return callPrice_; }
        [[nodiscard]] std::optional<double> PutPrice() const { return putPrice_; }
        [[nodiscard]] std::optional<double> CallK() const { return callK_; }
        [[nodiscard]] std::optional<double> CallKK() const { return callKK_; }
        [[nodiscard]] std::optional<double> CallT() const { return callT_; }
        [[nodiscard]] std::optional<double> DensityFactor() const { return densityFactor_; }
        [[nodiscard]] std::optional<double> LocalVariance() const { return localVariance_; }
        [[nodiscard]] std::optional<double> LocalVol() const { return localVol_; }
    };
    struct MarketIvDupireSettings_ {
        MarketIvEvaluationSettings_ evaluation_;
        size_t maxGridPoints_ = 100000, numericPayloadBudgetBytes_ = 67108864, workspaceBudgetBytes_ = 268435456;
    };
    struct MarketIvDupireRequest_ {
        Vector_<> spots_, times_;
        MarketIvDupireSettings_ settings_;
    };
    class MarketIvDupireResult_ {
        std::shared_ptr<const MarketIvDetail::DupireData_> data_;
        explicit MarketIvDupireResult_(std::shared_ptr<const MarketIvDetail::DupireData_> data) : data_(std::move(data)) {}
        friend MarketIvDupireResult_ CalibrateMarketIvDupire(const MarketIvCalibrationSnapshot_&, const MarketIvDupireRequest_&);

    public:
        [[nodiscard]] bool IsSuccess() const;
        [[nodiscard]] const String_& Failure() const;
        [[nodiscard]] const MarketIvCalibrationSnapshot_& Snapshot() const;
        [[nodiscard]] const MarketIvDupireRequest_& Request() const;
        [[nodiscard]] const Vector_<MarketIvPointResult_>& Nodes() const;
        [[nodiscard]] const LocalVolSurfaceData_& SurfaceData() const;
        [[nodiscard]] const MarketIvExecution_& Execution() const;
    };
    struct MarketIvRefitOutput_ {
        MarketIvQuery_ query_;
        MarketIvQuantity_ quantity_;
    };
    struct MarketIvRefitRiskSettings_ {
        MarketIvEvaluationSettings_ evaluation_;
        size_t maxSelectedQuotes_ = 32, maxOutputs_ = 128;
        std::optional<size_t> maxTotalModelRowEvaluations_;
        size_t numericPayloadBudgetBytes_ = 67108864, workspaceBudgetBytes_ = 268435456;
    };
    struct MarketIvRefitRiskRequest_ {
        Vector_<String_> quoteIds_;
        Vector_<MarketIvRefitOutput_> outputs_;
        MarketIvRefitRiskSettings_ settings_;
        Vector_<> steps_;
    };
    struct MarketIvRefitResponse_ {
        size_t quoteIndex_ = 0, outputIndex_ = 0;
        double step_ = 0.;
        std::optional<double> base_, plus_, minus_, symmetricBumpSlope_, plusSlope_, minusSlope_;
        bool regimeChanged_ = false;
        unsigned char plusOutputCode_ = 0, minusOutputCode_ = 0;
        [[nodiscard]] String_ PlusOutputStatus() const { return OutputStatus(plusOutputCode_); }
        [[nodiscard]] String_ MinusOutputStatus() const { return OutputStatus(minusOutputCode_); }
        [[nodiscard]] String_ Status() const { return symmetricBumpSlope_ ? (regimeChanged_ ? "REGIME_CHANGED" : "SUCCESS") : "INCOMPLETE"; }

    private:
        static String_ OutputStatus(unsigned char code) {
            static const char* const names[]{"NOT_RUN", "SUCCESS", "NUMERIC_RANGE", "SINGULAR_DENSITY"};
            REQUIRE(code < 4, "InvalidMarketIvRequest: invalid response output status code");
            return names[code];
        }
    };
    struct MarketIvRefitAxis_ {
        size_t quoteIndex_ = 0;
        double step_ = 0.;
        String_ plusStatus_ = "NOT_RUN", minusStatus_ = "NOT_RUN", plusFailure_, minusFailure_, inputUnit_;
        bool regimeChanged_ = false;
        int plusStart_ = -1, minusStart_ = -1, plusRank_ = 0, minusRank_ = 0;
    };
    class MarketIvRefitRiskResult_ {
        std::shared_ptr<const MarketIvDetail::RiskData_> data_;
        explicit MarketIvRefitRiskResult_(std::shared_ptr<const MarketIvDetail::RiskData_> data) : data_(std::move(data)) {}
        friend MarketIvRefitRiskResult_ EvaluateMarketIvRefitRisk(const MarketIvCalibrationSnapshot_&, const MarketIvRefitRiskRequest_&);

    public:
        [[nodiscard]] bool Complete() const;
        [[nodiscard]] String_ Method() const { return "FullRefitSymmetricBump"; }
        [[nodiscard]] const MarketIvCalibrationSnapshot_& Snapshot() const;
        [[nodiscard]] const MarketIvRefitRiskRequest_& Request() const;
        [[nodiscard]] const Vector_<MarketIvRefitResponse_>& Responses() const;
        [[nodiscard]] const Vector_<MarketIvRefitAxis_>& Axis() const;
        [[nodiscard]] const MarketIvExecution_& Execution() const;
    };

    void ValidateMarketIvFitSettings(const MarketIvFitSettings_&);
    void ValidateMarketIvEvaluationSettings(const MarketIvEvaluationSettings_&);
    void ValidateMarketIvFitRequest(const MarketIvFitRequest_&);
    void ValidateMarketIvDupireRequest(const MarketIvDupireRequest_&);
    void ValidateMarketIvRefitRiskRequest(const MarketIvRefitRiskRequest_&);
    MarketIvMarket_ NewFlatMarketIvMarket(const Date_&, double spot, double rate, double dividendYield, const Vector_<Date_>& expiries);
    MarketIvFitResult_ FitMarketIvSurface(const MarketIvFitRequest_&);
    MarketIvFitResult_ RefitMarketIvSurface(const MarketIvCalibrationSnapshot_&, const Vector_<MarketIvQuote_>&);
    MarketIvPointResult_ EvaluateMarketIvSurface(const MarketIvCalibrationSnapshot_&, const MarketIvQuery_&, const MarketIvEvaluationSettings_& = {});
    MarketIvDupireResult_ CalibrateMarketIvDupire(const MarketIvCalibrationSnapshot_&, const MarketIvDupireRequest_&);
    MarketIvRefitRiskResult_ EvaluateMarketIvRefitRisk(const MarketIvCalibrationSnapshot_&, const MarketIvRefitRiskRequest_&);
    Handle_<AAD::IVS_> MarketIvSurfaceAsLegacyIVS(const MarketIvCalibrationSnapshot_&);
} // namespace Dal
