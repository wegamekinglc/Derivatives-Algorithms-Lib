//
// Created by Codex on 2026/10/09.
//

#pragma once

#include <utility>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/structuraljacobian.hpp>

namespace Dal::AAD {
    class NativeStructuralInputs_ {
        NativeRecordingIdentity_ identity_;
        Vector_<const double*> slots_;

        NativeStructuralInputs_(NativeRecordingIdentity_ identity, Vector_<const double*> slots) : identity_(identity), slots_(std::move(slots)) {}
        friend NativeStructuralInputs_ BindStructuralJacobianInputs(RecordingScope_*, const Vector_<Number_>&);
        friend Matrix_<> ExecuteStructuralJacobian(
            RecordingScope_*, const NativeStructuralInputs_&, const StructuralJacobianPlan_&, const Vector_<Number_>&, const Vector_<Number_>&);

    public:
        [[nodiscard]] size_t Inputs() const { return slots_.size(); }
        [[nodiscard]] bool VectorAdjoints() const { return identity_.multi_; }
        [[nodiscard]] size_t Width() const { return identity_.width_; }
    };

    [[nodiscard]] NativeStructuralInputs_ BindStructuralJacobianInputs(RecordingScope_* recording, const Vector_<Number_>& inputs);

    [[nodiscard]] Matrix_<> ExecuteStructuralJacobian(RecordingScope_* recording,
                                                      const NativeStructuralInputs_& bindings,
                                                      const StructuralJacobianPlan_& plan,
                                                      const Vector_<Number_>& inputs,
                                                      const Vector_<Number_>& outputs);
} // namespace Dal::AAD
