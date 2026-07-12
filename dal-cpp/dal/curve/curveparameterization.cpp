//
// Created by dal-implementer on 2026/7/12.
//

#include <algorithm>

#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    CurveDefinition_ MakeCurveDefinition(const String_& name,
                                         const String_& ccy,
                                         CurveParameterization_ parameterization,
                                         LogDfScheme_ logDfScheme,
                                         const Vector_<Date_>& declaredKnots,
                                         const Date_& anchor,
                                         const DayBasis_& dayCount) {
        REQUIRE(!declaredKnots.empty(), "MakeCurveDefinition: declared knot dates must not be empty");
        REQUIRE(IsMonotonic(declaredKnots), "MakeCurveDefinition: declared knot dates must be strictly increasing");

        Vector_<Date_> nodeDates;
        switch (parameterization.Switch()) {
        case CurveParameterization_::Value_::LOG_DISCOUNT:
            REQUIRE(declaredKnots.front() >= anchor, "MakeCurveDefinition: log-DF knots must not precede the anchor");
            if (declaredKnots.front() != anchor)
                nodeDates.push_back(anchor);
            nodeDates.Append(declaredKnots);
            break;
        case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
        case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
            REQUIRE(declaredKnots.front() > anchor, "MakeCurveDefinition: forward knots must be after the anchor");
            nodeDates = declaredKnots;
            break;
        case CurveParameterization_::Value_::ZERO_RATE:
            REQUIRE(declaredKnots.front() > anchor, "MakeCurveDefinition: zero-rate knots must be after the anchor");
            nodeDates.push_back(anchor);
            nodeDates.Append(declaredKnots);
            break;
        default:
            REQUIRE(false, "Unknown curve parameterization");
        }

        return {name, ccy, parameterization, logDfScheme, nodeDates, dayCount, anchor};
    }

    CurveParameterLayout_ BuildCurveParameterLayout(const CurveDefinition_& definition) {
        const int storageNodes = static_cast<int>(definition.nodeDates_.size());
        switch (definition.parameterization_.Switch()) {
        case CurveParameterization_::Value_::LOG_DISCOUNT:
            return {storageNodes, storageNodes - 1, 1, true};
        case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
            return {storageNodes, storageNodes, 1, false};
        case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD:
            return {storageNodes, 2 * storageNodes, 2, false};
        case CurveParameterization_::Value_::ZERO_RATE:
            REQUIRE(storageNodes >= 2, "BuildCurveParameterLayout: zero-rate definition needs an anchor and at least one future node");
            REQUIRE(definition.nodeDates_.front() == definition.anchorDate_,
                    "BuildCurveParameterLayout: zero-rate definition must begin at its anchor");
            REQUIRE(IsMonotonic(definition.nodeDates_), "BuildCurveParameterLayout: zero-rate definition dates must be strictly increasing");
            REQUIRE(definition.nodeDates_[1] > definition.anchorDate_,
                    "BuildCurveParameterLayout: zero-rate definition nodes must be strictly after the anchor");
            return {storageNodes, storageNodes - 1, 1, true};
        default:
            REQUIRE(false, "Unknown curve parameterization");
            return {};
        }
    }

    Vector_<AAD::Number_> RegisterCurveParameters(const Vector_<>& parameters) {
        Vector_<AAD::Number_> result(parameters.size());
        for (int i = 0; i < static_cast<int>(parameters.size()); ++i)
            AAD::RegisterIndependent(result[i], parameters[i]);
        return result;
    }

    template <class T_, class B_>
    std::unique_ptr<Tape::DiscountCurve_<T_>>
    BuildDiscountCurveUniqueT(const CurveDefinition_& definition, const Vector_<T_>& parameters, const Handle_<B_>& base) {
        const CurveParameterLayout_ layout = BuildCurveParameterLayout(definition);
        REQUIRE(static_cast<int>(parameters.size()) == layout.parameterCount_,
                "BuildDiscountCurveT: parameter count does not match the curve layout");

        switch (definition.parameterization_.Switch()) {
        case CurveParameterization_::Value_::LOG_DISCOUNT: {
            Vector_<T_> logDf(definition.nodeDates_.size());
            logDf[0] = T_(0.0);
            std::copy(parameters.begin(), parameters.end(), logDf.begin() + 1);
            return std::make_unique<Tape::DiscountLogDF_<T_, B_>>(definition.name_, definition.ccy_, definition.nodeDates_, logDf,
                                                                  definition.dayCount_, definition.logDfScheme_, base);
        }
        case CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD:
            return std::make_unique<Tape::DiscountPWC_<T_, B_>>(definition.name_, definition.ccy_, definition.nodeDates_, parameters, base);
        case CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD: {
            Vector_<T_> left(definition.nodeDates_.size());
            Vector_<T_> right(definition.nodeDates_.size());
            for (int i = 0; i < static_cast<int>(definition.nodeDates_.size()); ++i) {
                left[i] = parameters[2 * i];
                right[i] = parameters[2 * i + 1];
            }
            return std::make_unique<Tape::DiscountPWLF_<T_, B_>>(definition.name_, definition.ccy_, definition.nodeDates_, left, right, base);
        }
        case CurveParameterization_::Value_::ZERO_RATE: {
            return std::make_unique<Tape::DiscountZeroRate_<T_, B_>>(definition.name_, definition.ccy_, definition.anchorDate_,
                                                                     Vector_<Date_>(definition.nodeDates_.begin() + 1, definition.nodeDates_.end()),
                                                                     parameters, definition.dayCount_, definition.logDfScheme_, base);
        }
        default:
            REQUIRE(false, "Unknown curve parameterization");
            return nullptr;
        }
    }

    template <class T_, class B_>
    std::shared_ptr<Tape::DiscountCurve_<T_>>
    BuildDiscountCurveT(const CurveDefinition_& definition, const Vector_<T_>& parameters, const Handle_<B_>& base) {
        return std::shared_ptr<Tape::DiscountCurve_<T_>>(BuildDiscountCurveUniqueT<T_, B_>(definition, parameters, base).release());
    }

    template std::unique_ptr<Tape::DiscountCurve_<double>> BuildDiscountCurveUniqueT<double, Tape::DiscountCurve_<double>>(
        const CurveDefinition_&, const Vector_<double>&, const Handle_<Tape::DiscountCurve_<double>>&);
    template std::unique_ptr<Tape::DiscountCurve_<AAD::Number_>> BuildDiscountCurveUniqueT<AAD::Number_, Tape::DiscountCurve_<double>>(
        const CurveDefinition_&, const Vector_<AAD::Number_>&, const Handle_<Tape::DiscountCurve_<double>>&);
    template std::unique_ptr<Tape::DiscountCurve_<AAD::Number_>> BuildDiscountCurveUniqueT<AAD::Number_, Tape::DiscountCurve_<AAD::Number_>>(
        const CurveDefinition_&, const Vector_<AAD::Number_>&, const Handle_<Tape::DiscountCurve_<AAD::Number_>>&);

    template std::shared_ptr<Tape::DiscountCurve_<double>> BuildDiscountCurveT<double, Tape::DiscountCurve_<double>>(
        const CurveDefinition_&, const Vector_<double>&, const Handle_<Tape::DiscountCurve_<double>>&);
    template std::shared_ptr<Tape::DiscountCurve_<AAD::Number_>> BuildDiscountCurveT<AAD::Number_, Tape::DiscountCurve_<double>>(
        const CurveDefinition_&, const Vector_<AAD::Number_>&, const Handle_<Tape::DiscountCurve_<double>>&);
    template std::shared_ptr<Tape::DiscountCurve_<AAD::Number_>> BuildDiscountCurveT<AAD::Number_, Tape::DiscountCurve_<AAD::Number_>>(
        const CurveDefinition_&, const Vector_<AAD::Number_>&, const Handle_<Tape::DiscountCurve_<AAD::Number_>>&);
} // namespace Dal
