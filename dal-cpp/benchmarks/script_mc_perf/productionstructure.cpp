//
// Created by Codex on 2026/10/15.
//

#include <iostream>
#include <optional>

#include <dal/math/aad/statistics.hpp>
#include <dal/platform/platform.hpp>

#include "productionstructure.hpp"

using namespace Dal;
using namespace Dal::Script;

namespace {
    void PrintStructure(const char* stage, size_t path, size_t inputs) {
        const auto& tape = *AAD::Tape();
        const auto statistics = AAD::MeasureTape(tape);
        REQUIRE(statistics.reverseEvents_ == 0, "structure leaf classification requires a scalar graph without reverse operators");
        size_t leaves = 0;
        auto node = tape.nodes_.Begin();
        for (size_t index = 0; index < statistics.nodes_; ++index) {
            const bool leaf = node->NumArguments() == 0;
            REQUIRE(index > inputs || leaf, "structure independent-root ordering differs");
            leaves += leaf;
            if (index + 1 < statistics.nodes_)
                ++node;
        }
        REQUIRE(leaves >= inputs + 1, "structure independent roots missing");
        std::cout << "{\"record\":\"structure\",\"stage\":\"" << stage << "\",\"path\":" << path << ",\"nodes\":" << statistics.nodes_
                  << ",\"edges\":" << statistics.edges_ << ",\"leaves\":" << leaves << ",\"risk_input_leaves\":" << inputs
                  << ",\"payoff_zero_leaves\":1,\"constant_leaf_candidates\":" << leaves - inputs - 1 << ",\"node_bytes\":" << sizeof(AAD::TapNode_)
                  << ",\"edge_bytes\":" << sizeof(double) + sizeof(double*) << ",\"tape_live_bytes\":" << statistics.liveBytes_
                  << ",\"tape_occupied_bytes\":" << statistics.occupiedBytes_ << ",\"tape_capacity_bytes\":" << statistics.capacityBytes_
                  << ",\"tape_blocks\":" << statistics.blocks_ << "}\n";
    }

    struct StructureCollector_ : Script::Detail::ScalarPayoffCollector_ {
        size_t inputs_;
        size_t* path_;

        StructureCollector_(size_t inputs, size_t* path) : inputs_(inputs), path_(path) {}

        AAD::Number_ operator()(const Vector_<AAD::Number_>& values, size_t payoffIndex, const AAD::Number_& zero) const {
            const auto result = Script::Detail::ScalarPayoffCollector_::operator()(values, payoffIndex, zero);
            PrintStructure("payoff", (*path_)++, inputs_);
            return result;
        }
    };
} // namespace

SimResults_ RunProductionStructure(const PreparedScript_& product, const Handle_<ModelData_>& data, size_t paths) {
    const auto model = CreateModel<double>(data);
    const auto names = Vector::Join(model->ParameterLabels(), product.ConstVarNames());
    const size_t inputs = names.size();
    Script::Detail::AADBatchSettings_ settings{"sobol",
                                               false,
                                               product.MaxNestedIfs(),
                                               product.Simulation().smooth_,
                                               paths,
                                               model->Parameters().size(),
                                               product.ConstVarNames().size(),
                                               product.PayOffIdx()};
    std::optional<ScriptCompiled_> compiled;
    if (product.Simulation().compiled_.value_or(false))
        compiled.emplace(product.Compile());
    SimResults_ result(names);
    size_t path = 0;
    Script::Detail::WithAADWorker(product, data, settings, compiled, [&](const auto& evaluate) {
        PrintStructure("prefix", 0, inputs);
        evaluate(PathBatch_{0, paths}, &result, StructureCollector_(inputs, &path), nullptr);
    });
    return result;
}
