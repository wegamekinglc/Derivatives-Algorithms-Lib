//
// Created by wegam on 2022/5/2.
//

#pragma once

#include <cmath>
#include <dal/storage/archive.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/aad/models/ivs.hpp>
#include <dal/math/aad/models/utilities.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/math/aad/operators.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/vectors.hpp>
#include <iomanip>
#include <sstream>

#define HALF_DAY 0.00136986301369863

/*IF--------------------------------------------------------------------------
storable DupireModelData
    Dupire local volatility model data
version 1
&members
name is ?string
spot is number
spots is number[]
times is number[]
vols is number[][]
-IF-------------------------------------------------------------------------*/

namespace Dal::AAD {
    template <class T_ = double> class Dupire_ : public Model_<T_> {
        T_ spot_;
        const Vector_<SampleDef_>* defLine_;
        const Vector_<> spots_;
        Vector_<> logSpots_;
        const Vector_<> times_;
        Matrix_<T_> vols_;
        const double maxDt_;
        Vector_<> timeLine_;
        Vector_<bool> commonSteps_;

        Matrix_<T_> interpVols_;
        Vector_<T_*> parameters_;
        Vector_<String_> parameterLabels_;

    public:
        template<class U_>
        Dupire_(const U_& spot,
                const Vector_<>& spots,
                const Vector_<>& times,
                const Matrix_<U_>& vols,
                double maxDt = 1.0)
            : spot_(spot), spots_(spots), logSpots_(spots.size()), times_(times), vols_(vols), maxDt_(maxDt),
              parameters_(vols.Rows() * vols.Cols() + 1), parameterLabels_(vols.Rows() * vols.Cols() + 1) {
            Transform(spots_, [](double x) { return Log(x); }, &logSpots_);
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

        [[nodiscard]] const Vector_<>& Spots() const { return spots_; }

        [[nodiscard]] const Vector_<>& Times() const { return times_; }

        const Vector_<T_>& Vols() const { return vols_; }

        [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }

        [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }

        std::unique_ptr<Model_<T_>> Clone() const override {
            auto cloned = std::make_unique<Dupire_<T_>>(*this);
            cloned->SetParameterPointers();
            return cloned;
        }

        //  Initialize timeline
        void Allocate(const Vector_<>& productTimeline, const Vector_<SampleDef_>& defLine) override {
            Vector_<> added(1, 0); // just to add 0
            timeLine_ = FillData(productTimeline, maxDt_, HALF_DAY, added.begin(), added.end());
            commonSteps_.Resize(timeLine_.size());
            Transform(&commonSteps_, timeLine_,
                      [&](double t) { return std::binary_search(productTimeline.begin(), productTimeline.end(), t); });
            defLine_ = &defLine;
            interpVols_.Resize(timeLine_.size() - 1, spots_.size());
        }

        void Init(const Vector_<>& productTimeline, const Vector_<SampleDef_>& defLine) override {
            const size_t n = timeLine_.size() - 1;
            const size_t m = logSpots_.size();
            for (size_t i = 0; i < n; ++i) {
                const double sqrtDt = Sqrt(timeLine_[i + 1] - timeLine_[i]);
                for (size_t j = 0; j < m; ++j) {
                    interpVols_(i, j) = sqrtDt * InterpLinearImplX<T_>(times_, vols_.Row(j), T_(timeLine_[i]));
                }
            }
        }

        [[nodiscard]] size_t SimDim() const override { return timeLine_.size() - 1; }

        void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const override {
            T_ logSpot = Log(spot_);
            size_t idx = 0;
            if (commonSteps_[idx]) {
                FillScenario(Exp(logSpot), (*path)[idx]);
                ++idx;
            }

            //  Iterate through timeline
            const size_t n = timeLine_.size() - 1;
            for (size_t i = 0; i < n; ++i) {
                T_ vol = InterpLinearImplX<T_>(logSpots_, interpVols_.Row(i), logSpot);
                logSpot += vol * (-0.5 * vol + gaussVec[i]);
                if (commonSteps_[i + 1]) {
                    FillScenario(Exp(logSpot), (*path)[idx]);
                    ++idx;
                }
            }
        }

    private:
        void SetParameterPointers() {
            parameters_[0] = &spot_;
            int k = 0;
            for(size_t i = 0; i < vols_.Rows(); ++i) {
                for (size_t j = 0; j < vols_.Cols(); ++j) {
                    ++k;
                    parameters_[k] = &vols_(i, j);
                }
            }
        }

        //  Helper function, fills a sample given the spot
        inline static void FillScenario(const T_& spot, Sample_<T_>& scenario) {
            scenario.spot_ = spot;
            std::fill(scenario.forwards_.front().begin(), scenario.forwards_.front().end(), spot);
        }
    };

    template <class IT_, class OT_, class T_ = double>
    void DupireCalibMaturity(const IVS_& ivs,
                             double maturity,
                             IT_ spotsBegin,
                             IT_ spotsEnd,
                             OT_ lVolsBegin,
                             const RiskView_<T_>& riskView = RiskView_<double>()) {
        // Number of spots
        IT_ spots = spotsBegin;
        const size_t nSpots = distance(spotsBegin, spotsEnd);

        // Estimate ATM, and we cut the grid 2 stdevs away to avoid instabilities
        const double atmCall = double(ivs.Call(ivs.Spot(), maturity));
        // Standard deviation, approx. atm call * sqrt(2pi)
        const double std = atmCall * 2.506628274631;

        //  Skip spots below and above 2.5 std
        int il = 0;
        while (il < nSpots && spots[il] < ivs.Spot() - 2.5 * std)
            ++il;
        int ih = nSpots - 1;
        while (ih >= 0 && spots[ih] > ivs.Spot() + 2.5 * std)
            --ih;

        //  Loop on spots
        for (int i = il; i <= ih; ++i) {
            //  Dupire's formula
            lVolsBegin[i] = ivs.LocalVol(spots[i], maturity, &riskView);
        }

        //  Extrapolate flat outside std
        for (int i = 0; i < il; ++i)
            lVolsBegin[i] = lVolsBegin[il];
        for (int i = ih + 1; i < nSpots; ++i)
            lVolsBegin[i] = lVolsBegin[ih];
    }

    // Returns a struct with spots, times and lVols
    template <class T_ = double>
    inline auto DupireCalib(const IVS_& ivs, const Vector_<>& inclSpots, double maxDs, const Vector_<>& inclTimes, double maxDt, const RiskView_<T_>& riskView = RiskView_<double>()) {
        struct {
            Vector_<> spots_;
            Vector_<> times_;
            Matrix_<T_> lVols_;
        } results;

        static const double ONE_HOUR = 0.000114469;
        results.spots_ = FillData(inclSpots, maxDs, 0.01);
        results.times_ = FillData(inclTimes, maxDt, ONE_HOUR,&maxDt, &maxDt + 1);

        Matrix_<T_> lVolsT(results.times_.size(), results.spots_.size());

        const size_t n = results.times_.size();
        for (size_t j = 0; j < n; ++j) {
            DupireCalibMaturity(ivs, results.times_[j], results.spots_.begin(), results.spots_.end(), lVolsT[j], riskView);
        }

        results.lVols_ = Dal::Matrix::MakeTranspose(lVolsT);
        return results;
    }

    struct DupireModelData_: public ModelData_ {
        double spot_;
        Vector_<> spots_;
        Vector_<> times_;
        Matrix_<> vols_;

        DupireModelData_(const String_& name,
                         double spot,
                         const Vector_<>& spots,
                         const Vector_<>& times,
                         const Matrix_<> vols)
                : ModelData_("BSModelData_", name), spot_(spot), spots_(spots), times_(times), vols_(vols) {}

        void Write(Archive::Store_& dst) const override;
    };
} // namespace Dal::AAD
