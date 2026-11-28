#pragma once

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace Dal::AAD::SharedSecondOrderDetail {
    using Reason = SecondOrderErrorReason_::Value_;
    using Stage = SecondOrderStage_::Value_;
    using Slot_ = std::uint32_t;
    using Number_ = SharedSecondOrderNumber_;

    struct Node_ {
        Slot_ left_, right_;
        double dl_, dr_, dll_, dlr_, drr_;
    };

    template <class T_> struct Buffer_ {
        std::unique_ptr<T_[]> values_;
        size_t capacity_ = 0;
        [[nodiscard]] size_t Bytes() const { return capacity_ * sizeof(T_); }
    };

    struct Operand_ {
        double value_;
        Slot_ slot_;
    };

    struct Tape_ {
        Buffer_<Node_> nodes_;
        Buffer_<double> scratch_;
        size_t inputs_ = 0, size_ = 0, peak_ = 0, cap_ = 0, reservedInputs_ = 0, logical_ = 0;
        std::uint64_t identity_ = 0;
        bool promote_ = false, poisoned_ = false;
        std::exception_ptr failure_;

        [[nodiscard]] size_t Retained() const { return nodes_.Bytes() + scratch_.Bytes(); }
        void Release() noexcept;
        void Begin(size_t inputs, size_t cap);
        void Finish(bool failed);
        void Admit(size_t additional, Stage stage);
        void GrowNodes();
        void GrowScratch(size_t required);
        [[noreturn]] void RecordingError(Reason reason, const char* message);
        [[noreturn]] void RethrowRecordingFailure() const;
        [[noreturn]] void Promote();

        [[nodiscard]] Number_ Bind(double value, Slot_ slot) const { return {value, slot, identity_}; }
        [[nodiscard]] Operand_ Read(const Number_& number);
        Number_ Append(double value, Node_ node);

        FORCE_INLINE void CheckNarrow(double value) {
            const double magnitude = std::abs(value);
            if (!(magnitude <= 0x1p48) || (magnitude != 0.0 && magnitude < 0x1p-900))
                Promote();
        }

        FORCE_INLINE double Add(double a, double b) {
            const double sum = a + b;
            if (std::abs(sum) > 0x1p28 &&
                (std::abs(sum) > 0x1p48 || (sum == a && std::abs(b) > 0x1p-24) || (sum == b && std::abs(a) > 0x1p-24)))
                Promote();
            return sum;
        }
    };

    inline thread_local Tape_* active = nullptr;
    [[noreturn]] void NoActiveGraph();

    FORCE_INLINE Tape_& Current() {
        auto* const tape = active;
        if (!tape)
            NoActiveGraph();
        if (tape->failure_)
            tape->RethrowRecordingFailure();
        return *tape;
    }

    FORCE_INLINE Operand_ Tape_::Read(const Number_& number) {
        const auto slot = number.slot_;
        if (slot) {
            if (number.identity_ != identity_ || slot > inputs_ + size_)
                RecordingError(Reason::LIFECYCLE, "stale shared graph handle");
        } else {
            if (!std::isfinite(number.value_))
                RecordingError(Reason::NUMERICAL_FAILURE, "nonfinite shared operand");
            CheckNarrow(number.value_);
        }
        return {number.value_, slot};
    }

    FORCE_INLINE Number_ Tape_::Append(double value, Node_ node) {
        CheckNarrow(value);
        if (!node.left_) {
            std::swap(node.left_, node.right_);
            std::swap(node.dl_, node.dr_);
            std::swap(node.dll_, node.drr_);
        }
        if (!node.left_)
            return Number_(value);
        CheckNarrow(node.dl_);
        CheckNarrow(node.dll_);
        if (node.right_) {
            CheckNarrow(node.dr_);
            CheckNarrow(node.dlr_);
            CheckNarrow(node.drr_);
        }
        if (inputs_ + size_ >= std::numeric_limits<Slot_>::max() - 1)
            RecordingError(Reason::CAPACITY_EXCEEDED, "shared graph slot limit exceeded");
        if (size_ == nodes_.capacity_)
            GrowNodes();
        nodes_.values_[size_++] = node;
        return Bind(value, static_cast<Slot_>(inputs_ + size_));
    }

    [[nodiscard]] size_t LastLogicalPeak();
    void SetCleanupFailureForTest(bool fail);
} // namespace Dal::AAD::SharedSecondOrderDetail

namespace Dal::AAD {
    FORCE_INLINE SharedSecondOrderNumber_ operator+(const SharedSecondOrderNumber_& a, const SharedSecondOrderNumber_& b) {
        auto& tape = SharedSecondOrderDetail::Current();
        const auto left = tape.Read(a), right = tape.Read(b);
        return tape.Append(tape.Add(left.value_, right.value_), {left.slot_, right.slot_, 1.0, 1.0, 0.0, 0.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ operator-(const SharedSecondOrderNumber_& a, const SharedSecondOrderNumber_& b) {
        auto& tape = SharedSecondOrderDetail::Current();
        const auto left = tape.Read(a), right = tape.Read(b);
        return tape.Append(tape.Add(left.value_, -right.value_), {left.slot_, right.slot_, 1.0, -1.0, 0.0, 0.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ operator-(const SharedSecondOrderNumber_& a) {
        auto& tape = SharedSecondOrderDetail::Current();
        const auto arg = tape.Read(a);
        return tape.Append(-arg.value_, {arg.slot_, 0, -1.0, 0.0, 0.0, 0.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ operator+(const SharedSecondOrderNumber_& a) {
        (void)SharedSecondOrderDetail::Current().Read(a);
        return a;
    }

    FORCE_INLINE SharedSecondOrderNumber_ operator*(const SharedSecondOrderNumber_& a, const SharedSecondOrderNumber_& b) {
        auto& tape = SharedSecondOrderDetail::Current();
        const auto left = tape.Read(a), right = tape.Read(b);
        return tape.Append(left.value_ * right.value_, {left.slot_, right.slot_, right.value_, left.value_, 0.0, 1.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ operator/(const SharedSecondOrderNumber_& a, const SharedSecondOrderNumber_& b) {
        using SharedSecondOrderDetail::Reason;
        auto& tape = SharedSecondOrderDetail::Current();
        const auto left = tape.Read(a), right = tape.Read(b);
        if (right.value_ == 0.0)
            tape.RecordingError(Reason::OUTSIDE_DOMAIN, "division by zero in shared graph");
        const double value = left.value_ / right.value_, inv = 1.0 / right.value_;
        if (!right.slot_)
            return tape.Append(value, {left.slot_, 0, inv, 0.0, 0.0, 0.0, 0.0});
        const double dr = -(value * inv);
        return tape.Append(value, {left.slot_, right.slot_, inv, dr, 0.0, -(inv * inv), -2.0 * (dr * inv)});
    }

#define DAL_SHARED_SECOND_ORDER_BINARY(OP)                                                                                                        \
    FORCE_INLINE SharedSecondOrderNumber_ operator OP(const SharedSecondOrderNumber_& a, double b) {                                              \
        return a OP SharedSecondOrderNumber_(b);                                                                                                  \
    }                                                                                                                                             \
    FORCE_INLINE SharedSecondOrderNumber_ operator OP(double a, const SharedSecondOrderNumber_& b) {                                             \
        return SharedSecondOrderNumber_(a) OP b;                                                                                                  \
    }                                                                                                                                             \
    inline SharedSecondOrderNumber_& SharedSecondOrderNumber_::operator OP##=(SharedSecondOrderNumber_ rhs) {                                     \
        return *this = *this OP rhs;                                                                                                              \
    }

    DAL_SHARED_SECOND_ORDER_BINARY(+)
    DAL_SHARED_SECOND_ORDER_BINARY(-)
    DAL_SHARED_SECOND_ORDER_BINARY(*)
    DAL_SHARED_SECOND_ORDER_BINARY(/)
#undef DAL_SHARED_SECOND_ORDER_BINARY

    FORCE_INLINE SharedSecondOrderNumber_ exp(const SharedSecondOrderNumber_& a) {
        auto& tape = SharedSecondOrderDetail::Current();
        const auto arg = tape.Read(a);
        const auto value = std::exp(arg.value_);
        if (value == 0.0)
            tape.Promote();
        return tape.Append(value, {arg.slot_, 0, value, 0.0, value, 0.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ log(const SharedSecondOrderNumber_& a) {
        using SharedSecondOrderDetail::Reason;
        auto& tape = SharedSecondOrderDetail::Current();
        const auto arg = tape.Read(a);
        if (!(arg.value_ > 0.0))
            tape.RecordingError(Reason::OUTSIDE_DOMAIN, "log requires a positive operand");
        const double inv = 1.0 / arg.value_;
        return tape.Append(std::log(arg.value_), {arg.slot_, 0, inv, 0.0, -(inv * inv), 0.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ sqrt(const SharedSecondOrderNumber_& a) {
        using SharedSecondOrderDetail::Reason;
        auto& tape = SharedSecondOrderDetail::Current();
        const auto arg = tape.Read(a);
        if (!(arg.value_ > 0.0))
            tape.RecordingError(Reason::OUTSIDE_DOMAIN, "sqrt requires a positive operand");
        const double value = std::sqrt(arg.value_), d = 0.5 / value;
        return tape.Append(value, {arg.slot_, 0, d, 0.0, -(d * (0.5 / arg.value_)), 0.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ erfc(const SharedSecondOrderNumber_& a) {
        auto& tape = SharedSecondOrderDetail::Current();
        const auto arg = tape.Read(a);
        const double d = -1.12837916709551257389615890312154517 * std::exp(-arg.value_ * arg.value_);
        if (d == 0.0)
            tape.Promote();
        return tape.Append(std::erfc(arg.value_), {arg.slot_, 0, d, 0.0, (-2.0 * arg.value_) * d, 0.0, 0.0});
    }

    FORCE_INLINE SharedSecondOrderNumber_ NCDF(const SharedSecondOrderNumber_& a) {
        auto& tape = SharedSecondOrderDetail::Current();
        const auto arg = tape.Read(a);
        const double d = 0.39894228040143267793994605993438187 * std::exp(-0.5 * arg.value_ * arg.value_);
        if (d == 0.0)
            tape.Promote();
        return tape.Append(0.5 * std::erfc(-0.7071067811865475244 * arg.value_), {arg.slot_, 0, d, 0.0, -arg.value_ * d, 0.0, 0.0});
    }
} // namespace Dal::AAD
