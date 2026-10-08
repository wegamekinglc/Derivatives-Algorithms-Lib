//
// Created by Codex on 2026/10/09.
//

#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/structuraljacobiannative.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {
    namespace {
        NativeRecordingIdentity_ CurrentRecording(RecordingScope_* recording, bool reverse, const char* operation) {
            REQUIRE(recording != nullptr, String_(operation) + ": recording scope must not be null");
            try {
                const auto identity = NativeRecordedOperation_::AccuracyRecording(recording, reverse);
                NativeOperations_::ValidateAdjointMode(identity.multi_, identity.width_);
                return identity;
            } catch (const Exception_& error) {
                THROW(String_(operation) + ": " + String_(error.what()));
            }
        }

        bool SameRecording(const NativeRecordingIdentity_& current, const NativeRecordingIdentity_& bound) {
            return current.recording_ == bound.recording_ && current.multi_ == bound.multi_ && current.width_ == bound.width_;
        }

        void RequireIndependentGraph(Tape_* tape) {
            REQUIRE(tape->ReverseEventCount() == 0, "BindStructuralJacobianInputs: reverse events must be constructed after binding");
            tape->nodes_.ForEachLiveRange([](const TapNode_* first, const TapNode_* last) {
                for (auto* node = first; node != last; ++node)
                    REQUIRE(node->NumArguments() == 0, "BindStructuralJacobianInputs: graph computation must follow binding");
            });
        }

        const double* ValidSlot(const NativeInputSlots_& live, const Number_& number, const char* axis, size_t index) {
            try {
                NativeRecordedOperation_::ValidateInput(live, number);
                REQUIRE(std::isfinite(Value(number)), "value must be finite");
                static_cast<void>(NativeOperations_::ReadAdjoint(number));
                return &Adjoint(number);
            } catch (const Exception_& error) {
                THROW(String_("StructuralJacobian: ") + axis + "[" + String_(std::to_string(index)) + "]: " + String_(error.what()));
            }
        }

        Vector_<const double*> CaptureInputs(const NativeInputSlots_& live, const Vector_<Number_>& inputs) {
            Vector_<const double*> result;
            result.reserve(inputs.size());
            std::set<const double*> unique;
            for (const auto& input : inputs) {
                const auto* slot = ValidSlot(live, input, "input", result.size());
                REQUIRE(unique.insert(slot).second, "BindStructuralJacobianInputs: independent input slots must be distinct");
                result.push_back(slot);
            }
            return result;
        }

        void ValidateInputs(const NativeInputSlots_& live, const Vector_<Number_>& inputs, const Vector_<const double*>& slots) {
            for (size_t column = 0; column < inputs.size(); ++column)
                REQUIRE(ValidSlot(live, inputs[column], "input", column) == slots[column],
                        "ExecuteStructuralJacobian: input slot order or identity changed");
        }

        void ValidateOutputs(const NativeInputSlots_& live, const Vector_<Number_>& outputs) {
            for (size_t row = 0; row < outputs.size(); ++row)
                static_cast<void>(ValidSlot(live, outputs[row], "output", row));
        }

        void SeedColors(const StructuralJacobianPlan_& plan, const Vector_<Number_>& outputs, size_t first, size_t count) {
            for (size_t lane = 0; lane < count; ++lane)
                for (const auto row : plan.ColorRows(first + lane)) {
                    auto output = outputs[row];
                    NativeOperations_::AddSeed(output, 1.0, lane);
                }
        }

        void HarvestColors(const Vector_<Number_>& inputs, size_t first, size_t count, Matrix_<>* directions) {
            for (size_t lane = 0; lane < count; ++lane)
                for (int column = 0; column < directions->Cols(); ++column)
                    (*directions)(static_cast<int>(first + lane), column) = NativeOperations_::ReadAdjoint(inputs[column], lane);
        }
    } // namespace

    NativeStructuralInputs_ BindStructuralJacobianInputs(RecordingScope_* recording, const Vector_<Number_>& inputs) {
        const auto identity = CurrentRecording(recording, false, "BindStructuralJacobianInputs");
        RequireIndependentGraph(Tape());
        const NativeInputSlots_ live(Tape());
        return NativeStructuralInputs_(identity, CaptureInputs(live, inputs));
    }

    Matrix_<> ExecuteStructuralJacobian(RecordingScope_* recording,
                                        const NativeStructuralInputs_& bindings,
                                        const StructuralJacobianPlan_& plan,
                                        const Vector_<Number_>& inputs,
                                        const Vector_<Number_>& outputs) {
        const auto identity = CurrentRecording(recording, true, "ExecuteStructuralJacobian");
        REQUIRE(SameRecording(identity, bindings.identity_), "ExecuteStructuralJacobian: binding belongs to another recording or mode");
        REQUIRE(inputs.size() == bindings.Inputs() && inputs.size() == plan.Inputs(), "ExecuteStructuralJacobian: input counts must match");
        REQUIRE(outputs.size() == plan.Outputs(), "ExecuteStructuralJacobian: output count must match");
        const NativeInputSlots_ live(Tape());
        ValidateInputs(live, inputs, bindings.slots_);
        ValidateOutputs(live, outputs);
        Matrix_<> directions(static_cast<int>(plan.ColorCount()), static_cast<int>(plan.Inputs()), 0.0);
        for (size_t first = 0; first < plan.ColorCount(); first += identity.width_) {
            const auto count = std::min(identity.width_, plan.ColorCount() - first);
            recording->ClearAdjoints();
            SeedColors(plan, outputs, first, count);
            recording->Reverse();
            HarvestColors(inputs, first, count, &directions);
        }
        return RecoverStructuralJacobian(plan, directions);
    }
} // namespace Dal::AAD
