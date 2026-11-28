#include <algorithm>
#include <atomic>
#include <cfenv>
#include <climits>
#include <string>

#include <dal/math/aad/detail/secondorderreplay.hpp>
#include <dal/math/aad/secondorder.hpp>
#include <dal/math/aad/tape.hpp>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace Dal::AAD {
#include <dal/auto/MG_SecondOrderErrorReason_enum.inc>
#include <dal/auto/MG_SecondOrderStage_enum.inc>

    SecondOrderError_::SecondOrderError_(SecondOrderErrorReason_ reason,
                                         SecondOrderStage_ stage,
                                         std::optional<size_t> direction,
                                         const std::string& message)
        : Exception_(__FILE__, __LINE__, "EvaluateSecondOrder", message), reason_(reason), stage_(stage), direction_(direction) {}

    namespace SecondOrderDetail {
        thread_local Arena_* activeArena = nullptr;

        namespace {
            thread_local bool failCleanup = false;
            thread_local bool failResult = false;

            Arena_& RetainedArena() {
                thread_local Arena_ arena;
                return arena;
            }

            std::uint64_t NewArenaIdentity() {
                static std::atomic<std::uint64_t> next{1};
                auto id = next.load(std::memory_order_relaxed);
                while (id != 0) {
                    const auto successor = id == std::numeric_limits<std::uint64_t>::max() ? 0 : id + 1;
                    if (next.compare_exchange_weak(id, successor, std::memory_order_relaxed))
                        return id;
                }
                Error(Reason::CAPACITY_EXCEEDED, "arena identities exhausted");
            }

            size_t CheckedAdd(size_t a, size_t b) {
                if (b > std::numeric_limits<size_t>::max() - a)
                    Error(Reason::CAPACITY_EXCEEDED, "second-order byte count exceeds size_t");
                return a + b;
            }

            size_t CheckedMultiply(size_t a, size_t b) {
                if (b && a > std::numeric_limits<size_t>::max() / b)
                    Error(Reason::CAPACITY_EXCEEDED, "second-order byte count exceeds size_t");
                return a * b;
            }
        } // namespace

        void SetSecondOrderCleanupFailureForTest(bool fail) { failCleanup = fail; }
        void SetSecondOrderResultFailureForTest(bool fail) { failResult = fail; }
        size_t LastSecondOrderLogicalPeak() { return RetainedArena().logicalPeak_; }
        const char* LastSecondOrderProtection() { return RetainedArena().protection_; }

        [[noreturn]] void Access_::Fail(Reason reason, Stage stage, std::optional<size_t> direction, const std::string& message) {
            throw SecondOrderError_(reason, stage, direction, direction ? "direction " + std::to_string(*direction) + ": " + message : message);
        }

        [[noreturn]] void Error(Reason reason, const char* message) {
            Access_::Fail(reason, activeArena ? activeArena->stage_ : Stage::VALIDATION,
                          activeArena ? activeArena->direction_ : std::optional<size_t>{}, message);
        }

        [[noreturn]] void Promote(const char* constraint) {
            activeArena->retry_ = true;
            if (constraint)
                activeArena->protection_ = constraint;
            throw Retry_{};
        }

        void CheckLargeSum(double a, double b, double sum) {
            constexpr double RESIDUAL = 0x1p-24;
            if (std::abs(sum) > 0x1p48)
                Promote("narrow addition range");
            if ((sum == a && std::abs(b) > RESIDUAL) || (sum == b && std::abs(a) > RESIDUAL))
                Promote("narrow addition lost term");
        }

        void Arena_::Release() noexcept {
            statement_ = {};
            rhs_ = {};
            weight_ = {};
            adjoint_ = {};
            wideWeight_ = {};
            wideAdjoint_ = {};
            wideTangent_ = {};
            live_ = 0;
            edgeCapacity_ = admittedTemporary_ = 0;
        }

        void Arena_::BeginRequest(size_t cap) {
            if (poisoned_) {
                Release();
                id_ = 0;
                poisoned_ = false;
            }
            if (live_ > cap)
                Release();
            if (id_ == 0) {
                id_ = NewArenaIdentity();
                generation_ = 0;
            }
            cap_ = cap;
            peak_ = live_;
            logicalPeak_ = scratch_ = admittedTemporary_ = 0;
            stage_ = Stage::VALIDATION;
            protection_ = "";
            direction_.reset();
        }

        void Arena_::BeginDirection(bool wide) {
            if (generation_ == std::numeric_limits<decltype(generation_)>::max())
                Error(Reason::CAPACITY_EXCEEDED, "direction generations exhausted");
            ++generation_;
            slots_ = statements_ = edges_ = 0;
            wide_ = wide;
            edgeCapacity_ = std::min(rhs_.capacity_, wide ? wideWeight_.capacity_ : weight_.capacity_);
            retry_ = false;
            adjointsSeeded_ = false;
            liveDirection_ = true;
            stage_ = Stage::RECORDING;
            recordingFailure_ = nullptr;
        }

        [[noreturn]] void Arena_::BudgetFailure(size_t additional) {
            const auto request = CheckedAdd(CheckedAdd(live_, scratch_), additional);
            Access_::Fail(Reason::BUDGET_EXCEEDED, stage_, direction_,
                          "recording requires " + std::to_string(request) + " bytes with cap " + std::to_string(cap_));
        }

        size_t Arena_::Growth(size_t capacity, size_t needed, size_t width) {
            const size_t growth = capacity ? capacity + capacity / 2 : 64;
            const size_t next = std::min<size_t>(std::max(needed, growth), PASSIVE);
            Account(CheckedMultiply(next, width));
            return next;
        }

        [[noreturn]] void Arena_::RecordFailure() {
            recordingFailure_ = std::current_exception();
            std::rethrow_exception(recordingFailure_);
        }

        void Arena_::FinishRequest(bool failed) {
            stage_ = Stage::CLEANUP;
            direction_.reset();
            liveDirection_ = false;
            recordingFailure_ = nullptr;
            if (failCleanup) {
                poisoned_ = true;
                Error(Reason::LIFECYCLE, "second-order cleanup failed");
            }
            const size_t retain = logicalPeak_ <= 64 * 1024 ? 256 * 1024 : 8 * 1024 * 1024;
            if (failed || live_ > std::min(retain, cap_))
                Release();
        }

        namespace {
            // Re-record on an exceptional intermediate, using the source operations.
            // Preserve the caller's arithmetic controls and accumulated flags.
            class NarrowArithmetic_ {
#if defined(__SSE2__)
                unsigned saved_ = _mm_getcsr();
                static constexpr unsigned FAULTS = _MM_EXCEPT_INVALID | _MM_EXCEPT_DIV_ZERO | _MM_EXCEPT_OVERFLOW | _MM_EXCEPT_UNDERFLOW;
#else
                std::fenv_t saved_;
#endif

            public:
                NarrowArithmetic_() {
#if defined(__SSE2__)
                    _mm_setcsr((saved_ & ~_MM_EXCEPT_MASK) | _MM_MASK_MASK);
#else
                    std::feholdexcept(&saved_);
#endif
                }
                ~NarrowArithmetic_() {
#if defined(__SSE2__)
                    _mm_setcsr(saved_ | (_mm_getcsr() & _MM_EXCEPT_MASK));
#else
                    std::feupdateenv(&saved_);
#endif
                }
                void Check() const {
#if defined(__SSE2__)
                    if (_mm_getcsr() & FAULTS)
                        Promote("narrow floating-point exception");
#else
                    if (std::fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW))
                        Promote("narrow floating-point exception");
#endif
                }
            };

            struct ProtectedArithmetic_ {
                void Check() const {}
            };

            class ActiveArena_ {
            public:
                explicit ActiveArena_(Arena_& arena) { activeArena = &arena; }
                ~ActiveArena_() { activeArena = nullptr; }
                ActiveArena_(const ActiveArena_&) = delete;
                ActiveArena_& operator=(const ActiveArena_&) = delete;
            };

            template <class T_> SecondOrderNumber_ Input(Arena_& arena, double point, double seed) {
                const auto slot = arena.slots_;
                if constexpr (std::is_same_v<T_, Wide_>) {
                    arena.Ensure(arena.wideTangent_, slot, slot + 1);
                    arena.wideTangent_.Data()[slot] = Wide_(seed);
                }
                ++arena.slots_;
                return Access_::Bind(arena, slot, point, seed);
            }

            template <class T_> void Propagate(Weight_<T_>& destination, Weight_<T_> outgoing, Weight_<T_> weight) {
                destination.value_ = Add(destination.value_, Mul(outgoing.value_, weight.value_));
                destination.tangent_ = Add(Add(destination.tangent_, Mul(outgoing.tangent_, weight.value_)), Mul(outgoing.value_, weight.tangent_));
            }

#if defined(__SSE2__)
            void Propagate(Weight_<double>& destination, Weight_<double> outgoing, Weight_<double> weight) {
                const auto old = _mm_loadu_pd(&destination.value_);
                const auto primary = _mm_mul_pd(_mm_set_pd(outgoing.tangent_, outgoing.value_), _mm_set1_pd(weight.value_));
                const auto accumulated = _mm_add_pd(old, primary);
                const auto extra = _mm_set_sd(outgoing.value_ * weight.tangent_);
                const auto tangent = _mm_add_sd(_mm_unpackhi_pd(accumulated, accumulated), extra);
                const auto updated = _mm_unpacklo_pd(accumulated, tangent);
                const auto magnitude = _mm_castsi128_pd(_mm_set1_epi64x(std::numeric_limits<std::int64_t>::max()));
                const auto threshold = _mm_set1_pd(0x1p28);
                const auto checked =
                    _mm_or_pd(_mm_cmpgt_pd(_mm_and_pd(accumulated, magnitude), threshold), _mm_cmpgt_pd(_mm_and_pd(updated, magnitude), threshold));
                if (_mm_movemask_pd(checked)) {
                    Propagate<double>(destination, outgoing, weight);
                } else {
                    _mm_storeu_pd(&destination.value_, updated);
                }
            }
#endif

            template <class T_> void Reverse(Arena_& arena, Slot_ root) {
                auto& buffer = arena.Adjoints<T_>();
                arena.Ensure(buffer, 0, arena.slots_);
                arena.AccountLogical<T_>(true);
                auto* const adjoints = buffer.Data();
                if (!arena.adjointsSeeded_) {
                    std::fill_n(adjoints, arena.slots_, Weight_<T_>{T_(0), T_(0)});
                    if (root == PASSIVE)
                        return;
                    adjoints[root].value_ = T_(1);
                }
                const auto* const statements = arena.statement_.Data();
                const auto* const weights = arena.Weights<T_>().Data();
                const auto* const targets = arena.rhs_.Data();
                for (auto row = arena.statements_; row > 0; --row) {
                    const auto statement = statements[row - 1];
                    const auto outgoing = adjoints[statement.lhs_];
                    if (Zero(outgoing.value_) && Zero(outgoing.tangent_))
                        continue;
                    const Slot_ begin = row == 1 ? 0 : statements[row - 2].end_;
                    for (Slot_ edge = begin; edge < statement.end_; ++edge) {
                        Propagate(adjoints[targets[edge]], outgoing, weights[edge]);
                    }
                }
            }

            double Publish(double value) { return FinitePrimal(value); }

            double Publish(Wide_ value) {
                const long double total = value.Value();
                if (!std::isfinite(total) || std::abs(total) > std::numeric_limits<double>::max())
                    Error(Reason::NUMERICAL_FAILURE, "protected derivative exceeds the finite double range");
                return static_cast<double>(total);
            }

            template <class T_, class F_>
            double Record(Arena_& arena,
                          const F_& function,
                          const Vector_<>& point,
                          const Matrix_<>& directions,
                          int row,
                          DirectionOutput_ output,
                          std::optional<double>& recordedPrice) {
                arena.BeginDirection(std::is_same_v<T_, Wide_>);
                const std::conditional_t<std::is_same_v<T_, double>, NarrowArithmetic_, ProtectedArithmetic_> arithmetic;
                const Scratch_ inputStorage(arena, CheckedMultiply(point.size(), sizeof(SecondOrderNumber_)));
                Vector_<SecondOrderNumber_> inputs;
                inputs.reserve(point.size());
                bool zero = true;
                for (size_t i = 0; i < point.size(); ++i) {
                    const double seed = directions.Rows() ? directions(row, static_cast<int>(i)) : 0.0;
                    zero &= seed == 0.0;
                    inputs.push_back(Input<T_>(arena, point[i], seed));
                }
                ++output.work_.recordings_;
                ++output.work_.callbackEvaluations_;
                const auto price = function(inputs);
                if (arena.recordingFailure_)
                    std::rethrow_exception(arena.recordingFailure_);
                if (arena.retry_)
                    Promote();
                arithmetic.Check();
                const double value = FinitePrimal(Access_::Value(price));
                if (recordedPrice && *recordedPrice != value)
                    Error(Reason::NUMERICAL_FAILURE, "same-point price changed between recordings");
                recordedPrice = value;
                const Slot_ slot = Access_::Slot(price, arena);
                bool publishGradient = row == 0;
                if constexpr (std::is_same_v<F_, ReplayFunction_>) {
                    publishGradient = row == function.gradientRow_;
                    if constexpr (std::is_same_v<T_, double>)
                        if (!zero || publishGradient)
                            Promote("shared replay needs protected derivative arithmetic");
                }
                const T_ tangent = zero ? T_(0) : Access_::Tangent<T_>(price, arena, slot);
                arena.liveDirection_ = false;
                arena.stage_ = Stage::REVERSE;
                ++output.work_.reverseSweeps_;
                Reverse<T_>(arena, slot);
                arithmetic.Check();
                arena.stage_ = Stage::RESULT;
                if (directions.Rows())
                    output.directional_[row] = zero ? 0.0 : Publish(tangent);
                const auto* const adjoints = arena.Adjoints<T_>().Data();
                for (size_t i = 0; i < point.size(); ++i) {
                    const double gradient = Publish(adjoints[i].value_);
                    if (publishGradient)
                        output.gradient_[i] = gradient;
                    if (directions.Rows())
                        output.products_(row, static_cast<int>(i)) = zero ? 0.0 : Publish(adjoints[i].tangent_);
                }
                return value;
            }

            template <class F_>
            double RecordDirections(Arena_& arena, const F_& function, const Vector_<>& point, const Matrix_<>& directions,
                                    DirectionOutput_ output, std::optional<double> recordedPrice = {}) {
                double value = 0.0;
                for (int row = 0; row < std::max(1, directions.Rows()); ++row) {
                    arena.direction_ = directions.Rows() ? std::optional<size_t>(row) : std::nullopt;
                    try {
                        value = Record<double>(arena, function, point, directions, row, output, recordedPrice);
                    } catch (const Retry_&) {
                        ++output.work_.safePathActivations_;
                        value = Record<Wide_>(arena, function, point, directions, row, output, recordedPrice);
                    }
                }
                return value;
            }
        } // namespace

        double ReplayPrivately(ReplayFunction_ function, const Vector_<>& point, const Matrix_<>& directions, size_t cap,
                               DirectionOutput_ output, std::optional<double> recordedPrice) {
            // Use a protected row for the direction-independent gradient.
            // For an all-zero request, protect the first row itself.
            for (int row = 0; row < directions.Rows(); ++row) {
                const auto seeds = directions.Row(row);
                if (std::any_of(seeds.begin(), seeds.end(), [](double seed) { return seed != 0.0; })) {
                    function.gradientRow_ = row;
                    break;
                }
            }
            Arena_ arena;
            arena.BeginRequest(cap);
            const ActiveArena_ active(arena);
            double value;
            try {
                value = RecordDirections(arena, function, point, directions, output, recordedPrice);
            } catch (...) {
                try {
                    arena.FinishRequest(true);
                } catch (...) {
                    // The private arena is still destroyed on this exit.
                }
                throw;
            }
            arena.FinishRequest(false);
            output.work_.peakTapeBytes_ = arena.peak_;
            return value;
        }
    } // namespace SecondOrderDetail

    size_t SecondOrderPayloadBytes(size_t inputs, size_t directions) {
        using namespace SecondOrderDetail;
        if (inputs == 0)
            Access_::Fail(Reason::INVALID_INPUT, Stage::VALIDATION, {}, "point requires at least one coordinate");
        if (inputs > INT_MAX || directions > INT_MAX)
            Access_::Fail(Reason::CAPACITY_EXCEEDED, Stage::VALIDATION, {}, "point or directions exceeds matrix dimension range");
        const auto axis = CheckedMultiply(2, inputs);
        const auto matrix = CheckedMultiply(axis, directions);
        return CheckedMultiply(sizeof(double), CheckedAdd(CheckedAdd(CheckedAdd(1, axis), matrix), directions));
    }

    SecondOrderResult_ EvaluateSecondOrder(const SecondOrderFunction_& function,
                                           const Vector_<>& point,
                                           const Matrix_<>& directions,
                                           const SecondOrderOptions_& options) {
        using namespace SecondOrderDetail;
        const Exclusive_ exclusive;
        const auto* const legacy = NativeThread::retainedTape;
        if (legacy && (legacy->nodes_.OccupiedSlots() || legacy->HasReverseEventState()))
            Error(Reason::LIFECYCLE, "existing legacy tape must be empty");
        const auto bytes = SecondOrderPayloadBytes(point.size(), directions.Rows());
        if (!function || directions.Cols() != static_cast<int>(point.size()))
            Error(Reason::INVALID_INPUT, "callback and directions with one column per input are required");
        if (options.numericPayloadBudgetBytes_ && bytes > *options.numericPayloadBudgetBytes_)
            Access_::Fail(Reason::BUDGET_EXCEEDED, Stage::VALIDATION, {},
                          "numeric payload requires " + std::to_string(bytes) + " bytes with cap " +
                              std::to_string(*options.numericPayloadBudgetBytes_));
        for (size_t i = 0; i < point.size(); ++i)
            if (!std::isfinite(point[i]))
                Access_::Fail(Reason::INVALID_INPUT, Stage::VALIDATION, {}, "point[" + std::to_string(i) + "] must be finite");
        for (int row = 0; row < directions.Rows(); ++row)
            for (int col = 0; col < directions.Cols(); ++col)
                if (!std::isfinite(directions(row, col)))
                    Access_::Fail(Reason::INVALID_INPUT, Stage::VALIDATION, static_cast<size_t>(row),
                                  "directions(" + std::to_string(row) + "," + std::to_string(col) + ") must be finite");

        auto& arena = RetainedArena();
        const auto settings = options;
        arena.BeginRequest(settings.recordingCapacityBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        const ActiveArena_ active(arena);
        bool cleanupStarted = false;
        try {
            SecondOrderResult_ result;
            result.point_ = point;
            result.directions_ = directions;
            const SecondOrderFunction_ fixedFunction = function;
            if (failResult)
                throw std::bad_alloc{};
            result.gradient_.Resize(result.point_.size());
            result.directional_.Resize(result.directions_.Rows());
            result.products_.Resize(result.directions_.Rows(), result.directions_.Cols());
            result.execution_.numericPayloadBytes_ = bytes;
            DirectionOutput_ output{result.gradient_, result.directional_, result.products_, result.execution_};
            result.value_ = RecordDirections(arena, fixedFunction, result.point_, result.directions_, output);
            cleanupStarted = true;
            arena.FinishRequest(false);
            result.execution_.peakTapeBytes_ = arena.peak_;
            result.execution_.retainedTapeBytes_ = arena.live_;
            return result;
        } catch (...) {
            if (!cleanupStarted) {
                try {
                    arena.FinishRequest(true);
                } catch (...) {
                    // A poisoned arena is rebuilt before the next independent request.
                }
            }
            throw;
        }
    }
} // namespace Dal::AAD
