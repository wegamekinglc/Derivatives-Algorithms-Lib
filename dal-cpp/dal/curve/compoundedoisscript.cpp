#include <dal/platform/platform.hpp>

#include <dal/curve/compoundedoisscript_internal.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/script/event.hpp>
#include <dal/script/instrument_internal.hpp>

namespace Dal::CompoundedOisInternal {
    using namespace CalibrationInternal;
    Index::Overnight_ Index(const CompoundedOisScriptSpec_& s, size_t observation) {
        const auto& r = s.observations_[observation];
        return {s.currency_, s.fixingIndex_, s.projectionRoute_, r.start_, r.end_, r.rateStart_, r.rateEnd_, s.observationBasis_};
    }
    String_ IndexName(const PreparedCompoundedOis_& tables, size_t observation) {
        const auto& s = tables.Spec();
        if (Bytes(s.ruleId_) == "dal.compounded-ois-cashflows/1")
            return Index(s, observation).Name();
        const auto& r = tables.ObservationSchedule()[observation];
        return Index::OvernightObservation_(
                   {s.currency_, r.fixingIndex_, s.projectionRoute_, r.accrualStart_, r.accrualEnd_, r.rateStart_, r.rateEnd_, s.observationBasis_})
            .Name();
    }
    namespace {
        void Product(
            const PreparedCompoundedOis_& tables, size_t first, size_t last, size_t sourceRow, String_* text, Vector_<SourceLocation_>* locations) {
            if (last - first == 1) {
                const auto& row = tables.ObservationSchedule()[first];
                locations->push_back({row.sourceKey_.substr(0, row.sourceKey_.find('/')), first, sourceRow, 0, text->size()});
                *text += "(1+" + NumberText(row.compoundingAccrual_) + "*FIX(" + IndexName(tables, first) + "," +
                         Date::ToString(row.fixingTime_.Date()) + "))";
                return;
            }
            const size_t middle = first + (last - first) / 2;
            *text += "(";
            Product(tables, first, middle, sourceRow, text, locations);
            *text += "*";
            Product(tables, middle, last, sourceRow, text, locations);
            *text += ")";
        }
        ScriptInstrumentDefinitionSpec_ Definition(const CompoundedOisScriptSpec_& s) {
            ScriptInstrumentDefinitionSpec_ d;
            const auto price = CalibrationValueUnit_::Parse("Price:" + String_(s.currency_.String()));
            const auto rate = CalibrationValueUnit_::Parse("Rate");
            d.header_.payCurrency_ = d.header_.resultCurrency_ = d.header_.collateralCurrency_ = s.currency_;
            d.header_.discountRoute_ = s.discountRoute_;
            d.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", NumberText(s.notional_)},
                               {ScriptSourceRowKind_::Value_::Definition, "K", NumberText(s.coupon_)}};
            d.parameters_ = {{"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true},
                             {"K", ScriptParameterType_::Value_::Real, rate, ScriptParameterRole_::Value_::Coupon, true}};
            d.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price},
                           {"ParRate", CalibrationMeasureKind_::Value_::ParAffineRate, "PAY", String_("K"), rate, s.direction_ == "PayFixed" ? -1 : 1,
                            s.minAbsAnnuity_}};
            return d;
        }
        void MapStatements(const ScriptInstrumentDefinitionSpec_& definition, Vector_<SourceLocation_>* locations) {
            std::map<String_, size_t> statements;
            Vector_<size_t> statementByRow(definition.source_.rows_.size(), 0);
            for (size_t i = 2; i < definition.source_.rows_.size(); ++i)
                statementByRow[i] = statements[definition.source_.rows_[i].key_]++;
            for (auto& location : *locations)
                location.statement_ = statementByRow[location.sourceRow_ - 1];
        }
    } // namespace
    Source_ BuildSource(const Handle_<PreparedCompoundedOis_>& tables) {
        Check(bool(tables), "InvalidCompoundedOis", "/tables", "frozen compounded OIS tables");
        const auto& s = tables->Spec();
        auto d = Definition(s);
        Source_ result;
        Date_ pillar = s.fixedLeg_.back().payment_;
        for (size_t i = 0; i < s.fixedLeg_.size(); ++i) {
            const auto& r = s.fixedLeg_[i];
            const String_ sign = s.direction_ == "PayFixed" ? "-" : "";
            result.locations_.push_back({"fixed_leg", i, d.source_.rows_.size() + 1, 0, 0});
            d.source_.rows_.push_back(
                {ScriptSourceRowKind_::Value_::Event, Date::ToString(r.payment_), "PAY PAYS N*(" + sign + "(" + NumberText(r.accrual_) + "*K))"});
            pillar = std::max(pillar, r.payment_);
        }
        size_t first = 0;
        for (size_t i = 0; i < s.coupons_.size(); ++i) {
            size_t last = first;
            while (last < tables->ObservationSchedule().size() && tables->ObservationSchedule()[last].couponRow_ == i)
                ++last;
            const auto row = d.source_.rows_.size() + 1;
            result.locations_.push_back({"coupons", i, row, 0, 0});
            String_ text = s.direction_ == "PayFixed" ? "PAY PAYS N*((" : "PAY PAYS N*(-(";
            Product(*tables, first, last, row, &text, &result.locations_);
            text += "-1))";
            d.source_.rows_.push_back({ScriptSourceRowKind_::Value_::Event, Date::ToString(s.coupons_[i].payment_), std::move(text)});
            pillar = std::max(pillar, std::max(s.coupons_[i].payment_, tables->ObservationSchedule()[last - 1].rateEnd_));
            first = last;
        }
        result.instrument_ = NewScriptCalibrationInstrument({s.instrumentId_, NewScriptInstrumentDefinition(d), {}, pillar});
        MapStatements(d, &result.locations_);
        return result;
    }
    bool ContainsOvernight(const Script::ScriptProduct_& product) {
        for (const auto& event : product.Events())
            for (const auto& node : event)
                if (Script::FindNode(*node, [](const Script::Node_& n) {
                        const auto* fixing = dynamic_cast<const Script::NodeFix_*>(&n);
                        return fixing && (dynamic_cast<const Index::Overnight_*>(fixing->index_.get()) ||
                                          dynamic_cast<const Index::OvernightObservation_*>(fixing->index_.get()));
                    }))
                    return true;
        return false;
    }
    bool ContainsOvernight(const CalibrationInstrument_& instrument) { return ContainsOvernight(*PreparationProduct(instrument)); }
} // namespace Dal::CompoundedOisInternal

namespace Dal {
    Handle_<CalibrationInstrument_> NewCompoundedOisScriptInstrument(const Handle_<PreparedCompoundedOis_>& tables) {
        return CompoundedOisInternal::BuildSource(tables).instrument_;
    }
} // namespace Dal
