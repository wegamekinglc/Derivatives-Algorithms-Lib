#pragma once

#include <algorithm>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/profiling.hpp>

namespace TapePerfComparison {
    using Phase = Dal::AAD::AADProfilingPhase_::Value_;
    using Span_ = Dal::AAD::ProfilingSpan_;

    struct Counts_ {
        size_t recordings_ = 0, sweeps_ = 0, statements_ = 0, edges_ = 0;
        size_t statementVisits_ = 0, edgeSlots_ = 0, maxBytes_ = 0;

        void Add(size_t sweeps, size_t statements, size_t edges, size_t bytes) {
            ++recordings_;
            sweeps_ += sweeps;
            statements_ += statements;
            edges_ += edges;
            statementVisits_ += sweeps * statements;
            edgeSlots_ += sweeps * edges;
            maxBytes_ = std::max(maxBytes_, bytes);
        }
    };

    inline void CountDal(Counts_& counts, size_t sweeps) {
        const auto tape = Dal::AAD::MeasureTape(*Dal::AAD::Tape());
        counts.Add(sweeps, tape.nodes_, tape.edges_, tape.capacityBytes_);
    }

    struct FirstOrderResult_ {
        double price_;
        Dal::Vector_<> gradient_;
    };

    template <class K_> FirstOrderResult_ DalFirst(const K_& kernel, const Dal::Vector_<>& point, Counts_* counts = nullptr, int sweeps = 1) {
        using namespace Dal::AAD;
        Span_ setup(Phase::PREPARE);
        RecordingScope_ recording;
        Dal::Vector_<Number_> inputs(point.size());
        for (size_t i = 0; i < point.size(); ++i)
            recording.RegisterInput(inputs[i], point[i]);
        recording.StartRecording();
        setup.Finish();
        Span_ price(Phase::PATH_FORWARD);
        auto output = kernel(&recording, inputs);
        price.Finish();
        recording.FinishRecording();
        if (counts)
            CountDal(*counts, sweeps);
        for (int pass = 0; pass < sweeps; ++pass) {
            Span_ seed(Phase::PREPARE);
            recording.ClearAdjoints();
            NativeOperations_::SetSeed(output, 1.0);
            seed.Finish();
            const Span_ reverse(Phase::REVERSE);
            recording.Reverse();
        }
        Span_ pack(Phase::REDUCE);
        FirstOrderResult_ result{Value(output), Dal::Vector_<>(point.size())};
        for (size_t i = 0; i < point.size(); ++i)
            result.gradient_[i] = NativeOperations_::ReadAdjoint(inputs[i]);
        pack.Finish();
        const Span_ cleanup(Phase::PREPARE);
        recording.Close();
        return result;
    }
} // namespace TapePerfComparison
