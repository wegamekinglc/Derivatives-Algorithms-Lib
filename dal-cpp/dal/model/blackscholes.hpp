//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

#include <dal/math/operators.hpp>
#include <dal/model/base.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>

/*IF--------------------------------------------------------------------------
storable BSModelData
    Black - Scholes model data
version 1
&members
name is ?string
spot is number
vol is number
rate is number
div is number
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        Vector_<String_> BlackScholesLabels() {
            Vector_<String_> labels(4);
            labels[0] = "spot";
            labels[1] = "vol";
            labels[2] = "rate";
            labels[3] = "div";
            return labels;
        }
    } // namespace

    namespace AAD {
        template <class T_ = double> class BlackScholes_ : public Model_<T_> {
            T_ spot_;
            T_ rate_;
            T_ div_;
            T_ vol_;

            Vector_<> timeLine_;
            bool todayOnTimeLine_ = false;
            const Vector_<SampleDef_>* defLine_ = nullptr;

            Vector_<T_> stds_;
            Vector_<T_> drifts_;
            Vector_<T_> numeraires_;

            Vector_<T_*> parameters_;
            Vector_<String_> parameterLabels_;

            void SetParamPointers() {
                parameters_[0] = &spot_;
                parameters_[1] = &vol_;
                parameters_[2] = &rate_;
                parameters_[3] = &div_;
            }

            template <bool VALIDATE_> bool FillScenario(const size_t& idx, const T_& spot, Sample_<T_>& scenario, const SampleDef_& def) const {
                if (def.numeraire_)
                    scenario.numeraire_ = numeraires_[idx];
                scenario.spot_ = spot;
                std::fill(scenario.observations_.begin(), scenario.observations_.end(), spot);
                if constexpr (VALIDATE_)
                    return std::isfinite(Value(spot)) & std::isfinite(Value(scenario.numeraire_)) & (Value(scenario.numeraire_) > 0.0);
                return true;
            }

            template <class F_>
            static bool GenerateSpots(const T_& spot,
                                      T_ logSpot,
                                      bool today,
                                      const Vector_<T_>& drifts,
                                      const Vector_<T_>& stds,
                                      const Vector_<>& gaussVec,
                                      const F_& fill) {
                bool valid = true;
                size_t idx = 0;
                if (today) {
                    valid &= fill(idx, spot);
                    ++idx;
                }
                for (size_t i = 0; i < drifts.size(); ++i) {
                    logSpot += drifts[i] + stds[i] * gaussVec[i];
                    valid &= fill(idx, Dal::exp(logSpot));
                    ++idx;
                }
                return valid;
            }

            template <bool VALIDATE_> bool GeneratePathImpl(const Vector_<>& gaussVec, Scenario_<T_>* path) const {
                return GenerateSpots(spot_, T_(Dal::log(spot_)), todayOnTimeLine_, drifts_, stds_, gaussVec,
                                     [&](size_t idx, const T_& spot) { return FillScenario<VALIDATE_>(idx, spot, (*path)[idx], (*defLine_)[idx]); });
            }

        public:
            class CheckedPaths_ {
                T_ spot_;
                T_ logSpot_;
                bool today_;
                Vector_<T_> drifts_;
                Vector_<T_> stds_;
                Scenario_<T_> path_;
                bool observations_ = false;
                bool numerairesValid_;

                template <bool OBSERVATIONS_> bool Generate(const Vector_<>& gaussVec) {
                    const bool valid = GenerateSpots(spot_, logSpot_, today_, drifts_, stds_, gaussVec, [&](size_t idx, const T_& spot) {
                        path_[idx].spot_ = spot;
                        if constexpr (OBSERVATIONS_)
                            std::fill(path_[idx].observations_.begin(), path_[idx].observations_.end(), spot);
                        return std::isfinite(Value(spot));
                    });
                    return valid & numerairesValid_;
                }

            public:
                explicit CheckedPaths_(const BlackScholes_& model)
                    : spot_(model.spot_), logSpot_(Dal::log(spot_)), today_(model.todayOnTimeLine_), drifts_(model.drifts_), stds_(model.stds_) {
                    static_assert(std::is_same_v<T_, double>, "Batch snapshots require double storage");
                    REQUIRE(typeid(model) == typeid(BlackScholes_), "CheckedPaths requires an exact BlackScholes model");
                    REQUIRE(model.defLine_ && model.defLine_->size() == drifts_.size() + static_cast<size_t>(today_),
                            "CheckedPaths requires consistent model allocation");
                    AllocatePath(*model.defLine_, path_);
                    InitializePath(path_);
                    for (size_t i = 0; i < path_.size(); ++i) {
                        model.template FillScenario<false>(i, T_(0.0), path_[i], (*model.defLine_)[i]);
                        observations_ |= !path_[i].observations_.empty();
                    }
                    numerairesValid_ = IsValidModelPath(path_);
                }

                bool Generate(const Vector_<>& gaussVec) { return observations_ ? Generate<true>(gaussVec) : Generate<false>(gaussVec); }

                [[nodiscard]] const Scenario_<T_>& Path() const { return path_; }
            };

            [[nodiscard]] bool SupportsIndex(const Index_& index) const override { return IsPlainEquity(index); }

            template <class U_>
            BlackScholes_(const U_& spot,
                          const U_& vol,
                          const U_& rate = U_(0.0),
                          const U_& div = U_(0.0))
                : spot_(spot), vol_(vol), rate_(rate), div_(div), parameters_(4), parameterLabels_(BlackScholesLabels()) {
                REQUIRE(std::isfinite(Value(spot_)) && Value(spot_) > 0.0, "InvalidModelParameter: spot must be finite and positive");
                REQUIRE(std::isfinite(Value(vol_)) && Value(vol_) >= 0.0, "InvalidModelParameter: vol must be finite and nonnegative");
                REQUIRE(std::isfinite(Value(rate_)), "InvalidModelParameter: rate must be finite");
                REQUIRE(std::isfinite(Value(div_)), "InvalidModelParameter: div must be finite");
                SetParamPointers();
            }

            const T_& Spot() const { return spot_; }

            const T_& Vol() const { return vol_; }

            const T_& Rate() const { return rate_; }

            const T_& Div() const { return div_; }

            const Vector_<T_*>& Parameters() const override { return parameters_; }

            const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }

            std::unique_ptr<Model_<T_>> Clone() const override {
                auto clone = std::make_unique<BlackScholes_<T_>>(*this);
                clone->SetParamPointers();
                return clone;
            }

            void Allocate(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                this->ValidateTimeline(productTimeLine, defLine);
                REQUIRE(!productTimeLine.empty(), "BlackScholes_::Allocate: empty product timeline");
                timeLine_.clear();
                timeLine_.push_back(0);

                for (const auto& time : productTimeLine) {
                    if (time > 0)
                        timeLine_.push_back(time);
                }

                todayOnTimeLine_ = productTimeLine[0] == 0;
                defLine_ = &defLine;

                stds_.Resize(timeLine_.size() - 1);
                drifts_.Resize(timeLine_.size() - 1);

                const size_t n = productTimeLine.size();
                numeraires_.Resize(n);
            }

            void Init(const Vector_<>& productTimeline, const Vector_<SampleDef_>& defLine) override {
                const T_ mu = rate_ - div_;
                const size_t n = timeLine_.size() - 1;

                for (size_t i = 0; i < n; ++i) {
                    const double dt = timeLine_[i + 1] - timeLine_[i];
                    stds_[i] = vol_ * Dal::sqrt(dt);

                    drifts_[i] = (mu - 0.5 * vol_ * vol_) * dt;
                    REQUIRE(std::isfinite(Value(stds_[i])) && std::isfinite(Value(drifts_[i])), "InvalidModelParameter: non-finite BS step");
                }

                const size_t m = productTimeline.size();
                for (size_t i = 0; i < m; ++i)
                    if (defLine[i].numeraire_) {
                        numeraires_[i] = Dal::exp(rate_ * productTimeline[i]);
                        REQUIRE(std::isfinite(Value(numeraires_[i])) && Value(numeraires_[i]) > 0.0,
                                "InvalidModelParameter: non-finite or zero BS numeraire");
                    }
            }

            [[nodiscard]] size_t SimDim() const override { return timeLine_.size() - 1; }

            void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const override {
                static_cast<void>(GeneratePathImpl<false>(gaussVec, path));
            }

            bool GeneratePathAndValidate(const Vector_<>& gaussVec, Scenario_<T_>* path) const {
                // Derived generators may write different outputs or resize the path.
                if (typeid(*this) != typeid(BlackScholes_) || path->size() != defLine_->size()) {
                    GeneratePath(gaussVec, path);
                    return IsValidModelPath(*path);
                }
                // Every observation is a copy of the emitted spot. Check it once,
                // together with the actual numeraire, while filling each sample.
                return GeneratePathImpl<true>(gaussVec, path);
            }
        };
    } // namespace AAD

    struct BSModelData_: ModelData_ {
        double spot_;
        double vol_;
        double rate_;
        double div_;

        BSModelData_(const String_& name,
                     double spot,
                     double vol,
                     double rate = 0.0,
                     double div = 0.0)
                     : ModelData_("BSModelData_", name), spot_(spot), vol_(vol), rate_(rate), div_(div) {
            parameterLabels_ = BlackScholesLabels();
        }

        void Write(Archive::Store_& dst) const override;

    private:
        std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };

} // namespace Dal
