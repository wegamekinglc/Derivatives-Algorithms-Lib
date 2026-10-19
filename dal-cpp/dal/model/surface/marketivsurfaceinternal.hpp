#pragma once

#include <array>
#include <cmath>
#include <limits>

#include <dal/model/surface/marketivsurface.hpp>

namespace Dal::MarketIvDetail {
    static_assert(sizeof(MarketIvRefitResponse_) <= 128, "Refit response exceeds its per-output payload envelope");
    static_assert(sizeof(MarketIvRefitAxis_) <= 512, "Refit axis exceeds its payload envelope");
    static_assert(sizeof(MarketIvPointResult_) + 2 * sizeof(double) <= 512, "Dupire node and matrix exceed their payload envelope");
    static_assert(sizeof(MarketIvQuote_) + sizeof(MarketIvQuoteDiagnostic_) <= 512, "Quote snapshot exceeds its payload envelope");
    struct ThetaCurve_ {
        Vector_<> times_, values_, slopes_, lowerBounds_;
        Vector_<int> branches_;
        bool certified_ = false;
        size_t peakWorkspaceBytes_ = 0;
    };
    struct Carry_ {
        double forward_, discount_, rate_, yield_;
        bool knot_;
    };
    struct Smile_ {
        double w_, wk_, wkk_, wTheta_, wRho_, wEta_;
        bool derivativeRange_ = false;
    };
    struct FitData_ {
        explicit FitData_(const MarketIvFitRequest_& request) : request_(request) {}
        MarketIvFitRequest_ request_;
        String_ status_ = "NO_CONVERGENCE", failure_;
        Vector_<MarketIvQuoteDiagnostic_> quotes_;
        Vector_<MarketIvStart_> starts_;
        Vector_<MarketIvIteration_> iterations_;
        Vector_<> expiryTimes_, parameters_;
        ThetaCurve_ curve_;
        MarketIvCertificate_ certificate_;
        Vector_<int> signature_;
        MarketIvExecution_ execution_;
        size_t rowLimit_ = 0;
        int selectedStart_ = -1;
        bool underidentified_ = false, nearTie_ = false;
    };
    struct DupireData_ {
        DupireData_(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request) : snapshot_(snapshot), request_(request) {}
        MarketIvCalibrationSnapshot_ snapshot_;
        MarketIvDupireRequest_ request_;
        Vector_<MarketIvPointResult_> nodes_;
        Handle_<LocalVolSurfaceData_> surface_;
        MarketIvExecution_ execution_;
        String_ failure_;
    };
    struct RiskData_ {
        RiskData_(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvRefitRiskRequest_& request) : snapshot_(snapshot), request_(request) {}
        MarketIvCalibrationSnapshot_ snapshot_;
        MarketIvRefitRiskRequest_ request_;
        Vector_<MarketIvRefitResponse_> responses_;
        Vector_<MarketIvRefitAxis_> axis_;
        MarketIvExecution_ execution_;
        bool complete_ = true;
    };
    ThetaCurve_ NewThetaCurve(const Vector_<>& times, const Vector_<>& values);
    std::pair<double, double> ThetaAt(const ThetaCurve_&, double time);
    double NormalizePrice(double premium, bool call, double forward, double strike, double discount, double maxError = 1e-12);
    double NormalizedBlack(double k, double variance, bool call = true);
    Smile_ Smile(double theta, double rho, double eta, double k);
    Carry_ CarryAt(const MarketIvMarket_&, double time);
    double EtaMax(const MarketIvFitSettings_&);
    std::pair<double, double> GlobalBounds(double eta, double rhoLimit);
    bool ParametersCertified(const MarketIvFitSettings_&, double rho, double eta);
    MarketIvExecution_ PlanFit(size_t quotes, size_t expiries, size_t terms, const MarketIvFitSettings_&);
    size_t CheckedAdd(size_t a, size_t b);
    size_t CheckedMultiply(size_t a, size_t b);
    void CheckBudget(size_t payload, size_t workspace, size_t payloadBudget, size_t workspaceBudget);
    void ValidateQuery(const MarketIvQuery_&);
    std::optional<double> Quantity(const MarketIvPointResult_&, const MarketIvQuantity_&);
    size_t EvaluationWorkspaceBytes();
    size_t FitPayload(const FitData_&);
    int SelectStart(const Vector_<MarketIvStart_>& starts, double tieTolerance);
    std::shared_ptr<const FitData_> Fit(const MarketIvFitRequest_&, std::optional<size_t> aggregateRemaining = std::nullopt);
    struct Access_ {
        static MarketIvFitResult_ Result(std::shared_ptr<const FitData_> data) { return MarketIvFitResult_(std::move(data)); }
    };
    template <class T_> size_t Bytes(const Vector_<T_>& v) { return sizeof(T_) * v.capacity(); }
} // namespace Dal::MarketIvDetail
