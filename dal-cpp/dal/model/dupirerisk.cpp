//
// Created by Codex on 2026/10/5.
//

#include <array>
#include <limits>
#include <map>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/dupirerisk.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    namespace {
        using SamplePoint_ = std::pair<double, double>;
        using BaseSamples_ = std::map<SamplePoint_, double>;
        using Band_ = std::pair<size_t, size_t>;

        struct FrozenBase_ {
            double spot_;
            double rate_;
            double dividend_;
            BaseSamples_ samples_;
            Vector_<Band_> bands_;
        };

        class FrozenIVS_ final : public AAD::IVS_ {
            const BaseSamples_& samples_;

        public:
            explicit FrozenIVS_(const FrozenBase_& base) : IVS_(base.spot_, base.rate_, base.dividend_), samples_(base.samples_) {}
            [[nodiscard]] double ImpliedVol(double strike, double maturity) const override {
                const auto found = samples_.find({strike, maturity});
                REQUIRE(found != samples_.end(), "InvalidDupireCalibration: replay requested an unsampled base coordinate");
                return found->second;
            }
        };

        void ValidateAxis(const Vector_<>& axis, const String_& field) {
            REQUIRE(!axis.empty() && axis.size() <= static_cast<size_t>(std::numeric_limits<int>::max()),
                    "InvalidDupireQuote: empty or oversized axis; field=" + field);
            for (size_t ordinal = 0; ordinal < axis.size(); ++ordinal)
                REQUIRE(std::isfinite(axis[ordinal]) && axis[ordinal] > 0.0 && (ordinal == 0 || axis[ordinal] > axis[ordinal - 1]),
                        "InvalidDupireQuote: axis must be finite, positive and strictly increasing; field=" + field);
        }

        void ValidateSpacing(const Vector_<>& axis, double spacing, bool addSpacing) {
            REQUIRE(std::isfinite(spacing) && spacing > 0.0, "InvalidDupireCalibration: grid spacing must be finite and positive");
            const double low = addSpacing ? std::min(axis.front(), spacing) : axis.front();
            const double high = addSpacing ? std::max(axis.back(), spacing) : axis.back();
            REQUIRE(high == low || spacing >= high - std::nextafter(high, 0.0),
                    "InvalidDupireCalibration: grid spacing is below coordinate precision");
            REQUIRE((high - low) / spacing < std::numeric_limits<int>::max() - 2.0,
                    "InvalidDupireCalibration: grid spacing exceeds the supported fill extent");
        }

        void ValidateInputs(const DupireRiskInputs_& inputs) {
            ValidateAxis(inputs.quoteStrikes_, "quoteStrikes");
            ValidateAxis(inputs.quoteMaturities_, "quoteMaturities");
            ValidateAxis(inputs.inclusionSpots_, "inclusionSpots");
            ValidateAxis(inputs.inclusionTimes_, "inclusionTimes");
            REQUIRE(inputs.quoteSpreads_.Rows() == static_cast<int>(inputs.quoteStrikes_.size()) &&
                        inputs.quoteSpreads_.Cols() == static_cast<int>(inputs.quoteMaturities_.size()),
                    "InvalidDupireQuote: spread matrix must match strike/maturity axes");
            for (double spread : inputs.quoteSpreads_)
                REQUIRE(std::isfinite(spread), "InvalidDupireQuote: non-finite spread");
            ValidateSpacing(inputs.inclusionSpots_, inputs.maxSpotSpacing_, false);
            ValidateSpacing(inputs.inclusionTimes_, inputs.maxTimeSpacing_, true);
        }

        std::array<SamplePoint_, 5> StencilPoints(double strike, double maturity) {
            const double dt = 1e-4 * maturity;
            const double ds = 1e-4 * strike;
            return {{{strike, maturity}, {strike, maturity - dt}, {strike, maturity + dt}, {strike - ds, maturity}, {strike + ds, maturity}}};
        }

        void SampleBase(const AAD::IVS_& source, const SamplePoint_& point, BaseSamples_* samples) {
            if (samples->find(point) != samples->end())
                return;
            const double vol = source.ImpliedVol(point.first, point.second);
            REQUIRE(std::isfinite(vol) && vol >= 0.0, "InvalidDupireCalibration: base implied volatility must be finite and nonnegative");
            samples->emplace(point, vol);
        }

        Band_ StableBand(const FrozenIVS_& ivs, const Vector_<>& spots, double maturity) {
            const double atmCall = ivs.Call(ivs.Spot(), maturity);
            const double width = 2.5 * atmCall * M_SQRT_2_PI;
            REQUIRE(std::isfinite(width) && width >= 0.0, "InvalidDupireCalibration: non-finite or negative base ATM call band");
            const auto low = std::lower_bound(spots.begin(), spots.end(), ivs.Spot() - width);
            const auto high = std::upper_bound(spots.begin(), spots.end(), ivs.Spot() + width);
            REQUIRE(low < high, "InvalidDupireCalibration: no spot nodes inside stable calibration band");
            return {static_cast<size_t>(low - spots.begin()), static_cast<size_t>(high - spots.begin())};
        }

        FrozenBase_ FreezeBase(const AAD::IVS_& source, const Vector_<>& spots, const Vector_<>& times) {
            FrozenBase_ base{source.Spot(), source.Rate(), source.DividendYield(), {}, {}};
            REQUIRE(std::isfinite(base.spot_) && base.spot_ > 0.0 && std::isfinite(base.rate_) && std::isfinite(base.dividend_),
                    "InvalidDupireCalibration: spot and deterministic carry must be finite with positive spot");
            const FrozenIVS_ frozen(base);
            for (double maturity : times) {
                SampleBase(source, {base.spot_, maturity}, &base.samples_);
                const auto band = StableBand(frozen, spots, maturity);
                base.bands_.push_back(band);
                for (size_t row = band.first; row < band.second; ++row)
                    for (const auto& point : StencilPoints(spots[row], maturity))
                        SampleBase(source, point, &base.samples_);
            }
            return base;
        }

        double CheckedCall(const FrozenIVS_& ivs, const AAD::RiskView_<double>& quotes, const SamplePoint_& point) {
            const double impliedVol = ivs.ImpliedVol(point.first, point.second) + quotes.Spread(point.first, point.second);
            REQUIRE(std::isfinite(impliedVol) && impliedVol > 0.0, "InvalidDupireQuote: bumped implied volatility must be finite and positive");
            const double call = ivs.Call(point.first, point.second, &quotes);
            REQUIRE(std::isfinite(call), "InvalidDupireCalibration: non-finite stencil call");
            return call;
        }

        void ValidateStencil(const FrozenIVS_& ivs, const AAD::RiskView_<double>& quotes, double strike, double maturity) {
            const auto points = StencilPoints(strike, maturity);
            std::array<double, 5> calls;
            for (size_t index = 0; index < points.size(); ++index)
                calls[index] = CheckedCall(ivs, quotes, points[index]);
            const double dt = 1e-4 * maturity;
            const double ds = 1e-4 * strike;
            const double centered = calls[3] + calls[4] - 2.0 * calls[0];
            const double scale = std::abs(calls[3]) + std::abs(calls[4]) + 2.0 * std::abs(calls[0]);
            REQUIRE(centered > 8.0 * std::numeric_limits<double>::epsilon() * scale,
                    "InvalidDupireCalibration: discrete call curvature is nonpositive or numerically unresolved");
            const double curvature = centered / ds / ds;
            const double ct = (calls[2] - calls[1]) * 0.5 / dt;
            const double ck = (calls[4] - calls[3]) * 0.5 / ds;
            const double variance = 2.0 * (ct + ivs.DividendYield() * calls[0] + (ivs.Rate() - ivs.DividendYield()) * strike * ck) / curvature;
            REQUIRE(std::isfinite(curvature) && std::isfinite(ct) && std::isfinite(ck) && std::isfinite(variance) && variance > 0.0,
                    "InvalidDupireCalibration: local variance is nonpositive or non-finite");
        }

        AAD::RiskView_<double> NumericQuotes(const DupireRiskInputs_& inputs) {
            AAD::RiskView_<double> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
            std::copy(inputs.quoteSpreads_.begin(), inputs.quoteSpreads_.end(), quotes.begin());
            return quotes;
        }

        void ValidateDomains(const FrozenBase_& base, const Vector_<>& spots, const Vector_<>& times, const AAD::RiskView_<double>& quotes) {
            const FrozenIVS_ ivs(base);
            for (size_t column = 0; column < times.size(); ++column)
                for (size_t row = base.bands_[column].first; row < base.bands_[column].second; ++row)
                    ValidateStencil(ivs, quotes, spots[row], times[column]);
        }

        bool SameMatrix(const Matrix_<>& lhs, const Matrix_<>& rhs) {
            return lhs.Rows() == rhs.Rows() && lhs.Cols() == rhs.Cols() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }

        bool SameQuotes(const DupireRiskInputs_& lhs, const DupireRiskInputs_& rhs) {
            return lhs.quoteStrikes_ == rhs.quoteStrikes_ && lhs.quoteMaturities_ == rhs.quoteMaturities_ &&
                   SameMatrix(lhs.quoteSpreads_, rhs.quoteSpreads_);
        }

        bool SameInputs(const DupireRiskInputs_& lhs, const DupireRiskInputs_& rhs) {
            return SameQuotes(lhs, rhs) && lhs.inclusionSpots_ == rhs.inclusionSpots_ && lhs.maxSpotSpacing_ == rhs.maxSpotSpacing_ &&
                   lhs.inclusionTimes_ == rhs.inclusionTimes_ && lhs.maxTimeSpacing_ == rhs.maxTimeSpacing_;
        }

        bool SameBase(const FrozenBase_& lhs, const FrozenBase_& rhs) {
            return lhs.spot_ == rhs.spot_ && lhs.rate_ == rhs.rate_ && lhs.dividend_ == rhs.dividend_ && lhs.samples_ == rhs.samples_ &&
                   lhs.bands_ == rhs.bands_;
        }

        void ValidateSeedMatrix(const Matrix_<>& seeds, int rows, int columns, const String_& field) {
            REQUIRE(seeds.Rows() == rows && seeds.Cols() == columns, "InvalidDupirePullback: seed dimensions disagree; field=" + field);
            for (double seed : seeds)
                REQUIRE(std::isfinite(seed), "InvalidDupirePullback: non-finite seed; field=" + field);
        }

        Matrix_<> ValidatedDirectSeeds(const DupireCalibrationSnapshot_& snapshot,
                                       const DupireParameterAdjoints_& parameters,
                                       const std::optional<DupireDirectQuoteAdjoints_>& direct) {
            REQUIRE(snapshot.Matches(parameters.calibration_), "DupireSnapshotMismatch: parameter seed calibration does not match");
            ValidateSeedMatrix(parameters.adjoints_, snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols(), "parameters");
            const auto& inputs = snapshot.Inputs();
            if (!direct)
                return Matrix_<>(inputs.quoteSpreads_.Rows(), inputs.quoteSpreads_.Cols(), 0.0);
            REQUIRE(SameQuotes(inputs, direct->calibration_.Inputs()), "DupireSnapshotMismatch: direct quote definition does not match");
            ValidateSeedMatrix(direct->adjoints_, inputs.quoteSpreads_.Rows(), inputs.quoteSpreads_.Cols(), "directQuotes");
            return direct->adjoints_;
        }

        AAD::RiskView_<AAD::Number_> RegisterQuotes(const DupireRiskInputs_& inputs, AAD::RecordingScope_* recording) {
            AAD::RiskView_<AAD::Number_> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
            auto active = quotes.begin();
            for (double value : inputs.quoteSpreads_)
                recording->RegisterInput(*active++, value);
            return quotes;
        }

        struct OPScalarPrimal_ {
            static double Eval(double, double scalarPrice) { return scalarPrice; }
            static double Derivative(double, double, double) { return 1.0; }
        };

        AAD::Number_ WithScalarPrimal(const AAD::Number_& active, double scalar) {
            if (Value(active) == scalar)
                return active;
            return AAD::UnaryExpression_<AAD::Number_, OPScalarPrimal_>(active, scalar);
        }

        struct ReplayedCall_ {
            AAD::Number_ price_;
            double roundoff_;
        };

        ReplayedCall_ ReplayCall(const FrozenIVS_& ivs,
                                 const AAD::RiskView_<AAD::Number_>& activeQuotes,
                                 const AAD::RiskView_<double>& scalarQuotes,
                                 double strike,
                                 double maturity) {
            const double scalarPrice = ivs.Call(strike, maturity, &scalarQuotes);
            const AAD::Number_ activePrice = ivs.Call(strike, maturity, &activeQuotes);
            const double scale = ivs.Spot() * std::exp(-ivs.DividendYield() * maturity) + strike * std::exp(-ivs.Rate() * maturity);
            const double roundoff = 8.0 * std::numeric_limits<double>::epsilon() * scale;
            REQUIRE(std::isfinite(scalarPrice) && std::isfinite(Value(activePrice)) && std::isfinite(scale) &&
                        std::abs(Value(activePrice) - scalarPrice) <= roundoff,
                    "InvalidDupirePullback: stencil call replay disagrees with its scalar price");
            return {WithScalarPrimal(activePrice, scalarPrice), roundoff};
        }

        void ValidateReplayVol(double value, double low, double high) {
            REQUIRE(std::isfinite(value) && value >= low && value <= high, "InvalidDupirePullback: stencil replay exceeds its rounding bound");
        }

        AAD::Number_ ReplayLocalVol(const FrozenIVS_& ivs,
                                    const AAD::RiskView_<AAD::Number_>& activeQuotes,
                                    const AAD::RiskView_<double>& scalarQuotes,
                                    double strike,
                                    double maturity,
                                    double scalarVol) {
            std::array<double, 5> calls;
            size_t ordinal = 0;
            double callError = 0.0;
            constexpr double EPSILON = std::numeric_limits<double>::epsilon();
            const auto call = [&](double k, double t) {
                const auto price = ReplayCall(ivs, activeQuotes, scalarQuotes, k, t);
                calls[ordinal++] = Value(price.price_);
                callError = std::max(callError, price.roundoff_);
                return price.price_;
            };
            const auto activeVol = AAD::Detail::DupireLocalVolFromCalls(strike, maturity, ivs.Rate(), ivs.DividendYield(), call);
            const double dt = 1e-4 * maturity;
            const double ds = 1e-4 * strike;
            const double ct = (calls[2] - calls[1]) * 0.5 / dt;
            const double ck = (calls[4] - calls[3]) * 0.5 / ds;
            const double curvature = (calls[3] + calls[4] - 2.0 * calls[0]) / ds / ds;
            const double carry = (ivs.Rate() - ivs.DividendYield()) * strike;
            const double numerator = ct + ivs.DividendYield() * calls[0] + carry * ck;
            // Bound call and stencil contraction before restoring the independently recomputed scalar primal.
            const double ctError = (2.0 * callError + 8.0 * EPSILON * (std::abs(calls[2]) + std::abs(calls[1]))) * 0.5 / dt;
            const double ckError = (2.0 * callError + 8.0 * EPSILON * (std::abs(calls[4]) + std::abs(calls[3]))) * 0.5 / ds;
            const double curvatureError =
                (4.0 * callError + 8.0 * EPSILON * (std::abs(calls[3]) + std::abs(calls[4]) + 2.0 * std::abs(calls[0]))) / ds / ds;
            const double numeratorError = ctError + std::abs(ivs.DividendYield()) * callError + std::abs(carry) * ckError +
                                          8.0 * EPSILON * (std::abs(ct) + std::abs(ivs.DividendYield() * calls[0]) + std::abs(carry * ck));
            REQUIRE(std::isfinite(numeratorError) && std::isfinite(curvatureError) && numerator > numeratorError && curvature > curvatureError,
                    "InvalidDupirePullback: stencil replay is numerically unresolved");
            const double low = std::sqrt(2.0 * (numerator - numeratorError) / (curvature + curvatureError)) / strike;
            const double high = std::sqrt(2.0 * (numerator + numeratorError) / (curvature - curvatureError)) / strike;
            const double rounding = 16.0 * EPSILON * high;
            REQUIRE(std::isfinite(low) && std::isfinite(high), "InvalidDupirePullback: stencil replay exceeds its rounding bound");
            ValidateReplayVol(scalarVol, low - rounding, high + rounding);
            ValidateReplayVol(Value(activeVol), low - rounding, high + rounding);
            return WithScalarPrimal(activeVol, scalarVol);
        }

        struct ReplayedSurface_ {
            Vector_<> spots_;
            Vector_<> times_;
            Matrix_<AAD::Number_> lVols_;
        };

        ReplayedSurface_
        ReplayCalibration(const FrozenBase_& base, const DupireRiskInputs_& inputs, const AAD::RiskView_<AAD::Number_>& activeQuotes) {
            const FrozenIVS_ ivs(base);
            const auto scalarQuotes = NumericQuotes(inputs);
            auto scalar =
                AAD::DupireCalib(ivs, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, scalarQuotes);
            ReplayedSurface_ result{std::move(scalar.spots_), std::move(scalar.times_), {}};
            result.lVols_.Resize(static_cast<int>(result.spots_.size()), static_cast<int>(result.times_.size()));
            for (int column = 0; column < result.lVols_.Cols(); ++column) {
                const auto band = base.bands_[static_cast<size_t>(column)];
                for (size_t row = band.first; row < band.second; ++row)
                    result.lVols_(static_cast<int>(row), column) =
                        ReplayLocalVol(ivs, activeQuotes, scalarQuotes, result.spots_[row], result.times_[static_cast<size_t>(column)],
                                       scalar.lVols_(static_cast<int>(row), column));
                for (size_t row = 0; row < result.spots_.size(); ++row) {
                    const auto source = std::clamp(row, band.first, band.second - 1);
                    if (source != row)
                        result.lVols_(static_cast<int>(row), column) = result.lVols_(static_cast<int>(source), column);
                }
            }
            return result;
        }

        template <class C_> bool ReplayAgrees(const C_& calibrated, const LocalVolSurfaceData_& expected) {
            if (calibrated.spots_ != expected.spots_ || calibrated.times_ != expected.times_)
                return false;
            for (int row = 0; row < expected.vols_.Rows(); ++row)
                for (int column = 0; column < expected.vols_.Cols(); ++column) {
                    const double value = Value(calibrated.lVols_(row, column));
                    const double reference = expected.vols_(row, column);
                    if (!std::isfinite(value) || std::abs(value - reference) > 1e-12 + 1e-12 * std::abs(reference))
                        return false;
                }
            return true;
        }

        template <class C_> void ValidateReplay(const C_& calibrated, const LocalVolSurfaceData_& expected) {
            REQUIRE(calibrated.spots_ == expected.spots_ && calibrated.times_ == expected.times_,
                    "InvalidDupirePullback: replay changed the surface axes");
            REQUIRE(ReplayAgrees(calibrated, expected), "InvalidDupirePullback: active replay disagrees with the retained numeric surface");
        }

        Matrix_<> ExtractQuoteAdjoints(const AAD::RiskView_<AAD::Number_>& quotes) {
            Matrix_<> result(static_cast<int>(quotes.Rows()), static_cast<int>(quotes.Cols()));
            auto input = quotes.begin();
            for (int row = 0; row < result.Rows(); ++row)
                for (int column = 0; column < result.Cols(); ++column) {
                    const double risk = AAD::NativeOperations_::ReadAdjoint(*input++);
                    REQUIRE(std::isfinite(risk), "InvalidDupirePullback: non-finite calibration quote adjoint");
                    result(row, column) = risk;
                }
            return result;
        }

        Matrix_<> AddQuoteAdjoints(const Matrix_<>& calibration, const Matrix_<>& direct) {
            Matrix_<> total(calibration.Rows(), calibration.Cols());
            for (int row = 0; row < total.Rows(); ++row)
                for (int column = 0; column < total.Cols(); ++column) {
                    const double value = calibration(row, column) + direct(row, column);
                    REQUIRE(std::isfinite(value), "InvalidDupirePullback: non-finite total quote adjoint");
                    total(row, column) = value;
                }
            return total;
        }
    } // namespace

    struct DupireCalibrationSnapshot_::Data_ {
        DupireRiskInputs_ inputs_;
        FrozenBase_ base_;
        Handle_<LocalVolSurfaceData_> surface_;
        String_ algorithm_ = "DupireCentral1e-4FixedBaseBand/v1";
    };

    DupireCalibrationSnapshot_::DupireCalibrationSnapshot_(std::shared_ptr<const Data_> data) : data_(std::move(data)) {}
    const DupireRiskInputs_& DupireCalibrationSnapshot_::Inputs() const { return data_->inputs_; }
    const Handle_<LocalVolSurfaceData_>& DupireCalibrationSnapshot_::Surface() const { return data_->surface_; }
    double DupireCalibrationSnapshot_::Spot() const { return data_->base_.spot_; }
    double DupireCalibrationSnapshot_::Rate() const { return data_->base_.rate_; }
    double DupireCalibrationSnapshot_::DividendYield() const { return data_->base_.dividend_; }
    const String_& DupireCalibrationSnapshot_::Algorithm() const { return data_->algorithm_; }

    bool DupireCalibrationSnapshot_::Matches(const DupireCalibrationSnapshot_& other) const {
        if (data_ == other.data_)
            return true;
        return data_->algorithm_ == other.data_->algorithm_ && SameInputs(data_->inputs_, other.data_->inputs_) &&
               SameBase(data_->base_, other.data_->base_) && data_->surface_->spots_ == other.data_->surface_->spots_ &&
               data_->surface_->times_ == other.data_->surface_->times_ && SameMatrix(data_->surface_->vols_, other.data_->surface_->vols_);
    }

    bool DupireCalibrationSnapshot_::MatchesQuotes(const DupireCalibrationSnapshot_& other) const {
        return data_ == other.data_ || SameQuotes(data_->inputs_, other.data_->inputs_);
    }

    DupireCalibrationSnapshot_ CalibrateDupireWithRisk(const AAD::IVS_& baseIvs, const DupireRiskInputs_& inputs, const String_& name) {
        const auto copied = inputs;
        const auto copiedName = name;
        ValidateInputs(copied);
        const auto spots = AAD::FillData(copied.inclusionSpots_, copied.maxSpotSpacing_, 0.01);
        constexpr double ONE_HOUR_YF = 0.000114469;
        const auto times =
            AAD::FillData(copied.inclusionTimes_, copied.maxTimeSpacing_, ONE_HOUR_YF, &copied.maxTimeSpacing_, &copied.maxTimeSpacing_ + 1);
        ValidateAxis(spots, "completedSpots");
        ValidateAxis(times, "completedTimes");
        auto base = FreezeBase(baseIvs, spots, times);
        const auto quotes = NumericQuotes(copied);
        ValidateDomains(base, spots, times, quotes);
        const FrozenIVS_ frozen(base);
        const auto calibrated =
            AAD::DupireCalib(frozen, copied.inclusionSpots_, copied.maxSpotSpacing_, copied.inclusionTimes_, copied.maxTimeSpacing_, quotes);
        auto data = std::make_shared<DupireCalibrationSnapshot_::Data_>();
        data->inputs_ = copied;
        data->base_ = std::move(base);
        data->surface_ = Handle_<LocalVolSurfaceData_>(new LocalVolSurfaceData_(copiedName, calibrated.spots_, calibrated.times_, calibrated.lVols_));
        return DupireCalibrationSnapshot_(std::move(data));
    }

    DupireQuoteRisk_::DupireQuoteRisk_(const DupireCalibrationSnapshot_& calibration, Matrix_<>&& calibrated, Matrix_<>&& direct, Matrix_<>&& total)
        : calibration_(calibration), calibrationAdjoints_(std::move(calibrated)), directAdjoints_(std::move(direct)),
          totalAdjoints_(std::move(total)) {}

    DupireQuoteRisk_ PullbackDupireCalibration(const DupireCalibrationSnapshot_& snapshot,
                                               const DupireParameterAdjoints_& parameterAdjoints,
                                               const std::optional<DupireDirectQuoteAdjoints_>& directQuoteAdjoints) {
        auto direct = ValidatedDirectSeeds(snapshot, parameterAdjoints, directQuoteAdjoints);
        const auto mode = AAD::SetNumResultsForAAD(false);
        AAD::RecordingScope_ recording;
        const auto& inputs = snapshot.Inputs();
        const auto quotes = RegisterQuotes(inputs, &recording);
        recording.StartRecording();
        const FrozenIVS_ frozen(snapshot.data_->base_);
        auto calibrated =
            AAD::DupireCalib(frozen, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, quotes);
        if (!ReplayAgrees(calibrated, *snapshot.Surface())) {
            auto corrected = ReplayCalibration(snapshot.data_->base_, inputs, quotes);
            calibrated.spots_ = std::move(corrected.spots_);
            calibrated.times_ = std::move(corrected.times_);
            calibrated.lVols_ = std::move(corrected.lVols_);
        }
        recording.FinishRecording();
        ValidateReplay(calibrated, *snapshot.Surface());
        for (int row = 0; row < calibrated.lVols_.Rows(); ++row)
            for (int column = 0; column < calibrated.lVols_.Cols(); ++column)
                AAD::NativeOperations_::AddSeed(calibrated.lVols_(row, column), parameterAdjoints.adjoints_(row, column));
        recording.Reverse();
        auto calibration = ExtractQuoteAdjoints(quotes);
        auto total = AddQuoteAdjoints(calibration, direct);
        recording.Close();
        return DupireQuoteRisk_(snapshot, std::move(calibration), std::move(direct), std::move(total));
    }

    void ValidateDupireQuoteRecalibration(const DupireCalibrationSnapshot_& snapshot, const Matrix_<>& quoteSpreads) {
        auto inputs = snapshot.Inputs();
        inputs.quoteSpreads_ = quoteSpreads;
        ValidateInputs(inputs);
        const auto quotes = NumericQuotes(inputs);
        ValidateDomains(snapshot.data_->base_, snapshot.Surface()->spots_, snapshot.Surface()->times_, quotes);
    }

    DupireCalibrationSnapshot_ RecalibrateDupireWithRisk(const DupireCalibrationSnapshot_& snapshot, const Matrix_<>& quoteSpreads) {
        ValidateDupireQuoteRecalibration(snapshot, quoteSpreads);
        auto data = std::make_shared<DupireCalibrationSnapshot_::Data_>(*snapshot.data_);
        data->inputs_.quoteSpreads_ = quoteSpreads;
        const auto& inputs = data->inputs_;
        const FrozenIVS_ frozen(data->base_);
        const auto quotes = NumericQuotes(inputs);
        const auto calibrated =
            AAD::DupireCalib(frozen, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, quotes);
        data->surface_ = Handle_<LocalVolSurfaceData_>(
            new LocalVolSurfaceData_(snapshot.Surface()->Name(), calibrated.spots_, calibrated.times_, calibrated.lVols_));
        return DupireCalibrationSnapshot_(std::move(data));
    }
} // namespace Dal
