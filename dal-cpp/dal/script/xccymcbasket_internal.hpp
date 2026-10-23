#pragma once

#include <cstdint>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/script/xccymc_internal.hpp>
#include <dal/script/xccymcbasket.hpp>
#include <functional>

namespace Dal::XccyMcBasketInternal {
    constexpr size_t SCALAR_LIMIT = static_cast<size_t>(std::numeric_limits<uint64_t>::max());
    [[nodiscard]] size_t Add(size_t a, size_t b, size_t limit = SCALAR_LIMIT);
    [[nodiscard]] size_t Multiply(size_t a, size_t b, size_t limit = SCALAR_LIMIT);
    void Storage(size_t count, size_t width);
    void Capacity(size_t members, size_t labels, const XccyMcSettings_& settings);
    void ModelCapacity(const XccyGaussianModelData_& model);
    [[nodiscard]] String_ NativeMapIdentity(const XccyGaussianModelData_& model);
    [[nodiscard]] XccyMcBasketMemberAllocation_ MemberAllocation(const String_& id, const XccyMcInternal::Member_& member);
    [[nodiscard]] XccyMcBasketAllocation_ Allocate(Vector_<XccyMcBasketMemberAllocation_> members, const XccyMcSettings_& settings);
    void RequireAllocation(const XccyMcBasketAllocation_& allocation);
    [[nodiscard]] Vector_<> Row(const CalibrationAffineCoefficients_& coefficients);
    void JointStatistics(const Vector_<XccyMcInternal::Member_>& members, XccyMcBasketSnapshot_* result);
    struct Producer_ {
        [[nodiscard]] static Handle_<XccyMcBasketPartial_> Partial(XccyMcBasketPartialSnapshot_ snapshot, Handle_<CalibrationReport_> report);
        [[nodiscard]] static XccyMcBasketResult_ Result(XccyMcBasketSnapshot_ snapshot, Handle_<CalibrationReport_> report);
    };
    using Hook_ = std::function<void(const String_&, size_t, size_t, size_t)>;
    struct TestAccess_ {
        [[nodiscard]] static XccyMcBasketResult_
        Evaluate(const PreparedXccyMcBasket_& prepared, const Handle_<XccyGaussianModelData_>& candidate, const Hook_& hook, bool reverse = false);
    };

    class ScalarNormals_ {
        std::unique_ptr<PseudoRandom_> rng_;
        size_t dimension_, cursor_, end_, pathsLeft_;

    public:
        ScalarNormals_(int seed, size_t dimension, const XccyMcRange_& range);
        [[nodiscard]] Vector_<> Next();
        [[nodiscard]] size_t Cursor() const { return cursor_; }
        void Finish() const;
    };
} // namespace Dal::XccyMcBasketInternal
