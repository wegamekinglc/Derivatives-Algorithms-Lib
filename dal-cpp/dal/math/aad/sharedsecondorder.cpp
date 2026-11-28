#include <array>
#include <atomic>
#include <cfenv>

#if defined(__SSE2__)
#include <xmmintrin.h>
#endif

#include <dal/math/aad/detail/secondorderreplay.hpp>
#include <dal/math/aad/sharedsecondorder.hpp>
#include <dal/math/aad/tape.hpp>

namespace Dal::AAD::SharedSecondOrderDetail {
    namespace {
        struct Retry_ {
            bool roundedTangentProduct_ = false;
        };
        thread_local bool failCleanup = false;

        FORCE_INLINE double TangentProduct(double weight, double tangent) {
            const double rounded = weight * tangent;
            // At the large-sum guard's scale, a rounded product can hide a
            // significant low part from a later cancellation, even on a
            // different node. Test the residual while both factors are live.
            if (std::abs(rounded) > 0x1p28 && std::isfinite(rounded) && std::fma(weight, tangent, -rounded) != 0.0)
                throw Retry_{true};
            return rounded;
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
            const auto increment = std::min(buffer.capacity_ / 2, limit - buffer.capacity_);
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
            void Check(Tape_& tape) const {
#if defined(__SSE2__)
                if (_mm_getcsr() & FAULTS)
#else
                if (std::fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW))
#endif
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
        void Propagate(Slot_ root, int first, bool makeGradient, const Floating_& arithmetic);
        void Execute(const Kernel_& kernel);

        SharedSecondOrderResult_ Finish() {
            tape_.Finish(false);
            result_.execution_.retainedTapeBytes_ = tape_.Retained();
            return std::move(result_);
        }
    };

    template <size_t L_>
    void Request_::Propagate(Slot_ root, int first, bool makeGradient, const Floating_& arithmetic) {
        auto& tape = tape_;
        auto& work = result_.execution_;
        const auto slots = 1 + tape.inputs_ + tape.size_;
        auto* const gradient = tape.scratch_.values_.get();
        auto* const tangent = gradient + slots;
        auto* const product = tangent + slots * L_;
        const Node_* nodes = tape.nodes_.values_.get();
        std::array<bool, L_> zero;
        zero.fill(true);
        std::fill_n(tangent, L_, 0.0);
        for (size_t i = 0; i < tape.inputs_; ++i) {
            for (size_t lane = 0; lane < L_; ++lane) {
                const double seed = result_.directions_(first + static_cast<int>(lane), static_cast<int>(i));
                tape.CheckNarrow(seed);
                tangent[(i + 1) * L_ + lane] = seed;
                zero[lane] &= seed == 0.0;
            }
        }
        ++work.sharedTangentSweeps_;
        for (size_t i = 0; i < tape.size_; ++i) {
            const auto& node = nodes[i];
            const auto left = static_cast<size_t>(node.left_) * L_, right = static_cast<size_t>(node.right_) * L_;
            const auto out = (1 + tape.inputs_ + i) * L_;
            if (node.right_) {
                for (size_t lane = 0; lane < L_; ++lane) {
                    const double a = TangentProduct(node.dl_, tangent[left + lane]);
                    const double b = TangentProduct(node.dr_, tangent[right + lane]);
                    tangent[out + lane] = tape.Add(a, b);
                }
            } else {
                for (size_t lane = 0; lane < L_; ++lane)
                    tangent[out + lane] = TangentProduct(node.dl_, tangent[left + lane]);
            }
        }
        arithmetic.Check(tape);
        ++work.sharedReverseSweeps_;
        if (makeGradient) {
            std::fill_n(gradient, slots, 0.0);
            if (root)
                gradient[root] = 1.0;
        }
        std::fill_n(product, slots * L_, 0.0);
        if (root) {
            for (size_t i = tape.size_; i > 0; --i) {
                const auto out = tape.inputs_ + i;
                const auto& node = nodes[i - 1];
                const double bar = gradient[out];
                const auto left = static_cast<size_t>(node.left_) * L_, right = static_cast<size_t>(node.right_) * L_;
                if (makeGradient)
                    gradient[node.left_] = tape.Add(gradient[node.left_], bar * node.dl_);
                if (node.right_) {
                    if (makeGradient)
                        gradient[node.right_] = tape.Add(gradient[node.right_], bar * node.dr_);
                    // Keep occurrence order for repeated operands, including x*x.
                    for (size_t lane = 0; lane < L_; ++lane) {
                        const double tl = tangent[left + lane], tr = tangent[right + lane], dot = product[out * L_ + lane];
                        const double dl = tape.Add(node.dll_ * tl, node.dlr_ * tr);
                        const double dr = tape.Add(node.dlr_ * tl, node.drr_ * tr);
                        product[left + lane] = tape.Add(tape.Add(product[left + lane], node.dl_ * dot), bar * dl);
                        product[right + lane] = tape.Add(tape.Add(product[right + lane], node.dr_ * dot), bar * dr);
                    }
                } else {
                    for (size_t lane = 0; lane < L_; ++lane) {
                        const double dot = product[out * L_ + lane];
                        const double local = node.dll_ * tangent[left + lane];
                        product[left + lane] = tape.Add(tape.Add(product[left + lane], node.dl_ * dot), bar * local);
                    }
                }
            }
        }
        arithmetic.Check(tape);
        for (size_t i = 0; i < tape.inputs_; ++i) {
            if (makeGradient) {
                tape.CheckNarrow(gradient[i + 1]);
                result_.gradient_[i] = gradient[i + 1];
            }
            for (size_t lane = 0; lane < L_; ++lane) {
                const double hvp = zero[lane] ? 0.0 : product[(i + 1) * L_ + lane];
                tape.CheckNarrow(hvp);
                result_.products_(first + static_cast<int>(lane), static_cast<int>(i)) = hvp;
            }
        }
        for (size_t lane = 0; lane < L_; ++lane) {
            const double direction = zero[lane] ? 0.0 : tangent[static_cast<size_t>(root) * L_ + lane];
            tape.CheckNarrow(direction);
            result_.directional_[first + static_cast<int>(lane)] = direction;
        }
    }

    void Request_::Execute(const Kernel_& kernel) {
        auto& work = result_.execution_;
        const auto& rows = result_.directions_;
        std::optional<double> trustedPrice;
        try {
            const Floating_ arithmetic;
            Operand_ root;
            {
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
                root = tape_.Read(price);
                arithmetic.Check(tape_);
            }
            trustedPrice = root.value_;
            result_.value_ = root.value_;
            const auto recordingBytes = tape_.size_ * sizeof(Node_) + tape_.reservedInputs_;
            tape_.reservedInputs_ = 0;
            const auto slots = 1 + tape_.inputs_ + tape_.size_;
            const size_t width = rows.Rows() >= 3 ? 7 : 3;
            if (slots > std::numeric_limits<size_t>::max() / width)
                Fail(Reason::CAPACITY_EXCEEDED, Stage::REVERSE, "shared propagation slots exceed size_t");
            const auto scratch = width * slots;
            tape_.GrowScratch(scratch);
            tape_.logical_ = std::max(recordingBytes, tape_.size_ * sizeof(Node_) + scratch * sizeof(double));

            int row = 0;
            for (; row <= rows.Rows() - 3; row += 3)
                Propagate<3>(root.slot_, row, row == 0, arithmetic);
            for (; row < rows.Rows(); ++row)
                Propagate<1>(root.slot_, row, row == 0, arithmetic);
            work.peakTapeBytes_ = tape_.peak_;
        } catch (const Retry_& retry) {
            const auto sharedPeak = tape_.peak_;
            tape_.Release();
            SecondOrderExecution_ replay;
            work.replayDirections_ = rows.Rows();
            result_.value_ = SecondOrderDetail::ReplayPrivately(
                {kernel.object_, kernel.pointwise_, retry.roundedTangentProduct_}, result_.point_, rows, tape_.cap_,
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

    SharedSecondOrderResult_ Evaluate(const void* kernel, WithKernel_ own, const Vector_<>& point, const Matrix_<>& directions,
                                      const SecondOrderOptions_& options) {
        const SecondOrderDetail::Exclusive_ exclusive;
        const auto* const legacy = NativeThread::retainedTape;
        if (legacy && (legacy->nodes_.OccupiedSlots() || legacy->HasReverseEventState()))
            Fail(Reason::LIFECYCLE, Stage::VALIDATION, "existing legacy tape must be empty");
        const auto payload = SecondOrderPayloadBytes(point.size(), directions.Rows());
        if (directions.Rows() < 2 || directions.Cols() != static_cast<int>(point.size()))
            Fail(Reason::INVALID_INPUT, Stage::VALIDATION, "shared graph requires at least two directions and one column per input");
        if (options.numericPayloadBudgetBytes_ && payload > *options.numericPayloadBudgetBytes_)
            Fail(Reason::BUDGET_EXCEEDED, Stage::VALIDATION, "shared graph owning result exceeds the payload cap");
        for (double x : point)
            if (!std::isfinite(x))
                Fail(Reason::INVALID_INPUT, Stage::VALIDATION, "shared graph requires a finite point");
        for (int row = 0; row < directions.Rows(); ++row)
            for (int col = 0; col < directions.Cols(); ++col)
                if (!std::isfinite(directions(row, col)))
                    Fail(Reason::INVALID_INPUT, Stage::VALIDATION, "shared graph requires finite directions", static_cast<size_t>(row));

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
