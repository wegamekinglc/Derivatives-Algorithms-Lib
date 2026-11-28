#pragma once

#include <dal/math/aad/secondorder.hpp>

namespace Dal::AAD {
    bool TryAcquireSecondOrderRecording();
    void ReleaseSecondOrderRecording() noexcept;

    namespace SecondOrderDetail {
        class Exclusive_ {
        public:
            Exclusive_() {
                if (!TryAcquireSecondOrderRecording())
                    Access_::Fail(Reason::LIFECYCLE, Stage::VALIDATION, {}, "nested independent recording is unsupported");
            }
            ~Exclusive_() { ReleaseSecondOrderRecording(); }
            Exclusive_(const Exclusive_&) = delete;
            Exclusive_& operator=(const Exclusive_&) = delete;
        };

        struct ReplayFunction_ {
            const void* object_;
            SecondOrderNumber_ (*call_)(const void*, const Vector_<SecondOrderNumber_>&);

            SecondOrderNumber_ operator()(const Vector_<SecondOrderNumber_>& point) const { return call_(object_, point); }
        };

        struct DirectionOutput_ {
            Vector_<>& gradient_;
            Vector_<>& directional_;
            Matrix_<>& products_;
            SecondOrderExecution_& work_;
        };

        // The caller holds the recording lock and owns all inputs and result buffers.
        double ReplayPrivately(const ReplayFunction_& function, const Vector_<>& point, const Matrix_<>& directions, size_t cap,
                               DirectionOutput_ output, std::optional<double> recordedPrice);
    } // namespace SecondOrderDetail
} // namespace Dal::AAD
