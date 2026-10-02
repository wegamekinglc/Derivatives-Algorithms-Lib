//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <cmath>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/model/gsrdata.hpp>

inline Dal::Handle_<Dal::GSRCurveData_> GSRBenchmarkCurve() {
    const Dal::Date_ today(2026, 10, 2);
    return Dal::Handle_<Dal::GSRCurveData_>(new Dal::GSRCurveData_("curve", today, "USD", {today, today.AddDays(365), today.AddDays(3650)},
                                                                   {0.0, -0.03, -0.3}, {}, Dal::Matrix_<>(0, 0)));
}

inline void RunGSRPathCase(Dal::AAD::Model_<double>* model, bool swap, const std::string& label) {
    using namespace Dal;
    const Vector_<> timeline{0.0, 90.0 / 365.0, 180.0 / 365.0, 270.0 / 365.0, 1.0};
    Vector_<AAD::SampleDef_> definitions(timeline.size());
    for (auto& definition : definitions)
        definition.indexNames_ = swap ? Vector_<String_>{"IR[USD,SWAP,5Y]", "IR[USD,LIBOR_3M_LCH]"} : Vector_<String_>{"IR[USD,DF,2031-10-02]"};
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    Vector_<Vector_<>> normals(128, Vector_<>(model->SimDim()));
    for (size_t i = 0; i < normals.size(); ++i)
        for (size_t j = 0; j < normals[i].size(); ++j)
            normals[i][j] = (static_cast<int>((i * 17 + j * 31) % 101) - 50) * 0.025;
    const size_t paths = swap ? 10000 : 100000;
    double checksum = 0.0;
    const auto result = Bench::Run(
        label,
        [&] {
            for (size_t i = 0; i < paths; ++i) {
                model->GeneratePath(normals[i % normals.size()], &path);
                checksum += AAD::Value(path.back().observations_[0] / path.back().numeraire_);
            }
        },
        1, 3);
    REQUIRE(std::isfinite(checksum), "invalid GSR benchmark checksum");
    Bench::DoNotOptimize(&checksum);
    Bench::Print(result);
}

void RunGSRPathCases();
