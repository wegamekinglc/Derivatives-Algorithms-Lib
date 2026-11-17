#pragma once

#include <chrono>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <rapidjson/istreamwrapper.h>

#include <dal/model/factory.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
#include <dal/script/simulation.hpp>

#include "capturecommon.hpp"
#include "mcprotocol.hpp"

namespace DalTest {
    using namespace Dal;
    using Script::MonteCarloSettings_;
    using Script::ScriptValuationSettings_;
    constexpr size_t CASES = 4, OUTPUTS = 20, WIDTH = CASES * OUTPUTS;

    const Json_& Find(const Json_& rows, const String_& id) {
        for (const auto& row : rows.GetArray())
            if (String_(row["id"].GetString()) == id)
                return row;
        THROW("Unknown qualification fixture/grid");
    }

    String_ Decimal(double value) {
        std::ostringstream result;
        result << std::setprecision(17) << value;
        return String_(result.str());
    }

    Vector_<String_> OutputNames() {
        Vector_<String_> result;
        for (const char* prefix : {"c", "p"})
            for (size_t i = 0; i < 5; ++i)
                result.push_back(String_(prefix) + String_(std::to_string(i)));
        result.Append(Vector_<String_>{"df", "forward"});
        for (size_t i = 0; i < 5; ++i)
            result.push_back("parity" + String_(std::to_string(i)));
        result.Append(Vector_<String_>{"mean", "fixed", "delayed"});
        return result;
    }

    String_ PayoffText(const Vector_<>& strikes) {
        String_ result;
        for (size_t i = 0; i < strikes.size(); ++i) {
            const String_ ordinal(std::to_string(i)), strike = Decimal(strikes[i]);
            result += "c" + ordinal + " PAYS MAX(spot()-" + strike + ",0) ";
            result += "p" + ordinal + " PAYS MAX(" + strike + "-spot(),0) ";
            result += "parity" + ordinal + "=c" + ordinal + "-p" + ordinal + " ";
        }
        return result + "df PAYS 1 forward PAYS spot() mean=spot() fixed PAYS 1 delayed PAYS 1 ON 2028-11-15";
    }

    Script::PreparedScript_ Prepared(const Handle_<ModelData_>& data, const String_& expression) {
        auto model = CreateModel<double>(data);
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 11, 16);
        MonteCarloSettings_ simulation;
        simulation.rsg_ = "mrg32";
        simulation.normalPrecision_ = "Precise";
        simulation.useBb_ = false;
        simulation.compiled_ = true;
        const Script::ScriptProductData_ product("qualification", {Cell_(Date_(2027, 11, 16))}, {expression});
        return Script::PrepareScript(product, model.get(), valuation, simulation);
    }

    struct Payoff_ {
        Script::PreparedScript_ product_;
        Vector_<size_t> slots_;
        explicit Payoff_(const Handle_<ModelData_>& data, const Vector_<>& strikes) : product_(Prepared(data, PayoffText(strikes))) {
            const auto names = OutputNames();
            for (const auto& name : names) {
                const auto& vars = product_.Product().VarNames();
                const auto found = std::find(vars.begin(), vars.end(), name);
                REQUIRE(found != vars.end(), "Missing qualification payoff output");
                slots_.push_back(static_cast<size_t>(found - vars.begin()));
            }
        }
    };

    struct Level_ {
        Vector_<> times_;
        Vector_<AAD::SampleDef_> definitions_;
        std::unique_ptr<AAD::Model_<double>> model_;
        AAD::Scenario_<> path_, event_;
        Vector_<size_t> samples_;
        std::vector<Script::EvalState_<double>> evaluators_;
        const std::vector<std::shared_ptr<const Payoff_>>& payoffs_;

        Level_(const Handle_<ModelData_>& data,
               Vector_<> times,
               const Vector_<>& maturities,
               const std::vector<std::shared_ptr<const Payoff_>>& payoffs)
            : times_(std::move(times)), definitions_(times_.size()), model_(CreateModel<double>(data)), payoffs_(payoffs) {
            for (size_t i = 0; i < maturities.size(); ++i) {
                const auto found = std::find(times_.begin(), times_.end(), maturities[i]);
                REQUIRE(found != times_.end(), "Qualification maturity absent from timeline");
                const size_t sample = static_cast<size_t>(found - times_.begin());
                samples_.push_back(sample);
                definitions_[sample] = payoffs[i]->product_.DefLine().front();
                definitions_[sample].discountMats_ = {2.25};
                evaluators_.push_back(payoffs[i]->product_.BuildEvalState<double>());
            }
            model_->Allocate(times_, definitions_);
            model_->Init(times_, definitions_);
            REQUIRE(model_->SimDim() == times_.size() - 1, "Production introduced a non-nested internal step");
            AAD::AllocatePath(definitions_, path_);
            AAD::InitializePath(path_);
            AAD::AllocatePath(Vector_<AAD::SampleDef_>{definitions_[samples_.front()]}, event_);
            AAD::InitializePath(event_);
        }

        void Values(const Vector_<>& normals, Vector_<>* values) {
            model_->GeneratePath(normals, &path_);
            values->Resize(WIDTH);
            for (size_t i = 0; i < CASES; ++i) {
                const auto& sample = path_[samples_[i]];
                event_[0].spot_ = sample.spot_;
                event_[0].numeraire_ = sample.numeraire_;
                event_[0].observations_ = sample.observations_;
                event_[0].discounts_ = sample.discounts_;
                payoffs_[i]->product_.CompiledProgram().Evaluate(event_, evaluators_[i]);
                for (size_t output = 0; output < OUTPUTS; ++output)
                    (*values)[i * OUTPUTS + output] = evaluators_[i].VarVals()[payoffs_[i]->slots_[output]];
            }
        }
    };

} // namespace DalTest
