#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>

namespace Dal::AAD::SecondOrderDetail {
    using Reason = SecondOrderErrorReason_::Value_;
    using Stage = SecondOrderStage_::Value_;
    using Slot_ = std::uint32_t;
    constexpr Slot_ PASSIVE = std::numeric_limits<Slot_>::max();

    struct Retry_ {};
    [[noreturn]] void Promote(const char* constraint = nullptr);
    [[noreturn]] void Error(Reason reason, const char* message);
    void CheckLargeSum(double a, double b, double sum);

    // The two limbs retain absorbed terms through cancellation. The extended
    // exponent range also keeps the local derivative of erfc in the tail.
    struct Wide_ {
        long double hi_, lo_;

        Wide_() = default;
        Wide_(long double value) : hi_(value), lo_(0.0L) {}
        Wide_(long double hi, long double lo) : hi_(hi), lo_(lo) {}
        [[nodiscard]] long double Value() const { return hi_ + lo_; }
    };

    inline Wide_ AddWide(long double a, long double b) {
        const long double sum = a + b, fromB = sum - a;
        return {sum, (a - (sum - fromB)) + (b - fromB)};
    }

    inline Wide_ operator+(Wide_ a, Wide_ b) {
        const auto sum = AddWide(a.hi_, b.hi_);
        return AddWide(sum.hi_, sum.lo_ + (a.lo_ + b.lo_));
    }
    inline Wide_ operator-(Wide_ a) { return {-a.hi_, -a.lo_}; }
    inline Wide_ operator-(Wide_ a, Wide_ b) { return a + (-b); }
    inline Wide_ operator*(Wide_ a, Wide_ b) {
        const auto product = a.hi_ * b.hi_;
        const auto residual = std::fma(a.hi_, b.hi_, -product);
        return AddWide(product, residual + (a.hi_ * b.lo_ + a.lo_ * b.hi_) + a.lo_ * b.lo_);
    }
    inline Wide_ operator/(Wide_ a, Wide_ b) {
        const auto quotient = a.hi_ / b.hi_;
        const auto residual = a - b * Wide_(quotient);
        return AddWide(quotient, residual.Value() / b.hi_);
    }
    inline bool Zero(Wide_ a) { return a.hi_ == 0.0L && a.lo_ == 0.0L; }
    inline bool Zero(double a) { return a == 0.0; }

    inline double Guard(double value) {
        constexpr double HIGH = 0x1p48, LOW = 0x1p-900;
        const auto magnitude = std::abs(value);
        if (!(magnitude <= HIGH) || (magnitude != 0.0 && magnitude < LOW))
            Promote("narrow operand or weight range");
        return value;
    }

    inline double Add(double a, double b) {
        const auto s = a + b;
        if (std::abs(s) > 0x1p28)
            CheckLargeSum(a, b, s);
        return s;
    }

    inline double Mul(double a, double b) { return a * b; }

    inline Wide_ Add(Wide_ a, Wide_ b) { return a + b; }
    inline Wide_ Mul(Wide_ a, Wide_ b) { return a * b; }
    inline Wide_ Guard(Wide_ value) { return value; }

    template <class T_> struct Weight_ {
        T_ value_, tangent_;
    };

    template <class T_> Weight_<T_> Compose(Weight_<T_> outer, Weight_<T_> inner) {
        return {Mul(outer.value_, inner.value_), Add(Mul(outer.tangent_, inner.value_), Mul(outer.value_, inner.tangent_))};
    }

    struct Statement_ {
        Slot_ end_;
    };

    template <class T_> struct Block_ {
        std::unique_ptr<T_[]> data_;
        size_t capacity_ = 0;

        [[nodiscard]] T_* Data() const { return data_.get(); }
    };

    struct Arena_ {
        std::uint64_t id_ = 0;
        std::uint32_t generation_ = 0;
        Slot_ slots_ = 0, statements_ = 0, edges_ = 0;
        Block_<Statement_> statement_;
        Block_<Slot_> rhs_;
        Block_<Weight_<double>> weight_, adjoint_;
        Block_<Weight_<Wide_>> wideWeight_, wideAdjoint_;
        Block_<Wide_> wideTangent_;
        size_t edgeCapacity_ = 0;
        size_t cap_ = std::numeric_limits<size_t>::max(), live_ = 0, peak_ = 0, logicalPeak_ = 0, scratch_ = 0;
        size_t admittedTemporary_ = 0;
        bool wide_ = false, retry_ = false, liveDirection_ = false;
        bool poisoned_ = false;
        const char* protection_ = "";
        Stage stage_ = Stage::VALIDATION;
        std::optional<size_t> direction_;
        std::exception_ptr recordingFailure_;

        void BeginRequest(size_t cap);
        void BeginDirection(bool wide);
        void FinishRequest(bool failed);
        void Release() noexcept;
        [[noreturn]] void BudgetFailure(size_t additional);
        void Account(size_t additional) {
            const size_t already = live_ + scratch_;
            if (additional > cap_ - already)
                BudgetFailure(additional);
            peak_ = std::max(peak_, already + additional);
        }

        // Between changes to the buffers or persistent scratch, a smaller
        // temporary fits the same cap and cannot raise the request peak.
        void AccountTemporary(size_t bytes) {
            if (bytes <= admittedTemporary_)
                return;
            Account(bytes);
            admittedTemporary_ = bytes;
        }

        template <class T_> void AccountLogical(bool reversing = false, size_t temporary = 0) {
            const size_t slots = static_cast<size_t>(slots_);
            const size_t tangents = std::is_same_v<T_, Wide_> ? slots * sizeof(Wide_) : 0;
            const size_t adjoints = reversing ? slots * sizeof(Weight_<T_>) : 0;
            const size_t logical =
                tangents + adjoints + statements_ * sizeof(Statement_) + edges_ * (sizeof(Weight_<T_>) + sizeof(Slot_)) + scratch_ + temporary;
            logicalPeak_ = std::max(logicalPeak_, logical);
        }
        [[nodiscard]] size_t Growth(size_t capacity, size_t needed, size_t width);
        [[noreturn]] void RecordFailure();

        template <class T_>
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((noinline, cold))
#endif
        void GrowStatement(Slot_ end) {
            Ensure(statement_, statements_, statements_ + 1);
            Ensure(rhs_, edges_, end);
            auto& weights = Weights<T_>();
            Ensure(weights, edges_, end);
            edgeCapacity_ = std::min(rhs_.capacity_, weights.capacity_);
        }

        template <class T_> void Ensure(Block_<T_>& block, size_t used, size_t needed) {
            if (needed <= block.capacity_)
                return;
            const auto next = Growth(block.capacity_, needed, sizeof(T_));
            try {
                auto memory = std::unique_ptr<T_[]>(new T_[next]);
                if (used)
                    std::copy_n(block.Data(), used, memory.get());
                live_ += (next - block.capacity_) * sizeof(T_);
                admittedTemporary_ = 0;
                block.data_ = std::move(memory);
                block.capacity_ = next;
            } catch (...) {
                RecordFailure();
            }
        }

        template <class T_> Block_<Weight_<T_>>& Weights() {
            if constexpr (std::is_same_v<T_, double>)
                return weight_;
            else
                return wideWeight_;
        }

        template <class T_> Block_<Weight_<T_>>& Adjoints() {
            if constexpr (std::is_same_v<T_, double>)
                return adjoint_;
            else
                return wideAdjoint_;
        }
    };

#if defined(__ELF__) && (defined(__GNUC__) || defined(__clang__))
    extern thread_local Arena_* activeArena __attribute__((tls_model("initial-exec")));
#else
    extern thread_local Arena_* activeArena;
#endif
    void SetSecondOrderCleanupFailureForTest(bool fail);
    void SetSecondOrderResultFailureForTest(bool fail);
    [[nodiscard]] size_t LastSecondOrderLogicalPeak();
    [[nodiscard]] const char* LastSecondOrderProtection();

    struct Scratch_ {
        Arena_& arena_;
        size_t bytes_;

        Scratch_(Arena_& arena, size_t bytes) : arena_(arena), bytes_(bytes) {
            arena_.Account(bytes);
            arena_.scratch_ += bytes;
            arena_.admittedTemporary_ = 0;
        }
        ~Scratch_() {
            arena_.scratch_ -= bytes_;
            arena_.admittedTemporary_ = 0;
        }
    };

    struct Access_ {
        static double Value(const SecondOrderNumber_& value) { return value.value_; }
        static Slot_ Slot(const SecondOrderNumber_& value, const Arena_& arena) {
            if (value.arenaId_ == 0)
                return PASSIVE;
            if ((value.arenaId_ != arena.id_) | (value.generation_ != arena.generation_) | (value.slot_ >= arena.slots_))
                Error(Reason::LIFECYCLE, "second-order handle belongs to an inactive arena, direction or slot");
            return value.slot_;
        }
        static SecondOrderNumber_ Bind(Arena_& arena, Slot_ slot, double value, double tangent) {
            SecondOrderNumber_ result(value);
            result.tangent_ = tangent;
            result.arenaId_ = arena.id_;
            result.generation_ = arena.generation_;
            result.slot_ = slot;
            return result;
        }

        template <class T_> static T_ Tangent(const SecondOrderNumber_& number, const Arena_& arena, Slot_ slot) {
            if (slot == PASSIVE)
                return T_(0.0);
            if constexpr (std::is_same_v<T_, double>)
                return number.tangent_;
            else
                return arena.wideTangent_.Data()[slot];
        }

        [[noreturn]] static void Fail(Reason reason, Stage stage, std::optional<size_t> direction, const std::string& message);
    };
} // namespace Dal::AAD::SecondOrderDetail
