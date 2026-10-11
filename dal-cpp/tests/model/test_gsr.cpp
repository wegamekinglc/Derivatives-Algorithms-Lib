//
// Created by Codex on 2026/9/28.
//

#include <gtest/gtest.h>

#include <cmath>
#include <future>
#include <utility>

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/gsrdata.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/json.hpp>
#include <dal/time/schedules.hpp>

namespace {
    Dal::Handle_<Dal::ModelData_> ObservationModelData(size_t factors, bool projectionCurve = false) {
        const Dal::Date_ today(2026, 10, 2);
        const Dal::Vector_<Dal::Date_> dates{today, today.AddDays(365), today.AddDays(3650)};
        Dal::Matrix_<> projection(projectionCurve ? 1 : 0, projectionCurve ? 3 : 0, 0.0);
        if (projectionCurve) {
            projection(0, 1) = -0.03;
            projection(0, 2) = -0.3;
        }
        const Dal::Handle_<Dal::GSRCurveData_> curve(
            new Dal::GSRCurveData_("curve", today, "USD", dates, {0.0, -0.03, -0.3},
                                   projectionCurve ? Dal::Vector_<Dal::String_>{"3M"} : Dal::Vector_<Dal::String_>{}, projection));
        Dal::MultiFactorGSRVolSettings_ settings;
        settings.gKnotDates_ = {today, today.AddDays(120), today.AddDays(240)};
        settings.hKnotDates_ = {today, today.AddDays(180)};
        settings.gValues_ = Dal::Matrix_<>(static_cast<int>(factors), 3);
        settings.hValues_ = Dal::Matrix_<>(static_cast<int>(factors), 2);
        settings.correlations_ = Dal::Matrix_<>(static_cast<int>(factors), static_cast<int>(factors), 0.3);
        for (size_t factor = 0; factor < factors; ++factor) {
            settings.factorNames_.push_back("factor" + Dal::String::FromInt(static_cast<int>(factor)));
            settings.correlations_(static_cast<int>(factor), static_cast<int>(factor)) = 1.0;
            for (int knot = 0; knot < 3; ++knot)
                settings.gValues_(static_cast<int>(factor), knot) = Dal::Vector_<>{0.02, 0.015, 0.025}[knot] / (factor + 1);
            for (int knot = 0; knot < 2; ++knot)
                settings.hValues_(static_cast<int>(factor), knot) = Dal::Vector_<>{1.0, 0.8}[knot] * (1.0 - factor * 0.3);
        }
        return Dal::Handle_<Dal::ModelData_>(new Dal::MultiFactorGSRModelData_(
            "rates", curve, Dal::Handle_<Dal::MultiFactorGSRVolData_>(new Dal::MultiFactorGSRVolData_("vol", settings))));
    }

    Dal::Vector_<Dal::AAD::SampleDef_> RepeatedObservations() {
        Dal::Vector_<Dal::AAD::SampleDef_> definitions(3);
        const Dal::Date_ today(2026, 10, 2), payment(2031, 10, 2);
        for (auto& definition : definitions) {
            definition.indexNames_ = {"IR[USD,SWAP,5Y]", "IR[USD,LIBOR_3M_LCH]", "IR[USD,DF,2031-10-02]", "IR[USD,LIBOR_6M_LCH]"};
            definition.discountMats_ = {(payment - today) / Dal::DAYS_PER_YEAR, (payment - today) / Dal::DAYS_PER_YEAR};
        }
        return definitions;
    }

    template <class T_> T_ ObservationPayoff(const Dal::AAD::Scenario_<T_>& path) {
        T_ total = path[0].observations_[0];
        for (const auto& sample : path) {
            for (size_t slot = 0; slot < sample.observations_.size(); ++slot)
                total += (slot + 1) * sample.observations_[slot];
            total += 0.2 * sample.discounts_[0] + 0.3 * sample.discounts_[1];
        }
        return total / path.back().numeraire_;
    }

    double ObservationValue(size_t factors, size_t parameter, double shift, const Dal::Vector_<>& gaussian) {
        auto model = Dal::CreateModel<double>(ObservationModelData(factors, true));
        const Dal::Vector_<> timeline{0.0, 146.0 / Dal::DAYS_PER_YEAR, 292.0 / Dal::DAYS_PER_YEAR};
        const auto definitions = RepeatedObservations();
        model->Allocate(timeline, definitions);
        *model->Parameters()[parameter] += shift;
        model->Init(timeline, definitions);
        Dal::AAD::Scenario_<> path;
        Dal::AAD::AllocatePath(definitions, path);
        model->GeneratePath(gaussian, &path);
        return ObservationPayoff(path);
    }

    double ObservationDifference(size_t factors, size_t parameter, double step, const Dal::Vector_<>& gaussian) {
        return (ObservationValue(factors, parameter, step, gaussian) - ObservationValue(factors, parameter, -step, gaussian)) / (2.0 * step);
    }

    Dal::AAD::Number_ HybridObservationPayoff(const Dal::Vector_<Dal::AAD::Number_>& observations, const Dal::Vector_<Dal::AAD::Number_>& discounts) {
        Dal::AAD::Number_ payoff = 0.2 * discounts[0] + 0.3 * discounts[1];
        for (size_t slot = 0; slot < observations.size(); ++slot)
            payoff += (slot + 1) * observations[slot];
        return payoff;
    }

    void AssertHybridObservationValues(const Dal::AAD::Sample_<Dal::AAD::Number_>& output,
                                       const Dal::Vector_<Dal::AAD::Number_>& observations,
                                       const Dal::Vector_<Dal::AAD::Number_>& discounts,
                                       size_t prefix) {
        for (size_t i = 0; i < prefix; ++i)
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(output.modelScratch_[i]), 0.03 * (i + 1));
        for (size_t slot = 0; slot < observations.size(); ++slot)
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(output.observations_[slot]), Dal::AAD::Value(observations[slot]));
        for (size_t slot = 0; slot < discounts.size(); ++slot)
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(output.discounts_[slot]), Dal::AAD::Value(discounts[slot]));
    }

    Dal::Vector_<>
    HybridObservationRisks(const Dal::AAD::GSR_<Dal::AAD::Number_>& model, const Dal::Vector_<Dal::AAD::Number_>& state, size_t prefix) {
        Dal::Vector_<> values;
        for (const auto* parameter : model.Parameters())
            values.push_back(Dal::AAD::Adjoint(*parameter));
        for (size_t i = 0; i < prefix; ++i)
            values.push_back(Dal::AAD::Adjoint(state[i]));
        return values;
    }

    void AssertObservationBatchRisks(const Dal::AAD::GSR_<Dal::AAD::Number_>& model,
                                     const Dal::Vector_<Dal::AAD::Number_>& state,
                                     size_t prefix,
                                     const Dal::AAD::Number_& directPayoff,
                                     const Dal::AAD::Number_& batchPayoff) {
        Dal::AAD::Adjoint(directPayoff) = 1.0;
        Dal::AAD::PropagateToStart(*Dal::AAD::Tape());
        const auto directRisks = HybridObservationRisks(model, state, prefix);
        Dal::AAD::ZeroAdjoints(*Dal::AAD::Tape());
        Dal::AAD::Adjoint(batchPayoff) = 1.0;
        Dal::AAD::PropagateToStart(*Dal::AAD::Tape());
        const auto batchRisks = HybridObservationRisks(model, state, prefix);
        ASSERT_EQ(batchRisks.size(), directRisks.size());
        for (size_t i = 0; i < batchRisks.size(); ++i)
            ASSERT_NEAR(batchRisks[i], directRisks[i], 1e-12 * std::max(1.0, std::abs(directRisks[i])));
    }

    bool FillStandaloneHJMFromFlat(const Dal::AAD::GSR_<Dal::AAD::Number_>& model,
                                   const Dal::Vector_<Dal::AAD::Number_>& flat,
                                   Dal::AAD::Sample_<Dal::AAD::Number_>* output) {
        const size_t factors = model.NumFactors();
        Dal::Vector_<Dal::AAD::Number_> state(flat.begin() + 1, flat.begin() + 1 + factors);
        Dal::Matrix_<Dal::AAD::Number_> covariance(factors, factors);
        for (size_t i = 0; i < factors; ++i)
            for (size_t j = 0; j < factors; ++j)
                covariance(i, j) = flat[1 + factors + i * factors + j];
        return model.FillHJMObservations(1, state, covariance, output);
    }

    void RegisterObservationInputs(Dal::AAD::GSR_<Dal::AAD::Number_>* model, Dal::Vector_<Dal::AAD::Number_>* state) {
        for (auto* parameter : model->Parameters())
            Dal::AAD::PutOnTape(*parameter);
        for (auto& value : *state)
            Dal::AAD::PutOnTape(value);
    }

    void CheckHJMObservationBatch(size_t factors, bool hybrid) {
        using namespace Dal;
        const TapeGuard_ guard(AAD::Tape());
        const auto data = ObservationModelData(factors, true);
        const auto* rates = dynamic_cast<const MultiFactorGSRModelData_*>(data.get());
        ASSERT_NE(rates, nullptr);
        AAD::GSR_<AAD::Number_> model(*rates);
        const Vector_<> timeline{0.0, 146.0 / DAYS_PER_YEAR};
        const Vector_<AAD::SampleDef_> definitions(timeline.size(), RepeatedObservations()[0]);
        model.Allocate(timeline, definitions);
        AAD::Sample_<AAD::Number_> output;
        output.Allocate(definitions[1]);
        Vector_<AAD::Number_> inputs;
        auto& flat = hybrid ? output.modelScratch_ : inputs;
        const size_t prefix = factors + factors * factors + 2;
        for (size_t i = 0; i < prefix; ++i)
            flat.push_back(0.03 * (i + 1));
        AAD::Rewind(*AAD::Tape());
        RegisterObservationInputs(&model, &flat);
        AAD::NewRecording(*AAD::Tape());
        model.InitHJM(timeline, definitions);
        const Vector_<std::pair<size_t, size_t>> slots = hybrid ? Vector_<std::pair<size_t, size_t>>{{2, 0}, {0, 3}, {1, 2}, {3, 1}}
                                                                : Vector_<std::pair<size_t, size_t>>{{0, 0}, {1, 1}, {2, 2}, {3, 3}};
        Vector_<AAD::Number_> direct(output.observations_.size()), discounts(output.discounts_.size());
        for (const auto& slot : slots)
            direct[slot.second] = model.ObserveHJMFlat(1, slot.first, &flat[1], &flat[1 + factors]);
        for (auto& discount : discounts)
            discount = model.ObserveHJMFlat(1, 2, &flat[1], &flat[1 + factors]);
        AAD::Number_ directPayoff = HybridObservationPayoff(direct, discounts);
        if (hybrid) {
            ASSERT_TRUE(model.FillHybridHJMObservationBatch(1, slots, flat, 1, &output, prefix));
        } else {
            ASSERT_TRUE(FillStandaloneHJMFromFlat(model, flat, &output));
        }
        ASSERT_NO_FATAL_FAILURE(AssertHybridObservationValues(output, direct, discounts, hybrid ? prefix : 0));
        AAD::Number_ batchPayoff = HybridObservationPayoff(output.observations_, output.discounts_);
        ASSERT_NO_FATAL_FAILURE(AssertObservationBatchRisks(model, flat, prefix, directPayoff, batchPayoff));
    }

    Dal::String_ CashSettledSwaptionExercise(const Dal::Date_& exercise, double strike) {
        const Dal::Ccy_ currency("USD");
        const Dal::Index::Swap_ swap(currency, "5Y");
        const Dal::Date_ start = swap.StartDate(Dal::DateTime_(exercise, 0.0));
        const Dal::Date_ maturity = Dal::Date::ParseIncrement("5Y")->FwdFrom(start);
        const auto& fixed = Dal::Ccy::Conventions::SwapFixedLeg()(currency);
        const auto periods = Dal::MakeSchedulePeriods(start, maturity, fixed.paymentFrequency_, fixed.accrualHolidays_, 0, Dal::Holidays::None(),
                                                      fixed.paymentLag_, fixed.paymentHolidays_, Dal::DateGeneration_("Forward"),
                                                      fixed.businessDayConvention_, fixed.paymentConvention_, fixed.endOfMonth_);
        Dal::String_ annuity;
        for (const auto& period : periods) {
            if (!annuity.empty())
                annuity += " + ";
            const double accrual = fixed.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get());
            annuity += Dal::String::FromDouble(accrual) + " * FIX(IR[USD,DF," + Dal::Date::ToString(period.paymentDate_) + "])";
        }
        return "EXERCISE MAX((FIX(IR[USD,SWAP,5Y]) - " + Dal::String::FromDouble(strike) + ") * (" + annuity + "), 0)";
    }
} // namespace

using namespace Dal;

TEST(ModelTest, TestGsrSwapObservationsFitNodeBudget) {
    for (const size_t factors : {size_t{1}, size_t{3}}) {
        const TapeGuard_ guard(AAD::Tape());
        auto model = CreateModel<AAD::Number_>(ObservationModelData(factors));
        Vector_<> timeline;
        for (int quarter = 0; quarter <= 16; ++quarter)
            timeline.push_back(quarter * 91.0 / DAYS_PER_YEAR);
        Vector_<AAD::SampleDef_> definitions(timeline.size());
        for (auto& definition : definitions)
            definition.indexNames_ = {"IR[USD,SWAP,5Y]"};
        model->Allocate(timeline, definitions);
        AAD::Scenario_<AAD::Number_> path;
        AAD::AllocatePath(definitions, path);
        AAD::Rewind(*AAD::Tape());
        for (auto* parameter : model->Parameters())
            AAD::PutOnTape(*parameter);
        AAD::NewRecording(*AAD::Tape());
        model->Init(timeline, definitions);
        const auto prefixNodes = AAD::Tape()->nodes_.OccupiedSlots();
        model->GeneratePath(Vector_<>(model->SimDim(), 0.2), &path);
        ASSERT_LE(AAD::Tape()->nodes_.OccupiedSlots() - prefixNodes, factors == 1 ? 1900U : 3000U);
    }
}

TEST(ModelTest, TestGsrSharedBondsPreserveAllRisksAndRecordingReplay) {
    for (const size_t factors : {size_t{1}, size_t{3}}) {
        const TapeGuard_ guard(AAD::Tape());
        auto model = CreateModel<AAD::Number_>(ObservationModelData(factors, true));
        const Vector_<> timeline{0.0, 146.0 / DAYS_PER_YEAR, 292.0 / DAYS_PER_YEAR};
        const auto definitions = RepeatedObservations();
        model->Allocate(timeline, definitions);
        const Vector_<> gaussian(model->SimDim(), 0.2);
        AAD::Scenario_<AAD::Number_> path;
        AAD::AllocatePath(definitions, path);
        for (int recording = 0; recording < 2; ++recording) {
            AAD::Rewind(*AAD::Tape());
            for (auto* parameter : model->Parameters())
                AAD::PutOnTape(*parameter);
            AAD::NewRecording(*AAD::Tape());
            model->Init(timeline, definitions);
            AAD::Mark(*AAD::Tape());
            for (int replay = 0; replay < 2; ++replay) {
                AAD::RewindToMark(*AAD::Tape());
                AAD::ZeroAdjoints(*AAD::Tape());
                model->GeneratePath(gaussian, &path);
                const AAD::Number_ payoff = ObservationPayoff(path);
                AAD::Adjoint(payoff) = 1.0;
                AAD::PropagateToStart(*AAD::Tape());
                ASSERT_NEAR(AAD::Value(payoff), ObservationValue(factors, 0, 0.0, gaussian), 1e-10);
                for (size_t parameter = 0; parameter < model->Parameters().size(); ++parameter) {
                    SCOPED_TRACE(model->ParameterLabels()[parameter].c_str());
                    const double risk = AAD::Adjoint(*model->Parameters()[parameter]);
                    const double coarse = ObservationDifference(factors, parameter, 1e-5, gaussian);
                    const double fine = ObservationDifference(factors, parameter, 5e-6, gaussian);
                    const double difference = (4.0 * fine - coarse) / 3.0;
                    ASSERT_NEAR(risk, difference, 1e-7 * std::max(1.0, std::abs(risk)));
                }
                ASSERT_GT(std::abs(AAD::Adjoint(*model->Parameters()[2])), 0.1);
                ASSERT_DOUBLE_EQ(AAD::Value(path.back().discounts_[0]), AAD::Value(path.back().discounts_[1]));
                ASSERT_DOUBLE_EQ(AAD::Value(path.back().discounts_[0]), AAD::Value(path.back().observations_[2]));
            }
        }
    }
}

TEST(ModelTest, TestGsrSharedModelKeepsBondPricesInEachPath) {
    auto model = CreateModel<double>(ObservationModelData(3, true));
    const Vector_<> timeline{0.0, 146.0 / DAYS_PER_YEAR, 292.0 / DAYS_PER_YEAR};
    const auto definitions = RepeatedObservations();
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    const auto evaluate = [&](double normal) {
        AAD::Scenario_<> path;
        AAD::AllocatePath(definitions, path);
        for (int iteration = 0; iteration < 50; ++iteration)
            model->GeneratePath(Vector_<>(model->SimDim(), normal), &path);
        return ObservationPayoff(path);
    };
    const double positive = evaluate(0.3), negative = evaluate(-0.4);
    ASSERT_GT(std::abs(positive - negative), 0.01);
    auto first = std::async(std::launch::async, evaluate, 0.3);
    auto second = std::async(std::launch::async, evaluate, -0.4);
    ASSERT_DOUBLE_EQ(first.get(), positive);
    ASSERT_DOUBLE_EQ(second.get(), negative);
}

TEST(ModelTest, TestGsrHybridBatchMatchesDirectObservationsAndRisks) {
    for (const size_t factors : {size_t{1}, size_t{3}}) {
        const TapeGuard_ guard(AAD::Tape());
        const auto data = ObservationModelData(factors, true);
        const auto* rates = dynamic_cast<const MultiFactorGSRModelData_*>(data.get());
        ASSERT_NE(rates, nullptr);
        AAD::GSR_<AAD::Number_> model(*rates);
        const Vector_<> timeline{
            0.0, 120.0 / DAYS_PER_YEAR, 146.0 / DAYS_PER_YEAR, 180.0 / DAYS_PER_YEAR, 240.0 / DAYS_PER_YEAR, 292.0 / DAYS_PER_YEAR};
        const Vector_<AAD::SampleDef_> definitions(timeline.size(), RepeatedObservations()[0]);
        const size_t sample = 2;
        model.Allocate(timeline, definitions);
        AAD::Sample_<AAD::Number_> output;
        output.Allocate(definitions[sample]);
        const size_t prefix = factors + 2;
        for (size_t i = 0; i < prefix; ++i)
            output.modelScratch_.push_back(0.03 * (i + 1));
        AAD::Rewind(*AAD::Tape());
        for (auto* parameter : model.Parameters())
            AAD::PutOnTape(*parameter);
        for (auto& value : output.modelScratch_)
            AAD::PutOnTape(value);
        AAD::NewRecording(*AAD::Tape());
        model.InitHybrid(timeline, definitions);
        ASSERT_TRUE(model.UsesHybridObservationBatch(sample));
        const Vector_<std::pair<size_t, size_t>> slots{{2, 0}, {0, 3}, {1, 2}, {3, 1}};
        Vector_<AAD::Number_> direct(output.observations_.size()), discounts(output.discounts_.size());
        for (const auto& slot : slots)
            direct[slot.second] = model.ObserveHybrid(sample, slot.first, &output.modelScratch_[1]);
        model.ObserveHybridDiscounts(sample, &output.modelScratch_[1], &discounts);
        AAD::Number_ directPayoff = HybridObservationPayoff(direct, discounts);
        ASSERT_TRUE(model.FillHybridObservationBatch(sample, slots, output.modelScratch_, 1, &output, prefix));
        ASSERT_NO_FATAL_FAILURE(AssertHybridObservationValues(output, direct, discounts, prefix));
        AAD::Number_ batchPayoff = HybridObservationPayoff(output.observations_, output.discounts_);
        AAD::Adjoint(directPayoff) = 1.0;
        AAD::PropagateToStart(*AAD::Tape());
        const auto directRisks = HybridObservationRisks(model, output.modelScratch_, prefix);
        AAD::ZeroAdjoints(*AAD::Tape());
        AAD::Adjoint(batchPayoff) = 1.0;
        AAD::PropagateToStart(*AAD::Tape());
        const auto batchRisks = HybridObservationRisks(model, output.modelScratch_, prefix);
        ASSERT_EQ(batchRisks.size(), directRisks.size());
        for (size_t i = 0; i < batchRisks.size(); ++i)
            ASSERT_NEAR(batchRisks[i], directRisks[i], 1e-12 * std::max(1.0, std::abs(directRisks[i])));
        ASSERT_THROW(model.FillHybridObservationBatch(sample, slots, output.modelScratch_, 1, &output, 0), Exception_);
        const Vector_<std::pair<size_t, size_t>> invalidSlots{{0, output.observations_.size()}};
        ASSERT_THROW(model.FillHybridObservationBatch(sample, invalidSlots, output.modelScratch_, 1, &output, prefix), Exception_);
        ASSERT_THROW(model.FillHybridObservationBatch(timeline.size(), slots, output.modelScratch_, 1, &output, prefix), Exception_);
    }
}

TEST(ModelTest, TestGsrHybridBatchLeavesSingleUseObservationsToDirectCalls) {
    const auto data = ObservationModelData(1, true);
    const auto* rates = dynamic_cast<const MultiFactorGSRModelData_*>(data.get());
    ASSERT_NE(rates, nullptr);
    AAD::GSR_<> model(*rates);
    const Vector_<> timeline{0.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"IR[USD,LIBOR_3M_LCH]"};
    model.Allocate(timeline, definitions);
    model.InitHybrid(timeline, definitions);
    AAD::Sample_<> output;
    output.Allocate(definitions[0]);
    output.Initialize();
    output.modelScratch_ = {99.0};
    ASSERT_FALSE(model.UsesHybridObservationBatch(0));
    ASSERT_THROW(static_cast<void>(model.UsesHybridObservationBatch(timeline.size())), Exception_);
    ASSERT_FALSE(model.FillHybridObservationBatch(0, {{0, 0}}, {0.02}, 0, &output, 0));
    ASSERT_DOUBLE_EQ(output.observations_[0], 0.0);
    ASSERT_EQ(output.modelScratch_.size(), 1U);
    ASSERT_DOUBLE_EQ(output.modelScratch_[0], 99.0);
}

TEST(ModelTest, TestGsrHjmBatchMatchesDirectObservationsAndAllInputRisks) {
    for (const size_t factors : {size_t{1}, size_t{3}})
        ASSERT_NO_FATAL_FAILURE(CheckHJMObservationBatch(factors, false));
}

TEST(ModelTest, TestGsrHybridHjmBatchPreservesAliasedStateAndAllInputRisks) {
    for (const size_t factors : {size_t{1}, size_t{3}})
        ASSERT_NO_FATAL_FAILURE(CheckHJMObservationBatch(factors, true));
}

TEST(ModelTest, TestGsrHjmBatchRejectsMalformedStateAndOutput) {
    const auto data = ObservationModelData(3, true);
    const auto* rates = dynamic_cast<const MultiFactorGSRModelData_*>(data.get());
    ASSERT_NE(rates, nullptr);
    AAD::GSR_<> model(*rates);
    const Vector_<> timeline{0.0};
    const Vector_<AAD::SampleDef_> definitions(1, RepeatedObservations()[0]);
    model.Allocate(timeline, definitions);
    model.InitHJM(timeline, definitions);
    AAD::Sample_<> output;
    output.Allocate(definitions[0]);
    output.modelScratch_ = Vector_<>(15, 0.03);
    const Matrix_<> covariance(3, 3, 0.001);
    ASSERT_THROW(model.FillHJMObservations(1, {0.02, 0.01, 0.03}, covariance, &output), Exception_);
    ASSERT_THROW(model.FillHJMObservations(0, {0.02}, covariance, &output), Exception_);
    ASSERT_THROW(model.FillHJMObservations(0, {0.02, 0.01, 0.03}, Matrix_<>(2, 2), &output), Exception_);
    ASSERT_THROW(model.FillHJMObservations(0, {0.02, 0.01, 0.03}, covariance, nullptr), Exception_);
    ASSERT_THROW(model.FillHybridHJMObservationBatch(0, {{0, 0}}, {0.02}, 0, &output, 0), Exception_);
    ASSERT_THROW(model.FillHybridHJMObservationBatch(0, {{0, 0}}, output.modelScratch_, 1, &output, 4), Exception_);
    ASSERT_THROW(model.FillHybridHJMObservationBatch(0, {{0, 0}}, output.modelScratch_, 1, &output, 16), Exception_);
    ASSERT_THROW(model.FillHybridHJMObservationBatch(0, {{4, 0}}, output.modelScratch_, 1, &output, 15), Exception_);
    ASSERT_THROW(model.FillHybridHJMObservationBatch(0, {{0, 4}}, output.modelScratch_, 1, &output, 15), Exception_);
    output.discounts_.clear();
    ASSERT_THROW(model.FillHybridHJMObservationBatch(0, {{0, 0}}, output.modelScratch_, 1, &output, 15), Exception_);
    ASSERT_THROW(model.FillHJMObservations(0, {0.02, 0.01, 0.03}, covariance, &output), Exception_);
    output.discounts_.Resize(definitions[0].discountMats_.size());
    output.observations_.pop_back();
    ASSERT_THROW(model.FillHJMObservations(0, {0.02, 0.01, 0.03}, covariance, &output), Exception_);
    output.observations_.Resize(definitions[0].indexNames_.size());
    output.modelScratch_ = {0.02, 0.01, 0.03};
    ASSERT_THROW(model.FillHJMObservations(0, output.modelScratch_, covariance, &output), Exception_);
}

TEST(ModelTest, TestGsrHjmBatchLeavesSingleUseOutputsUntouched) {
    const auto data = ObservationModelData(1, true);
    const auto* rates = dynamic_cast<const MultiFactorGSRModelData_*>(data.get());
    ASSERT_NE(rates, nullptr);
    AAD::GSR_<> model(*rates);
    const Vector_<> timeline{0.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"IR[USD,LIBOR_3M_LCH]"};
    model.Allocate(timeline, definitions);
    model.InitHJM(timeline, definitions);
    AAD::Sample_<> output;
    output.Allocate(definitions[0]);
    output.Initialize();
    output.modelScratch_ = {99.0};
    ASSERT_FALSE(model.FillHJMObservations(0, {0.02}, Matrix_<>(1, 1, 0.001), &output));
    ASSERT_FALSE(model.FillHybridHJMObservationBatch(0, {{0, 0}}, {0.02, 0.001}, 0, &output, 0));
    ASSERT_DOUBLE_EQ(output.observations_[0], 0.0);
    ASSERT_EQ(output.modelScratch_.size(), 1U);
    ASSERT_DOUBLE_EQ(output.modelScratch_[0], 99.0);
}

TEST(ModelTest, TestGsrEqualBondValuesKeepDistinctMaturityRisks) {
    const Date_ today(2026, 10, 2);
    const Date_ first = today.AddDays(730), second = today.AddDays(1095);
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, second}, {0.0, 0.0}, {}, Matrix_<>(0, 0)));
    const Handle_<MultiFactorGSRVolData_> vol(
        new MultiFactorGSRVolData_("vol", {"factor"}, {today}, Matrix_<>(1, 1, 0.02), {today}, Matrix_<>(1, 1, 0.0), Matrix_<>(1, 1, 1.0)));
    const TapeGuard_ guard(AAD::Tape());
    auto model = CreateModel<AAD::Number_>(Handle_<ModelData_>(new MultiFactorGSRModelData_("rates", curve, vol)));
    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"IR[USD,DF," + Date::ToString(first) + "]", "IR[USD,DF," + Date::ToString(second) + "]"};
    model->Allocate(timeline, definitions);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(definitions, path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init(timeline, definitions);
    model->GeneratePath({0.7}, &path);
    ASSERT_DOUBLE_EQ(AAD::Value(path[0].observations_[0]), 1.0);
    ASSERT_DOUBLE_EQ(AAD::Value(path[0].observations_[1]), 1.0);
    const AAD::Number_ payoff = path[0].observations_[0] + 2.0 * path[0].observations_[1];
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters().back()), -0.02 * 0.7 * 5.0, 1e-10);
}

TEST(ModelTest, TestGsrReallocationAndCloneReplacePreparedBondReferences) {
    const auto data = ObservationModelData(3, true);
    auto model = CreateModel<double>(data);
    const auto originalDefinitions = RepeatedObservations();
    const Vector_<> originalTimeline{0.0, 146.0 / DAYS_PER_YEAR, 292.0 / DAYS_PER_YEAR};
    model->Allocate(originalTimeline, originalDefinitions);
    model->Init(originalTimeline, originalDefinitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(originalDefinitions, path);
    model->GeneratePath(Vector_<>(model->SimDim(), 0.2), &path);
    const Vector_<> timeline{182.0 / DAYS_PER_YEAR};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"IR[USD,DF,2034-10-02]"};
    definitions[0].discountMats_ = {(Date_(2033, 10, 2) - Date_(2026, 10, 2)) / DAYS_PER_YEAR};
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    AAD::AllocatePath(definitions, path);
    const Vector_<> gaussian(model->SimDim(), -0.3);
    model->GeneratePath(gaussian, &path);
    auto fresh = CreateModel<double>(data);
    fresh->Allocate(timeline, definitions);
    fresh->Init(timeline, definitions);
    AAD::Scenario_<> reference;
    AAD::AllocatePath(definitions, reference);
    fresh->GeneratePath(gaussian, &reference);
    ASSERT_DOUBLE_EQ(path[0].observations_[0], reference[0].observations_[0]);
    ASSERT_DOUBLE_EQ(path[0].discounts_[0], reference[0].discounts_[0]);
    auto clone = model->Clone();
    clone->GeneratePath(gaussian, &reference);
    ASSERT_DOUBLE_EQ(path[0].observations_[0], reference[0].observations_[0]);
    ASSERT_DOUBLE_EQ(path[0].discounts_[0], reference[0].discounts_[0]);
}

TEST(ModelTest, TestGsrCopiesOwnTheirParameters) {
    const Date_ today(2026, 10, 3);
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, today.AddDays(365)}, {0.0, -0.03}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.02}, {today}, {1.0}));
    const GSRModelData_ data("model", curve, vol);
    AAD::GSR_<> original(data), copied(original), assigned(data);
    assigned = original;
    for (auto* copy : {&copied, &assigned}) {
        ASSERT_NE(copy->Parameters()[0], original.Parameters()[0]);
        *copy->Parameters()[0] = -0.1;
        ASSERT_NEAR(copy->InitialLogDiscount(1.0), -0.1, 1e-12);
        ASSERT_NEAR(original.InitialLogDiscount(1.0), -0.03, 1e-12);
    }
}

TEST(ModelTest, TestGsrZeroVolatilityRepricesInitialCurve) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Date_ twoYears(2028, 9, 28);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    const Handle_<ModelData_> data(new GSRModelData_("gsr", curve, vol));
    auto model = CreateModel<double>(data);
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> defs(2);
    model->Allocate(timeline, defs);
    model->Init(timeline, defs);
    AAD::Scenario_<> path;
    AAD::AllocatePath(defs, path);
    model->GeneratePath(Vector_<>(model->SimDim(), 0.3), &path);
    ASSERT_NEAR(path[0].numeraire_, 1.0, 1e-12);
    ASSERT_NEAR(path[1].numeraire_, std::exp(0.03), 1e-10);
}

TEST(ModelTest, TestGsrRateIndexNamesParseToExistingCanonicalNames) {
    ASSERT_EQ(Index::Parse("IR[USD,DF,2028-09-28]")->Name(), String_("IR[DF]:USD,2028-09-28"));
    ASSERT_EQ(Index::Parse("IR[USD,LIBOR_3M_LCH]")->Name(), String_("IR:USD,LIBOR_3M_LCH"));
    ASSERT_EQ(Index::Parse("IR[USD,SWAP,5Y]")->Name(), String_("IR:USD,5Y"));
}

TEST(ModelTest, TestGsrScriptBondPayoffRepricesInitialCurve) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Date_ twoYears(2028, 9, 28);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    const Handle_<ModelData_> model(new GSRModelData_("gsr", curve, vol));
    const Script::ScriptProductData_ product("bond", {Cell_(oneYear)}, {"pay PAYS FIX(IR[USD,DF,2028-09-28])"});
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    const auto results = Script::MCSimulation<double>(product, model, 1, valuation);
    ASSERT_NEAR(results.aggregated_, std::exp(-0.06), 1e-10);
}

TEST(ModelTest, TestGsrBermudanBondOptionUsesStochasticDiscounting) {
    const Date_ today(2026, 9, 28);
    const Date_ first(2027, 3, 28);
    const Date_ second(2027, 9, 28);
    const Date_ maturity(2028, 9, 28);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, first, second, maturity}, {0.0, -0.015, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.02}, {today}, {1.0}));
    const Handle_<ModelData_> model(new GSRModelData_("gsr", curve, vol));
    Script::ScriptProductSettings_ contract;
    contract.regressionFeatures_ = {"IR[USD,DF,2028-09-28]"};
    const Script::ScriptProductData_ product(
        "bond_option", {Cell_(first), Cell_(second)},
        {"EXERCISE MAX(FIX(IR[USD,DF,2028-09-28]) - 0.94, 0)", "EXERCISE MAX(FIX(IR[USD,DF,2028-09-28]) - 0.94, 0)"}, contract);
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    Script::MonteCarloSettings_ simulation;
    simulation.lsmcTrainingPaths_ = 128;
    const auto results = Script::MCSimulation<double>(product, model, 128, valuation, simulation);
    ASSERT_TRUE(std::isfinite(results.aggregated_));
    ASSERT_GT(results.aggregated_, 0.0);

    const Script::ScriptProductData_ parBond("par_bond", {Cell_(first), Cell_(second)},
                                             {"EXERCISE FIX(IR[USD,DF,2028-09-28])", "EXERCISE FIX(IR[USD,DF,2028-09-28])"}, contract);
    simulation.lsmcTrainingPaths_ = 4096;
    const auto parPrice = Script::MCSimulation<double>(parBond, model, 8192, valuation, simulation).aggregated_ / 8192.0;
    ASSERT_NEAR(parPrice, std::exp(-0.06), 3e-3);

    simulation.enableAad_ = true;
    const auto aad = Script::MCSimulation<AAD::Number_>(product, model, 512, valuation, simulation);
    ASSERT_TRUE(std::isfinite(aad.aggregated_));
    for (const double risk : aad.risks_)
        ASSERT_TRUE(std::isfinite(risk));
}

TEST(ModelTest, TestGsrConditionalBondAndDiscountStepMatchGaussianFormula) {
    const Date_ today(2025, 1, 1);
    const Date_ oneYear(2026, 1, 1);
    const Date_ twoYears(2027, 1, 1);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    auto model = CreateModel<double>(Handle_<ModelData_>(new GSRModelData_("gsr", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,DF,2027-01-01]"};
    model->Allocate({0.0, 1.0}, defs);
    model->Init({0.0, 1.0}, defs);
    AAD::Scenario_<> path;
    AAD::AllocatePath(defs, path);
    model->GeneratePath({0.0}, &path);
    ASSERT_NEAR(path[1].numeraire_, std::exp(0.03 + 0.0000125), 1e-12);
    ASSERT_NEAR(path[1].observations_[0], std::exp(-0.03 - 0.0001), 1e-12);
}

TEST(ModelTest, TestGsrDiscountedBondsAreMartingalesOnEventGrid) {
    const Date_ today(2025, 1, 1);
    const Date_ oneYear(2026, 1, 1);
    const Date_ twoYears(2027, 1, 1);
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.02}, {today}, {1.0}));
    auto model = CreateModel<double>(Handle_<ModelData_>(new GSRModelData_("gsr", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,DF,2027-01-01]"};
    model->Allocate({0.0, 1.0}, defs);
    model->Init({0.0, 1.0}, defs);
    double discountedBond = 0.0;
    double discount = 0.0;
    const double nodes[] = {0.0, std::sqrt(3.0), -std::sqrt(3.0)};
    const double weights[] = {2.0 / 3.0, 1.0 / 6.0, 1.0 / 6.0};
    for (size_t i = 0; i < 3; ++i) {
        AAD::Scenario_<> path;
        AAD::AllocatePath(defs, path);
        model->GeneratePath({nodes[i]}, &path);
        discountedBond += weights[i] * path[1].observations_[0] / path[1].numeraire_;
        discount += weights[i] / path[1].numeraire_;
    }
    ASSERT_NEAR(discount, std::exp(-0.03), 1e-10);
    ASSERT_NEAR(discountedBond, std::exp(-0.06), 1e-10);
}

TEST(ModelTest, TestGsrNumeraireCarriesCurveNodeAdjoint) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, oneYear}, {0.0, -0.03}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    const TapeGuard_ guard(AAD::Tape());
    auto model = CreateModel<AAD::Number_>(Handle_<ModelData_>(new GSRModelData_("gsr", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    model->Allocate({0.0, 1.0}, defs);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(defs, path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init({0.0, 1.0}, defs);
    model->GeneratePath({0.0}, &path);
    AAD::Number_ payoff = 1.0 / path[1].numeraire_;
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Value(payoff), std::exp(-0.03), 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[0]), std::exp(-0.03), 1e-10);
}

TEST(ModelTest, TestGsrProjectionLiborAndSwapUseRateCurve) {
    const Date_ today(2026, 9, 28);
    const Date_ observation(2027, 9, 28);
    const Date_ horizon(2037, 9, 28);
    const double years = (horizon - today) / DAYS_PER_YEAR;
    Matrix_<> projection(1, 3, 0.0);
    projection(0, 1) = -0.03;
    projection(0, 2) = -0.03 * years;
    const Handle_<GSRCurveData_> curve(
        new GSRCurveData_("curve", today, "USD", {today, observation, horizon}, {0.0, -0.02, -0.02 * years}, {"3M"}, projection));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    auto model = CreateModel<double>(Handle_<ModelData_>(new GSRModelData_("gsr", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,LIBOR_3M_LCH]", "IR[USD,SWAP,5Y]"};
    model->Allocate({0.0, 1.0}, defs);
    model->Init({0.0, 1.0}, defs);
    AAD::Scenario_<> path;
    AAD::AllocatePath(defs, path);
    model->GeneratePath({0.0}, &path);
    const auto index = Index::Parse("IR[USD,LIBOR_3M_LCH]");
    const auto* libor = dynamic_cast<const Index::Libor_*>(index.get());
    ASSERT_TRUE(libor);
    const Date_ start = libor->StartDate(DateTime_(observation, 0.0));
    const Date_ end = Date::NominalMaturity(start, libor->tenor_.Period(), libor->ccy_);
    const double accrual = Ccy::Conventions::LiborDayBasis()(libor->ccy_)(start, end, nullptr);
    ASSERT_NEAR(path[1].observations_[0], (std::exp(0.03 * (end - start) / DAYS_PER_YEAR) - 1.0) / accrual, 1e-10);
    ASSERT_GT(path[1].observations_[1], 0.02);
    ASSERT_LT(path[1].observations_[1], 0.04);
}

TEST(ModelTest, TestGsrBermudanCashSettledSwaptionUsesExistingLsmExercise) {
    const Date_ today(2026, 9, 28);
    const Date_ first(2027, 3, 28);
    const Date_ second(2027, 9, 28);
    const Date_ horizon(2034, 9, 28);
    const double years = (horizon - today) / DAYS_PER_YEAR;
    Matrix_<> projection(1, 2, 0.0);
    projection(0, 1) = -0.03 * years;
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, horizon}, {0.0, -0.02 * years}, {"3M"}, projection));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    const Handle_<ModelData_> model(new GSRModelData_("gsr", curve, vol));
    Script::ScriptProductSettings_ contract;
    contract.regressionFeatures_ = {"IR[USD,SWAP,5Y]"};
    const Script::ScriptProductData_ product("bermudan_swaption", {Cell_(first), Cell_(second)},
                                             {CashSettledSwaptionExercise(first, 0.025), CashSettledSwaptionExercise(second, 0.025)}, contract);
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    Script::MonteCarloSettings_ simulation;
    simulation.lsmcTrainingPaths_ = 512;
    const double tree = Script::MCSimulation<double>(product, model, 512, valuation, simulation).aggregated_ / 512.0;
    simulation.compiled_ = true;
    const double compiled = Script::MCSimulation<double>(product, model, 512, valuation, simulation).aggregated_ / 512.0;
    ASSERT_TRUE(std::isfinite(tree));
    ASSERT_GT(tree, 0.0);
    ASSERT_NEAR(compiled, tree, 1e-10);
}

TEST(ModelTest, TestGsrRejectsInvalidTenorsDatesAndVolatility) {
    const Date_ today(2026, 9, 28);
    const Date_ year(2027, 9, 28);
    Matrix_<> duplicateProjection(2, 2, 0.0);
    ASSERT_THROW(GSRCurveData_("duplicate", today, "USD", {today, year}, {0.0, -0.03}, {"3M", "QUARTERLY"}, duplicateProjection), Exception_);
    ASSERT_THROW(GSRVolData_("negative", {today}, {-0.01}, {today}, {1.0}), Exception_);
    ASSERT_THROW(GSRVolData_("mean_reversion", {today}, {0.01}, {today}, {0.0}), Exception_);

    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, year}, {0.0, -0.03}, {}, Matrix_<>(0, 0)));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    const Handle_<ModelData_> data(new GSRModelData_("gsr", curve, vol));
    auto model = CreateModel<double>(data);
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,DF,2028-09-28]"};
    model->Allocate({0.0, 1.0}, defs);
    ASSERT_THROW(model->Init({0.0, 1.0}, defs), Exception_);

    const Script::ScriptProductData_ product("bond", {Cell_(year)}, {"pay PAYS 1"});
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today.AddDays(1);
    ASSERT_THROW(Script::MCSimulation<double>(product, data, 16, valuation), Exception_);
}

TEST(ModelTest, TestGsrModelDataRoundTripsCurveAndVolatility) {
    const Date_ today(2026, 9, 28);
    const Date_ year(2027, 9, 28);
    Matrix_<> projection(1, 2, 0.0);
    projection(0, 1) = -0.04;
    const Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today, "USD", {today, year}, {0.0, -0.03}, {"3M"}, projection));
    const Handle_<GSRVolData_> vol(new GSRVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    const Handle_<GSRModelData_> data(new GSRModelData_("gsr", curve, vol));
    ASSERT_EQ(curve->Type(), String_("GSRCurveData"));
    ASSERT_EQ(vol->Type(), String_("GSRVolData"));
    ASSERT_EQ(data->Type(), String_("GSRModelData"));
    const auto restored = handle_cast<GSRModelData_>(JSON::ReadString(JSON::WriteString(*data), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->curve_->evaluationDate_, today);
    ASSERT_EQ(restored->curve_->currency_, String_("USD"));
    ASSERT_EQ(restored->curve_->projectionTenors_[0], String_("3M"));
    ASSERT_DOUBLE_EQ(restored->curve_->projectionLogDF_(0, 1), -0.04);
    ASSERT_DOUBLE_EQ(restored->vol_->gValues_[0], 0.01);
    ASSERT_DOUBLE_EQ(restored->vol_->hValues_[0], 1.0);
}
