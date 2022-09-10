//
// Created by wegam on 2022/5/2.
//

#pragma once

#include "dal/math/aad/operators.hpp"
#include <cmath>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <iomanip>
#include <sstream>

#define HALF_DAY 0.00136986301369863

namespace Dal::AAD {
    template <class T_> class Dupire_ : public Model_<T_> {
        T_ spot_;
        const Vector_<> spots_;
        Vector_<> logSpots_;
        const Vector_<Time_> times_;
        Matrix_<T_> vols_;
        const Time_ maxDt_;
        Vector_<Time_> timeLine_;
        Vector_<bool> commonSteps_;
        const Vector_<SampleDef_>* defLine_;

        Matrix_<T_> interpVols_;
        Vector_<T_*> parameters_;
        Vector_<String_> parameterLabels_;

    public:
        Dupire_(const T_& spot,
                const Vector_<>& spots,
                const Vector_<Time_>& times,
                const Matrix_<T_>& vols,
                Time_ maxDt = 0.25)
            : spot_(spot), spots_(spots), logSpots_(spots.size()), times_(times), vols_(vols), maxDt_(maxDt),
              parameters_(vols.Rows() * vols.Cols() + 1), parameterLabels_(vols.Rows() * vols.Cols() + 1) {
            Transform(&logSpots_, spots_, [](double x) { return std::log(x); });
            parameterLabels_[0] = "spot";
            size_t p = 0;
            for (size_t i = 0; i < vols_.Rows(); ++i)
                for (size_t j = 0; j < vols_.Cols(); ++j) {
                    std::ostringstream ost;
                    ost << std::setprecision(2) << std::fixed;
                    ost << "lvol " << spots_[i] << " " << times_[j];
                    parameterLabels_[++p] = String_(ost.str());
                }

            SetParameterPointers();
        }

        T_ Spot() const { return spot_; }

        const Vector_<>& Spots() const { return spots_; }

        const Vector_<Time_>& Times() const { return times_; }

        const Vector_<T_>& Vols() const { return vols_; }

        const Vector_<T_*> Parameters() const { return parameters_; }

        const Vector_<String_> ParameterLabels() const { return parameterLabels_; }

        std::unique_ptr<Model_<T_>> Clone() const override {
            auto cloned = std::make_unique<Dupire_<T_>>(*this);
            cloned->SetParamPointers();
            return cloned;
        }

        //  Initialize timeline
        void Allocate(const Vector_<Time_>& productTimeline, const Vector_<SampleDef_>& defLine) override {
            //  Fill from product timeline

            //  Do the fill
            timeLine_ = FillData(productTimeline, maxDt_, HALF_DAY, &systemTime, &systemTime + 1);

            //  Mark steps on timeline that are on the product timeline
            commonSteps_.Resize(timeLine_.size());
            Transform(&commonSteps_, timeLine_,
                      [&](Time_ t) { return std::binary_search(productTimeline.begin(), productTimeline.end(), t); });

            //  Take a reference on the product's defline
            defLine_ = &defLine;

            // Allocate the local volatility
            // pre-interpolated in time over simulation timeline
            interpVols_.resize(timeLine_.size() - 1, spots_.size());
        }

        void init(const Vector_<Time_>& productTimeline, const Vector_<SampleDef_>& defLine) override {
            // Compute the local volatility
            // pre-interpolated in time and multiplied by sqrt(dt)
            const size_t n = timeLine_.size() - 1;
            for (size_t i = 0; i < n; ++i) {
                const double sqrtDt = std::sqrt(timeLine_[i + 1] - timeLine_[i]);
                const size_t m = logSpots_.size();
                for (size_t j = 0; j < m; ++j)
                    interpVols_[i][j] =
                        sqrtDt * interp(times_.begin(), times_.end(), vols_[j], vols_[j] + times_.size(), timeLine_[i]);
            }
        }

        size_t SimDim() const override { return timeLine_.size() - 1; }

        void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>& path) const override {
            //  The starting spot
            //  We know that today is on the timeline
            T_ logSpot = Log(spot_);
            Time_ current = systemTime;
            //  Next index to fill on the product timeline
            size_t idx = 0;
            //  Is today on the product timeline?
            if (commonSteps_[idx]) {
                fillScen(Exp(logSpot), path[idx]);
                ++idx;
            }

            //  Iterate through timeline
            const size_t n = timeLine_.size() - 1;
            const size_t m = logSpots_.size();
            for (size_t i = 0; i < n; ++i) {
                //  Interpolate volatility in spot
                T_ vol = interp(logSpots_.begin(), logSpots_.end(), interpVols_[i], interpVols_[i] + m, logSpot);
                //  vol comes out * sqrt(dt)

                //  Apply Euler's scheme
                logSpot += vol * (-0.5 * vol + gaussVec[i]);

                //  Store on the path?
                if (commonSteps_[i + 1]) {
                    fillScen(Exp(logSpot), path[idx]);
                    ++idx;
                }
            }
        }

    private:
        void SetParameterPointers() {
            parameters_[0] = &spot_;
            transform(vols_.begin(), vols_.end(), next(parameters_.begin()), [](auto& vol) { return &vol; });
        }

        //  Helper function, fills a sample given the spot
        inline static void FillScen(const T_& spot, Sample_<T_>& scenario) {
            std::fill(scenario.forwards_.front().begin(), scenario.forwards_.front().end(), spot);
        }
    };
} // namespace Dal
