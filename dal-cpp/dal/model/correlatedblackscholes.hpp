//
// Created by Codex on 2026/9/27.
//

#pragma once

#include <cmath>
#include <limits>
#include <memory>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/operators.hpp>
#include <dal/model/base.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable CorrelatedBSModelData
    Correlated multi-equity Black-Scholes model data
version 1
&members
name is ?string
indices is string[]
spots is number[]
vols is number[]
divs is number[]
rate is number
correlations is number[][]
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct CorrelatedBSAsset_ {
        String_ index_;
        double spot_;
        double vol_;
        double div_;
    };

    struct CorrelatedBSSettings_ {
        Vector_<CorrelatedBSAsset_> assets_;
        double rate_ = 0.0;
        Matrix_<> correlations_;
    };

    namespace AAD {
        template <class T_ = double> class CorrelatedBlackScholes_ : public Model_<T_> {
            Vector_<String_> assetNames_;
            Vector_<T_> spots_;
            Vector_<T_> vols_;
            Vector_<T_> divs_;
            T_ rate_;
            Matrix_<> lower_;
            Vector_<> timeLine_;
            bool todayOnTimeLine_ = false;
            const Vector_<SampleDef_>* defLine_ = nullptr;
            Vector_<Vector_<size_t>> observationSlots_;
            Vector_<T_> drifts_;
            Vector_<T_> stds_;
            Vector_<T_> initialLogSpots_;
            Vector_<T_> numeraires_;
            Vector_<T_*> parameters_;
            Vector_<String_> parameterLabels_;

            [[nodiscard]] size_t AssetSlot(const String_& indexName) const {
                const Handle_<Index_> index(Index::Parse(indexName));
                REQUIRE(index, "UnsupportedModelObservation: " + indexName);
                for (size_t i = 0; i < assetNames_.size(); ++i)
                    if (assetNames_[i] == index->Name())
                        return i;
                THROW("UnsupportedModelObservation: " + indexName);
            }

            void ValidateParameters() const {
                REQUIRE(!assetNames_.empty(), "InvalidModelParameter: correlated BS requires at least one asset");
                REQUIRE(spots_.size() == assetNames_.size() && vols_.size() == assetNames_.size() && divs_.size() == assetNames_.size(),
                        "InvalidModelParameter: correlated BS asset parameter sizes differ");
                REQUIRE(std::isfinite(Value(rate_)), "InvalidModelParameter: rate must be finite");
                for (size_t i = 0; i < assetNames_.size(); ++i) {
                    REQUIRE(std::isfinite(Value(spots_[i])) && Value(spots_[i]) > 0.0,
                            "InvalidModelParameter: spot must be finite and positive for " + assetNames_[i]);
                    REQUIRE(std::isfinite(Value(vols_[i])) && Value(vols_[i]) >= 0.0,
                            "InvalidModelParameter: vol must be finite and nonnegative for " + assetNames_[i]);
                    REQUIRE(std::isfinite(Value(divs_[i])), "InvalidModelParameter: div must be finite for " + assetNames_[i]);
                }
            }

            void SetParamPointers() {
                parameters_.clear();
                parameterLabels_.clear();
                parameters_.reserve(3 * assetNames_.size() + 1);
                parameterLabels_.reserve(3 * assetNames_.size() + 1);
                for (size_t i = 0; i < assetNames_.size(); ++i) {
                    parameters_.push_back(&spots_[i]);
                    parameters_.push_back(&vols_[i]);
                    parameters_.push_back(&divs_[i]);
                    parameterLabels_.push_back("spot:" + assetNames_[i]);
                    parameterLabels_.push_back("vol:" + assetNames_[i]);
                    parameterLabels_.push_back("div:" + assetNames_[i]);
                }
                parameters_.push_back(&rate_);
                parameterLabels_.push_back("rate");
            }

            void ValidateAssetNames() {
                for (size_t i = 0; i < assetNames_.size(); ++i) {
                    const Handle_<Index_> index(Index::Parse(assetNames_[i]));
                    REQUIRE(index && IsPlainEquity(*index), "InvalidModelIndex: expected an ordinary EQ index: " + assetNames_[i]);
                    assetNames_[i] = index->Name();
                    for (size_t j = 0; j < i; ++j)
                        REQUIRE(assetNames_[i] != assetNames_[j], "DuplicateModelIndex: " + assetNames_[i]);
                }
            }

            void ValidateCorrelationMatrix(const Matrix_<>& correlations) const {
                const int n = static_cast<int>(assetNames_.size());
                REQUIRE(correlations.Rows() == n && correlations.Cols() == n, "InvalidCorrelation: dimensions must match the number of assets");
                for (int i = 0; i < n; ++i) {
                    for (int j = 0; j < n; ++j) {
                        REQUIRE(std::isfinite(correlations(i, j)), "InvalidCorrelation: non-finite entry");
                        REQUIRE(std::abs(correlations(i, j) - correlations(j, i)) <= 1e-12, "InvalidCorrelation: matrix must be symmetric");
                    }
                    REQUIRE(std::abs(correlations(i, i) - 1.0) <= 1e-12, "InvalidCorrelation: diagonal entries must equal one");
                }
            }

            void FactorCorrelation(const Matrix_<>& correlations) {
                const int n = static_cast<int>(assetNames_.size());
                lower_ = Matrix_<>(n, n, 0.0);
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j <= i; ++j) {
                        double sum = correlations(i, j);
                        for (int k = 0; k < j; ++k)
                            sum -= lower_(i, k) * lower_(j, k);
                        if (i == j) {
                            REQUIRE(std::isfinite(sum) && sum > 1e-14, "InvalidCorrelation: matrix must be positive definite");
                            lower_(i, j) = std::sqrt(sum);
                        } else
                            lower_(i, j) = sum / lower_(j, j);
                    }
            }

            void InitializeStepConstants() {
                const size_t n = assetNames_.size();
                for (size_t step = 0; step + 1 < timeLine_.size(); ++step) {
                    const double dt = timeLine_[step + 1] - timeLine_[step];
                    for (size_t asset = 0; asset < n; ++asset) {
                        const size_t id = step * n + asset;
                        drifts_[id] = (rate_ - divs_[asset] - 0.5 * vols_[asset] * vols_[asset]) * dt;
                        stds_[id] = vols_[asset] * Dal::sqrt(dt);
                        REQUIRE(std::isfinite(Value(drifts_[id])) && std::isfinite(Value(stds_[id])),
                                "InvalidModelParameter: non-finite correlated BS step");
                    }
                }
            }

            void InitializeNumeraires(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) {
                for (size_t sample = 0; sample < productTimeLine.size(); ++sample)
                    if (defLine[sample].numeraire_) {
                        numeraires_[sample] = Dal::exp(rate_ * productTimeLine[sample]);
                        REQUIRE(std::isfinite(Value(numeraires_[sample])) && Value(numeraires_[sample]) > 0.0,
                                "InvalidModelParameter: non-finite or zero correlated BS numeraire");
                    }
            }

            void ResetLogSpots(Vector_<T_>* logSpots) const {
                if (logSpots->size() != assetNames_.size())
                    logSpots->Resize(assetNames_.size());
                for (size_t asset = 0; asset < assetNames_.size(); ++asset)
                    (*logSpots)[asset] = initialLogSpots_[asset];
            }

            void AdvanceStep(size_t step, const Vector_<>& gaussVec, Vector_<T_>* logSpots) const {
                const size_t n = assetNames_.size();
                for (size_t asset = 0; asset < n; ++asset) {
                    double correlated = 0.0;
                    for (size_t factor = 0; factor <= asset; ++factor)
                        correlated += lower_(static_cast<int>(asset), static_cast<int>(factor)) * gaussVec[step * n + factor];
                    const size_t id = step * n + asset;
                    (*logSpots)[asset] += drifts_[id] + stds_[id] * correlated;
                }
            }

            void FillSample(size_t sampleId, const Vector_<T_>& logSpots, bool isToday, Sample_<T_>* sample) const {
                if ((*defLine_)[sampleId].numeraire_)
                    sample->numeraire_ = numeraires_[sampleId];
                if (isToday)
                    sample->spot_ = spots_[0];
                else
                    sample->spot_ = Dal::exp(logSpots[0]);
                const auto& slots = observationSlots_[sampleId];
                for (size_t output = 0; output < slots.size(); ++output) {
                    const size_t asset = slots[output];
                    if (asset == 0)
                        sample->observations_[output] = sample->spot_;
                    else if (isToday)
                        sample->observations_[output] = spots_[asset];
                    else
                        sample->observations_[output] = Dal::exp(logSpots[asset]);
                }
            }

        public:
            CorrelatedBlackScholes_(
                Vector_<String_> assetNames, Vector_<T_> spots, Vector_<T_> vols, Vector_<T_> divs, T_ rate, const Matrix_<>& correlations)
                : assetNames_(std::move(assetNames)), spots_(std::move(spots)), vols_(std::move(vols)), divs_(std::move(divs)),
                  rate_(std::move(rate)) {
                ValidateParameters();
                ValidateAssetNames();
                ValidateCorrelationMatrix(correlations);
                FactorCorrelation(correlations);
                SetParamPointers();
            }

            [[nodiscard]] bool SupportsIndex(const Index_& index) const override {
                if (!IsPlainEquity(index))
                    return false;
                for (const auto& name : assetNames_)
                    if (name == index.Name())
                        return true;
                return false;
            }

            [[nodiscard]] size_t MaxObservedIndices() const override { return assetNames_.size(); }
            [[nodiscard]] size_t MaxOutputSlotsPerSample() const override { return std::numeric_limits<size_t>::max(); }
            [[nodiscard]] size_t NumFactors() const override { return assetNames_.size(); }
            [[nodiscard]] size_t NumAssets() const override { return assetNames_.size(); }
            [[nodiscard]] const Vector_<String_>& AssetNames() const override { return assetNames_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }

            [[nodiscard]] std::unique_ptr<Model_<T_>> Clone() const override {
                auto copy = std::make_unique<CorrelatedBlackScholes_<T_>>(*this);
                copy->SetParamPointers();
                return copy;
            }

            void Allocate(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                this->ValidateTimeline(productTimeLine, defLine);
                defLine_ = &defLine;
                timeLine_.clear();
                timeLine_.push_back(0.0);
                for (const double time : productTimeLine)
                    if (time > 0.0)
                        timeLine_.push_back(time);
                todayOnTimeLine_ = productTimeLine[0] == 0.0;
                observationSlots_.Resize(defLine.size());
                for (size_t sample = 0; sample < defLine.size(); ++sample) {
                    observationSlots_[sample].clear();
                    for (const auto& name : defLine[sample].indexNames_)
                        observationSlots_[sample].push_back(AssetSlot(name));
                }
                const size_t steps = timeLine_.size() - 1;
                drifts_.Resize(steps * assetNames_.size());
                stds_.Resize(steps * assetNames_.size());
                initialLogSpots_.Resize(assetNames_.size());
                numeraires_.Resize(productTimeLine.size());
            }

            void Init(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                ValidateParameters();
                REQUIRE(defLine_ == &defLine && defLine.size() == productTimeLine.size(), "InvalidModelTimeline: call Allocate before Init");
                for (size_t asset = 0; asset < assetNames_.size(); ++asset)
                    initialLogSpots_[asset] = Dal::log(spots_[asset]);
                InitializeStepConstants();
                InitializeNumeraires(productTimeLine, defLine);
            }

            [[nodiscard]] size_t SimDim() const override { return (timeLine_.size() - 1) * assetNames_.size(); }

            void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const override {
                REQUIRE(defLine_ && path && path->size() == defLine_->size() && gaussVec.size() == SimDim(),
                        "InvalidModelPath: correlated BS path or Gaussian dimension mismatch");
                auto* logSpots = &(*path)[0].modelScratch_;
                ResetLogSpots(logSpots);
                size_t sample = 0;
                if (todayOnTimeLine_)
                    FillSample(sample++, *logSpots, true, &(*path)[0]);
                for (size_t step = 0; step + 1 < timeLine_.size(); ++step, ++sample) {
                    AdvanceStep(step, gaussVec, logSpots);
                    FillSample(sample, *logSpots, false, &(*path)[sample]);
                }
            }
        };
    } // namespace AAD

    struct CorrelatedBSModelData_ : ModelData_ {
        Vector_<String_> indices_;
        Vector_<> spots_;
        Vector_<> vols_;
        Vector_<> divs_;
        double rate_;
        Matrix_<> correlations_;

        CorrelatedBSModelData_(const String_& name, const CorrelatedBSSettings_& settings)
            : ModelData_("CorrelatedBSModelData_", name), rate_(settings.rate_), correlations_(settings.correlations_) {
            for (const auto& asset : settings.assets_) {
                indices_.push_back(asset.index_);
                spots_.push_back(asset.spot_);
                vols_.push_back(asset.vol_);
                divs_.push_back(asset.div_);
            }
            SetParameterLabels();
        }

        CorrelatedBSModelData_(const String_& name,
                               const Vector_<String_>& indices,
                               const Vector_<>& spots,
                               const Vector_<>& vols,
                               const Vector_<>& divs,
                               double rate,
                               const Matrix_<>& correlations)
            : ModelData_("CorrelatedBSModelData_", name), indices_(indices), spots_(spots), vols_(vols), divs_(divs), rate_(rate),
              correlations_(correlations) {
            SetParameterLabels();
        }

        void Write(Archive::Store_& dst) const override;

    private:
        void SetParameterLabels() {
            parameterLabels_.clear();
            for (const auto& index : indices_) {
                parameterLabels_.push_back("spot:" + index);
                parameterLabels_.push_back("vol:" + index);
                parameterLabels_.push_back("div:" + index);
            }
            parameterLabels_.push_back("rate");
        }

        std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };
} // namespace Dal
