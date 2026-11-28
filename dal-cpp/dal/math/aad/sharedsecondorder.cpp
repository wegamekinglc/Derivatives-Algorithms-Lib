#include <array>
#include <atomic>
#include <cfenv>
#include <type_traits>

#if defined(__SSE2__)
#include <xmmintrin.h>
#endif

#if defined(__x86_64__) && defined(__ELF__) && defined(__has_attribute)
#if __has_attribute(target_clones)
// Dispatch once per sweep so the explicit product residual can use hardware FMA.
#define DAL_SHARED_FMA_SWEEP __attribute__((target_clones("fma", "default")))
#endif
#endif
#ifndef DAL_SHARED_FMA_SWEEP
#define DAL_SHARED_FMA_SWEEP
#endif

#include <dal/math/aad/detail/secondorderreplay.hpp>
#include <dal/math/aad/sharedsecondorder.hpp>
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD::SharedSecondOrderDetail {
    namespace {
        struct Retry_ {};
        thread_local bool failCleanup = false;

        struct Twofold_ {
            double high_, low_;
        };

        FORCE_INLINE Twofold_ Normalize(double high, double low) {
            const double rounded = high + low;
            return {rounded, (high - rounded) + low};
        }

        template <bool Normalize_ = false> FORCE_INLINE Twofold_ Multiply(double weight, Twofold_ tangent) {
            const double high = weight * tangent.high_;
            const double low = std::fma(weight, tangent.low_, std::fma(weight, tangent.high_, -high));
            if constexpr (Normalize_)
                return Normalize(high, low);
            return {high, low};
        }

        FORCE_INLINE Twofold_ Multiply(Twofold_ left, Twofold_ right) {
            auto product = Multiply(left.high_, right);
            product.low_ = std::fma(left.low_, right.low_, std::fma(left.low_, right.high_, product.low_));
            return product;
        }

        FORCE_INLINE Twofold_ Add(Twofold_ left, Twofold_ right) {
            const double high = left.high_ + right.high_;
            if (Tape_::UnsafeSum(left.high_, right.high_, high))
                throw Retry_{};
            // TwoSum also retains the residual of a rounded addition.
            const double fromRight = high - left.high_;
            const double error = (left.high_ - (high - fromRight)) + (right.high_ - fromRight);
            return {high, error + (left.low_ + right.low_)};
        }

        FORCE_INLINE Twofold_ SumTangents(Twofold_ left, Twofold_ right) {
            const auto sum = Add(left, right);
            if (sum.low_ != 0.0 && std::abs(sum.high_) <= std::abs(sum.low_))
                throw Retry_{};
            return Normalize(sum.high_, sum.low_);
        }

        FORCE_INLINE Twofold_ NormalizeReverse(Twofold_ sum) {
            // Cancellation can expose the correction while a reverse slot
            // still has consumers. Normalize with its larger limb first.
            if (std::abs(sum.high_) < std::abs(sum.low_))
                return Normalize(sum.low_, sum.high_);
            return Normalize(sum.high_, sum.low_);
        }

        struct Lanes_ {
            double* high_;
            double* low_;

            FORCE_INLINE Twofold_ Read(size_t index) const { return {high_[index], low_[index]}; }
            FORCE_INLINE void Write(size_t index, Twofold_ value) const {
                high_[index] = value.high_;
                low_[index] = value.low_;
            }
            void Clear(size_t count) const {
                std::fill_n(high_, count, 0.0);
                std::fill_n(low_, count, 0.0);
            }
        };

        FORCE_INLINE void AccumulateAdjoint(const Lanes_& adjoint, size_t target, double weight, Twofold_ bar) {
            adjoint.Write(target, NormalizeReverse(Add(adjoint.Read(target), Multiply(weight, bar))));
        }

        FORCE_INLINE void Accumulate(const Lanes_& product, size_t target, double weight, Twofold_ dot, Twofold_ bar, Twofold_ local) {
            const bool propagated = weight != 0.0 && dot.high_ != 0.0;
            const bool curvature = bar.high_ != 0.0 && (local.high_ != 0.0 || local.low_ != 0.0);
            if (!propagated && !curvature)
                return;
            auto total = product.Read(target);
            if (propagated)
                total = Add(total, Multiply(weight, dot));
            if (curvature)
                total = Add(total, Multiply(bar, local));
            product.Write(target, NormalizeReverse(total));
        }

        Tape_& RetainedTape() {
            thread_local Tape_ tape;
            return tape;
        }

        [[noreturn]] void Fail(Reason reason, Stage stage, const char* message, std::optional<size_t> row = {}) {
            SecondOrderDetail::Access_::Fail(reason, stage, row, message);
        }

        std::uint64_t NewIdentity() {
            static std::atomic<std::uint64_t> next{1};
            auto id = next.load(std::memory_order_relaxed);
            while (id != std::numeric_limits<std::uint64_t>::max()) {
                if (next.compare_exchange_weak(id, id + 1, std::memory_order_relaxed))
                    return id;
            }
            Fail(Reason::CAPACITY_EXCEEDED, Stage::RECORDING, "shared graph identities exhausted");
        }

        template <class T_> void Grow(Tape_& tape, Buffer_<T_>& buffer, size_t used, size_t required, Stage stage) {
            if (required <= buffer.capacity_)
                return;
            const auto limit = std::numeric_limits<size_t>::max() / sizeof(T_);
            if (required > limit)
                Fail(Reason::CAPACITY_EXCEEDED, stage, "shared tape byte count exceeds size_t");
            // Leave room in the retained budget for compensated propagation.
            const auto increment = std::min(buffer.capacity_ / (std::is_same_v<T_, Node_> ? 3 : 2), limit - buffer.capacity_);
            const auto capacity = buffer.capacity_ ? std::max(required, buffer.capacity_ + increment) : std::max<size_t>(required, 64);
            tape.Admit(capacity * sizeof(T_), stage);
            std::unique_ptr<T_[]> next(new T_[capacity]);
            if (used)
                std::copy_n(buffer.values_.get(), used, next.get());
            buffer.values_ = std::move(next);
            buffer.capacity_ = capacity;
        }

        class ActiveGraph_ {
        public:
            explicit ActiveGraph_(Tape_& tape) { active = &tape; }
            ~ActiveGraph_() { active = nullptr; }
            ActiveGraph_(const ActiveGraph_&) = delete;
            ActiveGraph_& operator=(const ActiveGraph_&) = delete;
        };

        class Floating_ {
#if defined(__SSE2__)
            unsigned saved_ = _mm_getcsr();
            static constexpr unsigned FAULTS = _MM_EXCEPT_INVALID | _MM_EXCEPT_DIV_ZERO | _MM_EXCEPT_OVERFLOW | _MM_EXCEPT_UNDERFLOW;
#else
            std::fenv_t saved_;
#endif

        public:
            Floating_() {
#if defined(__SSE2__)
                _mm_setcsr((saved_ & ~_MM_EXCEPT_MASK) | _MM_MASK_MASK);
#else
                std::feholdexcept(&saved_);
#endif
            }
            ~Floating_() {
#if defined(__SSE2__)
                _mm_setcsr(saved_ | (_mm_getcsr() & _MM_EXCEPT_MASK));
#else
                std::feupdateenv(&saved_);
#endif
            }
            bool Faulted() const {
#if defined(__SSE2__)
                return (_mm_getcsr() & FAULTS) != 0;
#else
                return std::fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) != 0;
#endif
            }
            void Check(Tape_& tape) const {
                if (Faulted())
                    tape.Promote();
            }
        };
    } // namespace

    void SetCleanupFailureForTest(bool fail) { failCleanup = fail; }
    size_t LastLogicalPeak() { return RetainedTape().logical_; }

    [[noreturn]] void NoActiveGraph() { Fail(Reason::LIFECYCLE, Stage::RECORDING, "shared graph operation outside its callback"); }

    void Tape_::Release() noexcept {
        nodes_ = {};
        scratch_ = {};
        size_ = reservedInputs_ = 0;
        identity_ = 0;
    }

    void Tape_::Begin(size_t inputs, size_t cap) {
        if (inputs > std::numeric_limits<size_t>::max() / sizeof(Number_))
            Fail(Reason::CAPACITY_EXCEEDED, Stage::RECORDING, "shared graph input byte count exceeds size_t");
        const auto inputBytes = inputs * sizeof(Number_);
        const auto live = Retained();
        if (poisoned_ || live > cap || inputBytes > cap - live)
            Release();
        poisoned_ = promote_ = false;
        failure_ = nullptr;
        identity_ = NewIdentity();
        inputs_ = inputs;
        size_ = logical_ = 0;
        cap_ = cap;
        peak_ = Retained();
        reservedInputs_ = inputBytes;
        Admit(0, Stage::RECORDING);
    }

    void Tape_::Finish(bool failed) {
        identity_ = 0;
        reservedInputs_ = 0;
        failure_ = nullptr;
        if (failCleanup) {
            poisoned_ = true;
            Fail(Reason::LIFECYCLE, Stage::CLEANUP, "shared second-order cleanup failed");
        }
        constexpr size_t LARGE_RETENTION = 16 * 1024 * 1024;
        const auto retention = logical_ <= 64 * 1024 ? 256 * 1024 : LARGE_RETENTION;
        if (failed || Retained() > std::min(retention, cap_))
            Release();
    }

    void Tape_::Admit(size_t additional, Stage stage) {
        const auto live = Retained();
        if (live > cap_ || reservedInputs_ > cap_ - live || additional > cap_ - live - reservedInputs_)
            Fail(Reason::BUDGET_EXCEEDED, stage, "shared graph or propagation storage exceeds the tape cap");
        peak_ = std::max(peak_, live + reservedInputs_ + additional);
    }

    void Tape_::GrowNodes() {
        try {
            Grow(*this, nodes_, size_, size_ + 1, Stage::RECORDING);
        } catch (...) {
            if (!failure_)
                failure_ = std::current_exception();
            throw;
        }
    }

    void Tape_::GrowScratch(size_t required) { Grow(*this, scratch_, 0, required, Stage::REVERSE); }

    [[noreturn]] void Tape_::RecordingError(Reason reason, const char* message) {
        try {
            Fail(reason, Stage::RECORDING, message);
        } catch (...) {
            if (!failure_)
                failure_ = std::current_exception();
            throw;
        }
    }

    [[noreturn]] void Tape_::RethrowRecordingFailure() const { std::rethrow_exception(failure_); }

    [[noreturn]] void Tape_::Promote() {
        if (failure_)
            RethrowRecordingFailure();
        promote_ = true;
        throw Retry_{};
    }

    struct Request_ {
        Tape_& tape_;
        SharedSecondOrderResult_ result_;

        Request_(Tape_& tape, const Vector_<>& point, const Matrix_<>& directions, size_t payload) : tape_(tape) {
            result_.point_ = point;
            result_.directions_ = directions;
            result_.gradient_.Resize(point.size());
            result_.directional_.Resize(directions.Rows());
            result_.products_.Resize(directions.Rows(), directions.Cols());
            result_.execution_.numericPayloadBytes_ = payload;
        }

        template <size_t L_>
        FORCE_INLINE void Propagate(Slot_ root, int first, bool makeGradient, const Floating_& arithmetic);
        DAL_SHARED_FMA_SWEEP void PropagateSingle(Slot_ root, int first, const Floating_& arithmetic) {
            Propagate<1>(root, first, first == 0, arithmetic);
        }
        DAL_SHARED_FMA_SWEEP void PropagateBlock(Slot_ root, int first, const Floating_& arithmetic) {
            Propagate<3>(root, first, first == 0, arithmetic);
        }
        template <size_t L_> std::array<bool, L_> SeedTangents(int first, Lanes_ tangent);
        template <size_t L_>
        void
        PublishProducts(Slot_ root, int first, bool makeGradient, const std::array<bool, L_>& zero, Lanes_ gradient, Lanes_ tangent, Lanes_ product);
        template <size_t L_> void PublishDirections(Slot_ root, int first, const std::array<bool, L_>& zero, Lanes_ tangent);
        Operand_ RecordShared(const Kernel_& kernel, const Floating_& arithmetic);
        void Execute(const Kernel_& kernel);

        SharedSecondOrderResult_ Finish() {
            tape_.Finish(false);
            result_.execution_.retainedTapeBytes_ = tape_.Retained();
            return std::move(result_);
        }
    };

    template <size_t L_>
    FORCE_INLINE void ReverseBinary(const Node_& node, size_t out, size_t left, size_t right, Lanes_ tangent, Lanes_ product, Twofold_ bar) {
        const double drr = node.RightCurvature();
        // Keep occurrence order for repeated operands, including x*x.
        for (size_t lane = 0; lane < L_; ++lane) {
            const auto dot = product.Read(out * L_ + lane);
            const auto dl = Multiply(node.dlr_, tangent.Read(right + lane));
            auto dr = Multiply(node.dlr_, tangent.Read(left + lane));
            if (drr != 0.0)
                dr = NormalizeReverse(Add(dr, Multiply(drr, tangent.Read(right + lane))));
            Accumulate(product, left + lane, node.dl_, dot, bar, dl);
            Accumulate(product, right + lane, node.rightWeightOrCurvature_, dot, bar, dr);
        }
    }

    template <size_t L_> FORCE_INLINE void ForwardNodes(const Tape_& tape, Lanes_ tangent) {
        const auto* nodes = tape.nodes_.values_.get();
        for (size_t i = 0; i < tape.size_; ++i) {
            const auto& node = nodes[i];
            const auto left = static_cast<size_t>(node.left_) * L_, right = static_cast<size_t>(node.right_) * L_;
            const auto out = (1 + tape.inputs_ + i) * L_;
            if (node.right_) {
                for (size_t lane = 0; lane < L_; ++lane) {
                    const auto a = Multiply<true>(node.dl_, tangent.Read(left + lane));
                    const auto b = Multiply<true>(node.rightWeightOrCurvature_, tangent.Read(right + lane));
                    tangent.Write(out + lane, SumTangents(a, b));
                }
            } else {
                for (size_t lane = 0; lane < L_; ++lane) {
                    tangent.Write(out + lane, Multiply<true>(node.dl_, tangent.Read(left + lane)));
                }
            }
        }
    }

    template <size_t L_> FORCE_INLINE void ReverseUnary(const Node_& node, size_t out, size_t left, Lanes_ tangent, Lanes_ product, Twofold_ bar) {
        for (size_t lane = 0; lane < L_; ++lane) {
            const auto dot = product.Read(out * L_ + lane);
            const auto local = Multiply(node.rightWeightOrCurvature_, tangent.Read(left + lane));
            Accumulate(product, left + lane, node.dl_, dot, bar, local);
        }
    }

    template <size_t L_>
    FORCE_INLINE void
    ReverseNodes(const Tape_& tape, Slot_ root, bool makeGradient, size_t slots, size_t lanes, Lanes_ gradient, Lanes_ tangent, Lanes_ product) {
        const auto* nodes = tape.nodes_.values_.get();
        if (makeGradient) {
            gradient.Clear(slots);
            if (root)
                gradient.high_[root] = 1.0;
        }
        product.Clear(lanes);
        if (root) {
            for (size_t i = tape.size_; i > 0; --i) {
                const auto out = tape.inputs_ + i;
                const auto& node = nodes[i - 1];
                // The ordinary adjoint supplies the full curvature
                // coefficient even when a later block reuses the gradient.
                const auto bar = gradient.Read(out);
                const auto left = static_cast<size_t>(node.left_) * L_, right = static_cast<size_t>(node.right_) * L_;
                if (makeGradient)
                    AccumulateAdjoint(gradient, node.left_, node.dl_, bar);
                if (node.right_) {
                    if (makeGradient)
                        AccumulateAdjoint(gradient, node.right_, node.rightWeightOrCurvature_, bar);
                    ReverseBinary<L_>(node, out, left, right, tangent, product, bar);
                } else {
                    ReverseUnary<L_>(node, out, left, tangent, product, bar);
                }
            }
        }
    }

    template <size_t L_> FORCE_INLINE std::array<bool, L_> Request_::SeedTangents(int first, Lanes_ tangent) {
        auto& tape = tape_;
        std::array<bool, L_> zero;
        zero.fill(true);
        tangent.Clear((tape.inputs_ + 1) * L_);
        for (size_t i = 0; i < tape.inputs_; ++i) {
            for (size_t lane = 0; lane < L_; ++lane) {
                const double seed = result_.directions_(first + static_cast<int>(lane), static_cast<int>(i));
                tape.CheckNarrow(seed);
                tangent.high_[(i + 1) * L_ + lane] = seed;
                zero[lane] &= seed == 0.0;
            }
        }
        return zero;
    }

    template <size_t L_> FORCE_INLINE void Request_::PublishDirections(Slot_ root, int first, const std::array<bool, L_>& zero, Lanes_ tangent) {
        for (size_t lane = 0; lane < L_; ++lane) {
            const double direction = zero[lane] ? 0.0 : tangent.high_[static_cast<size_t>(root) * L_ + lane];
            if (!Tape_::IsNarrow(direction))
                throw Retry_{};
            result_.directional_[first + static_cast<int>(lane)] = direction;
        }
    }

    template <size_t L_>
    FORCE_INLINE void Request_::PublishProducts(
        Slot_ root, int first, bool makeGradient, const std::array<bool, L_>& zero, Lanes_ gradient, Lanes_ tangent, Lanes_ product) {
        const auto& tape = tape_;
        for (size_t i = 0; i < tape.inputs_; ++i) {
            if (makeGradient) {
                if (!Tape_::IsNarrow(gradient.high_[i + 1]))
                    throw Retry_{};
                result_.gradient_[i] = gradient.high_[i + 1];
            }
            for (size_t lane = 0; lane < L_; ++lane) {
                // The pair is normalized after every operation. Its high
                // limb is the rounded result; the input slot has no consumers.
                const double hvp = zero[lane] ? 0.0 : product.high_[(i + 1) * L_ + lane];
                if (!Tape_::IsNarrow(hvp))
                    throw Retry_{};
                result_.products_(first + static_cast<int>(lane), static_cast<int>(i)) = hvp;
            }
        }
        PublishDirections<L_>(root, first, zero, tangent);
    }

    template <size_t L_> void Request_::Propagate(Slot_ root, int first, bool makeGradient, const Floating_& arithmetic) {
        auto& tape = tape_;
        auto& work = result_.execution_;
        const auto slots = 1 + tape.inputs_ + tape.size_;
        auto* const scratch = tape.scratch_.values_.get();
        const Lanes_ gradient{scratch, scratch + slots};
        const auto lanes = slots * L_;
        const Lanes_ tangent{gradient.low_ + slots, gradient.low_ + slots + lanes};
        // A fan-out can read a tangent after other parents have already
        // contributed to the same slot's HVP. Both low limbs must stay live.
        const Lanes_ product{tangent.low_ + lanes, tangent.low_ + 2 * lanes};
        const auto zero = SeedTangents<L_>(first, tangent);
        ++work.sharedTangentSweeps_;
        ForwardNodes<L_>(tape, tangent);
        if (arithmetic.Faulted())
            throw Retry_{};
        ++work.sharedReverseSweeps_;
        ReverseNodes<L_>(tape, root, makeGradient, slots, lanes, gradient, tangent, product);
        if (arithmetic.Faulted())
            throw Retry_{};
        PublishProducts<L_>(root, first, makeGradient, zero, gradient, tangent, product);
    }

    Operand_ Request_::RecordShared(const Kernel_& kernel, const Floating_& arithmetic) {
        auto& work = result_.execution_;
        const ActiveGraph_ recording(tape_);
        ++work.sharedRecordings_;
        Vector_<Number_> inputs;
        inputs.reserve(tape_.inputs_);
        for (size_t i = 0; i < tape_.inputs_; ++i) {
            const double value = result_.point_[i];
            tape_.CheckNarrow(value);
            inputs.push_back(tape_.Bind(value, static_cast<Slot_>(i + 1)));
        }
        ++work.callbackEvaluations_;
        const auto price = kernel.shared_(kernel.object_, inputs);
        if (tape_.failure_)
            tape_.RethrowRecordingFailure();
        if (tape_.promote_)
            tape_.Promote();
        const auto root = tape_.Read(price);
        arithmetic.Check(tape_);
        return root;
    }

    void Request_::Execute(const Kernel_& kernel) {
        auto& work = result_.execution_;
        const auto& rows = result_.directions_;
        std::optional<double> trustedPrice;
        try {
            const Floating_ arithmetic;
            const auto root = RecordShared(kernel, arithmetic);
            trustedPrice = root.value_;
            result_.value_ = root.value_;
            const auto recordingBytes = tape_.size_ * sizeof(Node_) + tape_.reservedInputs_;
            tape_.reservedInputs_ = 0;
            const auto slots = 1 + tape_.inputs_ + tape_.size_;
            // Each slot has one ordinary adjoint pair, plus a tangent
            // pair and an HVP pair for each live direction.
            const size_t width = 2 + 4 * (rows.Rows() >= 3 ? 3 : 1);
            if (slots > std::numeric_limits<size_t>::max() / width)
                Fail(Reason::CAPACITY_EXCEEDED, Stage::REVERSE, "shared propagation slots exceed size_t");
            const auto scratch = width * slots;
            tape_.GrowScratch(scratch);
            tape_.logical_ = std::max(recordingBytes, tape_.size_ * sizeof(Node_) + scratch * sizeof(double));

            int row = 0;
            for (; row <= rows.Rows() - 3; row += 3)
                PropagateBlock(root.slot_, row, arithmetic);
            for (; row < rows.Rows(); ++row)
                PropagateSingle(root.slot_, row, arithmetic);
            work.peakTapeBytes_ = tape_.peak_;
        } catch (const Retry_&) {
            const auto sharedPeak = tape_.peak_;
            tape_.Release();
            SecondOrderExecution_ replay;
            work.replayDirections_ = rows.Rows();
            result_.value_ = SecondOrderDetail::ReplayPrivately(
                {kernel.object_, kernel.pointwise_}, result_.point_, rows, tape_.cap_,
                {result_.gradient_, result_.directional_, result_.products_, replay}, trustedPrice);
            work.callbackEvaluations_ += replay.callbackEvaluations_;
            work.pointwiseRecordings_ = replay.recordings_;
            work.pointwiseReverseSweeps_ = replay.reverseSweeps_;
            work.pointwiseSafePathActivations_ = replay.safePathActivations_;
            work.peakTapeBytes_ = std::max(sharedPeak, replay.peakTapeBytes_);
            work.cleanupReserveBytes_ = replay.cleanupReserveBytes_;
        }
    }

    void Run(Request_& request, const Kernel_& kernel) { request.Execute(kernel); }

    void ValidateSharedCoordinates(const Vector_<>& point, const Matrix_<>& directions) {
        for (double x : point)
            if (!std::isfinite(x))
                Fail(Reason::INVALID_INPUT, Stage::VALIDATION, "shared graph requires a finite point");
        for (int row = 0; row < directions.Rows(); ++row)
            for (int col = 0; col < directions.Cols(); ++col)
                if (!std::isfinite(directions(row, col)))
                    Fail(Reason::INVALID_INPUT, Stage::VALIDATION, "shared graph requires finite directions", static_cast<size_t>(row));
    }

    size_t ValidateSharedRequest(const Vector_<>& point, const Matrix_<>& directions, const SecondOrderOptions_& options) {
        const auto* const legacy = NativeThread::retainedTape;
        if (legacy && (legacy->nodes_.OccupiedSlots() || legacy->HasReverseEventState()))
            Fail(Reason::LIFECYCLE, Stage::VALIDATION, "existing legacy tape must be empty");
        const auto payload = SecondOrderPayloadBytes(point.size(), directions.Rows());
        if (directions.Rows() < 2 || directions.Cols() != static_cast<int>(point.size()))
            Fail(Reason::INVALID_INPUT, Stage::VALIDATION, "shared graph requires at least two directions and one column per input");
        if (options.numericPayloadBudgetBytes_ && payload > *options.numericPayloadBudgetBytes_)
            Fail(Reason::BUDGET_EXCEEDED, Stage::VALIDATION, "shared graph owning result exceeds the payload cap");
        ValidateSharedCoordinates(point, directions);
        return payload;
    }

    SharedSecondOrderResult_
    Evaluate(const void* kernel, WithKernel_ own, const Vector_<>& point, const Matrix_<>& directions, const SecondOrderOptions_& options) {
        const SecondOrderDetail::Exclusive_ exclusive;
        const auto payload = ValidateSharedRequest(point, directions, options);

        auto& tape = RetainedTape();
        bool cleanupStarted = false;
        try {
            tape.Begin(point.size(), options.recordingCapacityBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
            Request_ request(tape, point, directions, payload);
            own(kernel, request);
            cleanupStarted = true;
            return request.Finish();
        } catch (...) {
            if (!cleanupStarted)
                try {
                    tape.Finish(true);
                } catch (...) {
                    // Preserve the primary error. A poisoned graph is rebuilt on the next request.
                }
            throw;
        }
    }
} // namespace Dal::AAD::SharedSecondOrderDetail

#undef DAL_SHARED_FMA_SWEEP
