//
// Created by Codex on 2026/10/2.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>

#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/quoteriskprovenance_internal.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/model/gsrcurverisk.hpp>

namespace Dal {
    namespace {
        Vector_<> Flatten(const GSRCurveData_& snapshot) {
            auto result = snapshot.discountLogDF_;
            for (int row = 0; row < snapshot.projectionLogDF_.Rows(); ++row)
                for (int col = 0; col < snapshot.projectionLogDF_.Cols(); ++col)
                    result.push_back(snapshot.projectionLogDF_(row, col));
            return result;
        }

        Vector_<> Sample(const GSRCurveData_& snapshot, const Vector_<Handle_<DiscountCurve_>>& curves) {
            Vector_<> result;
            for (const auto& curve : curves) {
                REQUIRE(curve && curve->ccy_.String() == snapshot.currency_, "InvalidGSRCurveRisk: curve currency mismatch");
                for (const auto& date : snapshot.nodeDates_) {
                    const double df = (*curve)(snapshot.evaluationDate_, date);
                    REQUIRE(std::isfinite(df) && df > 0.0, "InvalidGSRCurveRisk: source discount must be finite and positive");
                    result.push_back(std::log(df));
                }
            }
            return result;
        }

        Handle_<GSRCurveData_> Snapshot(const GSRCurveData_& source, const Vector_<>& values) {
            const size_t nodes = source.nodeDates_.size();
            Vector_<> discount(values.begin(), values.begin() + nodes);
            Matrix_<> projection(source.projectionLogDF_.Rows(), source.projectionLogDF_.Cols());
            for (int row = 0; row < projection.Rows(); ++row)
                for (int col = 0; col < projection.Cols(); ++col)
                    projection(row, col) = values[nodes * (row + 1) + col];
            return Handle_<GSRCurveData_>(new GSRCurveData_(source.name_, source.evaluationDate_, source.currency_, source.nodeDates_, discount,
                                                            source.projectionTenors_, projection));
        }

        struct Component_ {
            Handle_<DiscountCurve_> curve_;
            CurveParameterState_ parameters_;
            Matrix_<> quoteDirections_;
        };

        Vector_<Handle_<DiscountCurve_>> Rebuild(const Vector_<Handle_<DiscountCurve_>>& roles,
                                                 const std::map<const DiscountCurve_*, Component_>& components,
                                                 int quote,
                                                 double bump) {
            std::map<const DiscountCurve_*, Handle_<DiscountCurve_>> cache;
            std::set<const DiscountCurve_*> visiting;
            std::function<Handle_<DiscountCurve_>(const Handle_<DiscountCurve_>&)> build;
            build = [&](const Handle_<DiscountCurve_>& curve) -> Handle_<DiscountCurve_> {
                const auto found = components.find(curve.get());
                if (found == components.end())
                    return curve;
                const auto cached = cache.find(curve.get());
                if (cached != cache.end())
                    return cached->second;
                REQUIRE(visiting.insert(curve.get()).second, "InvalidGSRCurveRisk: cyclic curve bases");
                const auto& component = found->second;
                auto values = component.parameters_.passiveParameters_;
                for (size_t i = 0; i < values.size(); ++i)
                    values[i] += bump * component.quoteDirections_(i, quote);
                const auto base = build(component.parameters_.passiveBase_);
                auto rebuilt = Handle_<DiscountCurve_>(BuildDiscountCurveT<double>(component.parameters_.definition_, values, base));
                visiting.erase(curve.get());
                cache.emplace(curve.get(), rebuilt);
                return rebuilt;
            };
            Vector_<Handle_<DiscountCurve_>> result;
            for (const auto& curve : roles)
                result.push_back(build(curve));
            return result;
        }
        void ValidateProvenance(const GSRCurveData_& snapshot,
                                const RatePricingMarket_& market,
                                const RateQuoteRiskProvenance_& provenance,
                                size_t projectionCount) {
            REQUIRE(provenance.Available(), "InvalidGSRCurveRisk: curve quote provenance unavailable: " + provenance.Reason());
            REQUIRE(market.valuationTime_.Date() == snapshot.evaluationDate_ && market.resultCurrency_.String() == snapshot.currency_,
                    "InvalidGSRCurveRisk: market date or currency mismatch");
            REQUIRE(projectionCount == snapshot.projectionTenors_.size(), "InvalidGSRCurveRisk: projection component count mismatch");
            for (const auto& state : provenance.State().components_)
                REQUIRE(CurrentRateQuoteRiskComponentState(state.componentKey_, market, provenance.State().scheme_).fingerprint_ ==
                            state.fingerprint_,
                        "InvalidGSRCurveRisk: stale curve quote provenance");
            const auto& axis = provenance.Axis();
            const auto& inverse = provenance.EffectiveInverse();
            REQUIRE(std::isfinite(provenance.Tolerance()) && provenance.Tolerance() > 0.0, "InvalidGSRCurveRisk: curve residual tolerance invalid");
            REQUIRE(inverse.Rows() == static_cast<int>(axis.parameters_.size()) && inverse.Cols() == static_cast<int>(axis.quotes_.size()) &&
                        inverse.Cols() > 0,
                    "InvalidGSRCurveRisk: quote inverse dimensions mismatch");
        }

        void ValidateCoordinate(const RateQuoteRiskParameterCoordinate_& coordinate,
                                size_t row,
                                const Vector_<CurveFreeParameter_>& descriptors,
                                const Vector_<bool>& seen) {
            const int local = coordinate.blockOrdinal_;
            REQUIRE(coordinate.globalOrdinal_ == static_cast<int>(row) && local >= 0 && local < static_cast<int>(descriptors.size()) &&
                        !seen[local] && coordinate.date_ == descriptors[local].date_ && coordinate.component_ == descriptors[local].component_,
                    "InvalidGSRCurveRisk: curve parameter axis mismatch");
        }

        Matrix_<> QuoteDirections(const CurveParameterState_& state, const RateQuoteRiskProvenance_& provenance, const String_& block) {
            const auto& axis = provenance.Axis();
            const auto& inverse = provenance.EffectiveInverse();
            const auto descriptors = DescribeCurveFreeParameters(state.definition_);
            Matrix_<> directions(descriptors.size(), inverse.Cols(), 0.0);
            Vector_<bool> seen(descriptors.size(), false);
            for (size_t row = 0; row < axis.parameters_.size(); ++row) {
                const auto& coordinate = axis.parameters_[row];
                if (coordinate.blockKey_ != block)
                    continue;
                ValidateCoordinate(coordinate, row, descriptors, seen);
                const int local = coordinate.blockOrdinal_;
                seen[local] = true;
                for (int col = 0; col < inverse.Cols(); ++col) {
                    REQUIRE(std::isfinite(inverse(row, col)), "InvalidGSRCurveRisk: nonfinite quote inverse");
                    directions(local, col) = inverse(row, col) / provenance.Tolerance();
                    REQUIRE(std::isfinite(directions(local, col)), "InvalidGSRCurveRisk: quote direction overflow");
                }
            }
            REQUIRE(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }), "InvalidGSRCurveRisk: incomplete parameter axis");
            return directions;
        }

        std::map<const DiscountCurve_*, Component_>
        BoundComponents(const GSRCurveData_& snapshot, const RatePricingMarket_& market, const RateQuoteRiskProvenance_& provenance) {
            std::map<const DiscountCurve_*, Component_> components;
            for (const auto& [block, key] : provenance.ComponentKeyByParameterBlock()) {
                const auto found = market.curveComponents_.find(key);
                REQUIRE(found != market.curveComponents_.end() && found->second, "InvalidGSRCurveRisk: bound curve component missing");
                Component_ component{found->second, InspectCurveParameters(*found->second, snapshot.evaluationDate_), {}};
                component.quoteDirections_ = QuoteDirections(component.parameters_, provenance, block);
                REQUIRE(components.emplace(found->second.get(), std::move(component)).second, "InvalidGSRCurveRisk: duplicate curve bindings");
            }
            return components;
        }

        Vector_<Handle_<DiscountCurve_>> SnapshotCurves(const RatePricingMarket_& market,
                                                        const std::map<const DiscountCurve_*, Component_>& components,
                                                        const String_& discountComponent,
                                                        const Vector_<String_>& projectionComponents) {
            Vector_<Handle_<DiscountCurve_>> roles;
            Vector_<String_> keys{discountComponent};
            keys.Append(projectionComponents);
            for (const auto& key : keys) {
                const auto found = market.curveComponents_.find(key);
                REQUIRE(found != market.curveComponents_.end() && found->second && components.count(found->second.get()),
                        "InvalidGSRCurveRisk: snapshot component missing from quote provenance");
                roles.push_back(found->second);
            }
            return roles;
        }

        void ValidateSnapshotValues(const GSRCurveData_& snapshot, const Vector_<Handle_<DiscountCurve_>>& roles) {
            const auto original = Flatten(snapshot), sampled = Sample(snapshot, roles);
            for (size_t i = 0; i < original.size(); ++i)
                REQUIRE(std::abs(original[i] - sampled[i]) <= 1e-11, "InvalidGSRCurveRisk: snapshot does not match bound market");
        }

        Matrix_<> SnapshotJacobian(const GSRCurveData_& snapshot,
                                   const Vector_<Handle_<DiscountCurve_>>& roles,
                                   const std::map<const DiscountCurve_*, Component_>& components,
                                   const RateQuoteRiskProvenance_& provenance,
                                   Vector_<String_>* names,
                                   Vector_<String_>* units) {
            const auto& axis = provenance.Axis();
            const auto& inverse = provenance.EffectiveInverse();
            const auto nodes = Flatten(snapshot).size();
            Matrix_<> jacobian(nodes, axis.quotes_.size(), 0.0);
            for (int col = 0; col < inverse.Cols(); ++col) {
                const auto& quote = axis.quotes_[col];
                REQUIRE(quote.globalOrdinal_ == col, "InvalidGSRCurveRisk: quote axis ordering mismatch");
                names->push_back("curve:" + provenance.CalibrationId() + ":" + quote.blockKey_ + ":" + String::FromInt(quote.blockOrdinal_) + ":" +
                                 quote.displayName_);
                units->push_back(quote.unit_);
                const double step = 1e-6;
                const auto up = Sample(snapshot, Rebuild(roles, components, col, step));
                const auto down = Sample(snapshot, Rebuild(roles, components, col, -step));
                for (size_t row = 0; row < nodes; ++row) {
                    jacobian(row, col) = (up[row] - down[row]) / (2.0 * step);
                    REQUIRE(std::isfinite(jacobian(row, col)), "InvalidGSRCurveRisk: nonfinite snapshot Jacobian");
                }
            }
            return jacobian;
        }
    } // namespace

    GSRCurveQuoteRisk_ BuildGSRCurveQuoteRisk(const GSRCurveData_& snapshot,
                                              const RatePricingMarket_& market,
                                              const RateQuoteRiskProvenance_& provenance,
                                              const String_& discountComponent,
                                              const Vector_<String_>& projectionComponents) {
        ValidateProvenance(snapshot, market, provenance, projectionComponents.size());
        const auto components = BoundComponents(snapshot, market, provenance);
        const auto roles = SnapshotCurves(market, components, discountComponent, projectionComponents);
        ValidateSnapshotValues(snapshot, roles);
        Vector_<String_> names, units;
        const auto jacobian = SnapshotJacobian(snapshot, roles, components, provenance, &names, &units);
        return GSRCurveQuoteRisk_(Snapshot(snapshot, Flatten(snapshot)), names, units, jacobian);
    }

    Handle_<GSRCurveData_> GSRCurveQuoteRisk_::Shifted(size_t quote, double bump) const {
        REQUIRE(quote < quoteNames_.size() && std::isfinite(bump), "InvalidGSRCurveRisk: quote index or bump invalid");
        auto values = Flatten(*snapshot_);
        for (size_t row = 0; row < values.size(); ++row)
            values[row] += bump * logDFQuoteJacobian_(row, quote);
        return Dal::Snapshot(*snapshot_, values);
    }
} // namespace Dal
