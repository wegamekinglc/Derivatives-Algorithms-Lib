//
// Created by dal-implementer on 2026/9/20.
//

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/simulation.hpp>
#include <dal/script/visitor/compiler.hpp>
#include <dal/script/visitor/fuzzy.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    namespace {
        //  N3: sample-size guard needs ten paths per basis function
        constexpr size_t PATHS_PER_BASIS_FUNCTION = 10;
        //  N3: sigma floor keeps the z-normalization finite on (legal) constant regressors
        constexpr double SIGMA_FLOOR_SCALE = 1e-12;
        //  N3: explicit relative ridge applied before CholeskySolve
        constexpr double RIDGE_LAMBDA = 1e-12;
        //  Bound both the Gram diagonal spread and the scaled Cholesky pivots.
        constexpr double CONDITION_LIMIT = 1e12;
        constexpr double QR_RANK_TOLERANCE = 1e-10;

        struct PathCounts_ {
            size_t training_;
            size_t validation_;
            size_t pricingOffset_;
            size_t replicates_;
        };

        PathCounts_ LsmcPathCounts(const MonteCarloSettings_& simulation, size_t nPaths) {
            const size_t training = simulation.lsmcTrainingPaths_ ? static_cast<size_t>(*simulation.lsmcTrainingPaths_) : nPaths;
            const size_t validation = simulation.lsmcValidationPaths_ ? static_cast<size_t>(*simulation.lsmcValidationPaths_) : 0;
            REQUIRE2(!simulation.lsmcRqmcReplicates_ || *simulation.lsmcRqmcReplicates_ >= 2,
                     "InvalidLsmcRqmcReplicates: expected at least 2 pricing replicates", ScriptError_);
            const size_t replicates = static_cast<size_t>(simulation.lsmcRqmcReplicates_.value_or(1));
            constexpr size_t MAX_SOBOL_PATHS = std::numeric_limits<uint32_t>::max();
            REQUIRE2(training <= MAX_SOBOL_PATHS && validation <= MAX_SOBOL_PATHS - training &&
                         nPaths <= (MAX_SOBOL_PATHS - training - validation) / replicates,
                     "InvalidPathCount: LSMC training, validation, and all pricing replicates exceed the 32-bit Sobol sequence", ScriptError_);
            return {training, validation, training + validation, replicates};
        }

        uint64_t LsmcScrambleKey(bool pricing, int seed, size_t replicate = 0) {
            return (static_cast<uint64_t>(pricing) << 63) | (static_cast<uint64_t>(seed) << 32) | static_cast<uint64_t>(replicate);
        }

        template <class E_> const E_* RowData(const Vector_<E_>& row) { return row.empty() ? nullptr : &row[0]; }

        //  One regression's rows; the backward phase regresses storage rows in place
        struct RegressionRows_ {
            const double* x_;
            const double* targets_;
            const char* included_;
            size_t n_;
        };

        RegressionRows_ Rows(const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included) {
            return {RowData(x), RowData(targets), RowData(included), x.size()};
        }

        RegressionRows_ SubRows(const RegressionRows_& rows, const PathBatch_& chunk) {
            return {rows.x_ + chunk.firstPath_, rows.targets_ + chunk.firstPath_, rows.included_ + chunk.firstPath_, chunk.pathCount_};
        }

        //  Fork-join team for the backward phase. A pool task group costs tens of
        //  microseconds per task on virtualised hosts, more than one chunk of a sweep, so
        //  helpers join once per phase and claim the chunks of each published pass through
        //  atomics. The caller claims chunks too and can finish every pass alone, so a busy
        //  pool only costs speed; it never waits on the pool while its team is active, so
        //  helpers cannot form a wait cycle. A helper leaves after MAX_HELPER_IDLE without
        //  work rather than hold a pool worker through a long sequential stretch.
        class ChunkTeam_ {
            using Work_ = std::function<void(size_t)>;
            static constexpr int PASS_SHIFT = 32;
            static constexpr uint64_t INDEX_MASK = (uint64_t(1) << PASS_SHIFT) - 1;
            static constexpr size_t SPINS_BEFORE_YIELD = 1 << 14;
            static constexpr std::chrono::microseconds MAX_HELPER_IDLE{1000};

            //  Pass p lives in slot p & 1: the caller publishes pass p + 2 only after every
            //  chunk of p + 1, and therefore of p, has finished
            struct Pass_ {
                std::atomic<size_t> chunkCount_{0};
                std::atomic<const Work_*> work_{nullptr};
            };

            std::array<Pass_, 2> passes_;
            std::atomic<uint64_t> claim_{0}; //  pass number << PASS_SHIFT | next chunk index
            std::atomic<size_t> done_{0};
            std::atomic<bool> finished_{false};
            std::atomic<bool> failed_{false};
            std::mutex failureMutex_;
            std::exception_ptr failure_;
            uint64_t pass_ = 0; //  caller only

            static void Pause(size_t* spins) {
                if (++*spins >= SPINS_BEFORE_YIELD)
                    std::this_thread::yield();
            }

            bool RunOneChunk() {
                uint64_t word = claim_.load(std::memory_order_acquire);
                while (true) {
                    const Pass_& pass = passes_[(word >> PASS_SHIFT) & 1];
                    const size_t index = static_cast<size_t>(word & INDEX_MASK);
                    const Work_* work = pass.work_.load(std::memory_order_relaxed);
                    if (!work || index >= pass.chunkCount_.load(std::memory_order_relaxed))
                        return false;
                    //  success means the pass was still current, so the slot read above was its own
                    if (claim_.compare_exchange_weak(word, word + 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        try {
                            (*work)(index);
                        } catch (...) {
                            std::lock_guard<std::mutex> lock(failureMutex_);
                            if (!failure_)
                                failure_ = std::current_exception();
                            failed_.store(true, std::memory_order_relaxed);
                        }
                        done_.fetch_add(1, std::memory_order_release);
                        return true;
                    }
                }
            }

        public:
            void Help() {
                size_t spins = 0;
                std::chrono::steady_clock::time_point idleSince;
                while (!finished_.load(std::memory_order_acquire)) {
                    if (RunOneChunk()) {
                        spins = 0;
                        continue;
                    }
                    if (++spins < SPINS_BEFORE_YIELD)
                        continue;
                    const auto now = std::chrono::steady_clock::now();
                    if (spins == SPINS_BEFORE_YIELD)
                        idleSince = now;
                    else if (now - idleSince > MAX_HELPER_IDLE)
                        return;
                    std::this_thread::yield();
                }
            }

            void Finish() { finished_.store(true, std::memory_order_release); }

            void Run(size_t chunkCount, const Work_& work) {
                REQUIRE(chunkCount <= INDEX_MASK, "InvalidPathCount: too many path chunks");
                ++pass_;
                Pass_& pass = passes_[pass_ & 1];
                pass.chunkCount_.store(chunkCount, std::memory_order_relaxed);
                pass.work_.store(&work, std::memory_order_relaxed);
                done_.store(0, std::memory_order_relaxed);
                claim_.store(pass_ << PASS_SHIFT, std::memory_order_release);
                while (RunOneChunk()) {
                }
                size_t spins = 0;
                while (done_.load(std::memory_order_acquire) != chunkCount)
                    Pause(&spins);
                if (failed_.load(std::memory_order_relaxed))
                    std::rethrow_exception(failure_);
            }
        };

        //  Joins the pool's free workers to a team for one scope; the team finishes before
        //  the helper tasks are drained, however the scope exits
        class ChunkTeamScope_ {
            ChunkTeam_ team_;
            SimulationTaskGroup_ helpers_;
            struct Finisher_ {
                ChunkTeam_* team_;
                ~Finisher_() { team_->Finish(); }
            } finisher_;

        public:
            ChunkTeamScope_(size_t nHelpers) : helpers_(ThreadPool_::GetInstance(), nHelpers), finisher_{&team_} {
                for (size_t k = 0; k < nHelpers; ++k)
                    helpers_.Spawn([this]() {
                        team_.Help();
                        return true;
                    });
            }

            ChunkTeamScope_(const ChunkTeamScope_&) = delete;
            ChunkTeamScope_& operator=(const ChunkTeamScope_&) = delete;

            ChunkTeam_* Team() { return &team_; }

            void Complete() {
                team_.Finish();
                helpers_.Complete();
            }
        };

        //  Fixed-size path chunks. Every regression sum is accumulated in path order within
        //  a chunk and then across chunks in chunk order, so it does not depend on the
        //  thread count (N9/N10); a set within one chunk sums in plain path order.
        class PathChunks_ {
            BatchPlan_ plan_;
            ChunkTeam_* team_;

        public:
            explicit PathChunks_(size_t nPaths, ChunkTeam_* team = nullptr) : plan_(nPaths, 1), team_(plan_.BatchCount() > 1 ? team : nullptr) {}

            [[nodiscard]] size_t Count() const { return plan_.BatchCount(); }

            template <class R_, class F_> Vector_<R_> Map(const F_& fn) const {
                Vector_<R_> results(plan_.BatchCount());
                if (!team_) {
                    for (size_t c = 0; c < results.size(); ++c)
                        results[c] = fn(plan_.BatchAt(c));
                    return results;
                }
                const std::function<void(size_t)> work = [&](size_t c) { results[c] = fn(plan_.BatchAt(c)); };
                team_->Run(results.size(), work);
                return results;
            }
        };

        //  Bitwise selects. The included mask is close to a coin flip on real regression
        //  sets, so branching on it mispredicts on about half of the paths. Adding the
        //  +0.0 of an excluded path leaves a running sum bitwise unchanged, so the masked
        //  passes reproduce the included-only arithmetic exactly.
        FORCE_INLINE double Select(bool keepFirst, double first, double second) {
            uint64_t a, b;
            std::memcpy(&a, &first, sizeof(a));
            std::memcpy(&b, &second, sizeof(b));
            const uint64_t mask = uint64_t(0) - static_cast<uint64_t>(keepFirst);
            const uint64_t bits = (a & mask) | (b & ~mask);
            double result;
            std::memcpy(&result, &bits, sizeof(result));
            return result;
        }

        FORCE_INLINE double Masked(bool keep, double value) { return Select(keep, value, 0.0); }

        struct IncludedSums_ {
            size_t count_ = 0;
            double sumX_ = 0.0;
            double sumTargets_ = 0.0;
        };

        FORCE_INLINE void AddIncluded(bool included, double x, double target, IncludedSums_* sums) {
            sums->count_ += static_cast<size_t>(included);
            sums->sumX_ += Masked(included, x);
            sums->sumTargets_ += Masked(included, target);
        }

        IncludedSums_ SumIncluded(const RegressionRows_& rows) {
            IncludedSums_ sums;
            for (size_t i = 0; i < rows.n_; ++i)
                AddIncluded(rows.included_[i] != 0, rows.x_[i], rows.targets_[i], &sums);
            return sums;
        }

        IncludedSums_ ReduceSums(const Vector_<IncludedSums_>& chunks) {
            IncludedSums_ sums;
            for (const auto& chunk : chunks) {
                sums.count_ += chunk.count_;
                sums.sumX_ += chunk.sumX_;
                sums.sumTargets_ += chunk.sumTargets_;
            }
            return sums;
        }

        double SumSquaredDeviations(const RegressionRows_& rows, double mean) {
            double sumSq = 0.0;
            for (size_t i = 0; i < rows.n_; ++i) {
                const double dev = Masked(rows.included_[i] != 0, rows.x_[i] - mean);
                sumSq += dev * dev;
            }
            return sumSq;
        }

        //  Any non-finite included value makes its sum non-finite, so the per-path check
        //  runs only to name the failure after a non-finite sum
        void RequireFiniteIncluded(const double* values, const char* included, size_t n, const char* message) {
            for (size_t i = 0; i < n; ++i)
                REQUIRE(!included[i] || std::isfinite(values[i]), message);
        }

        double ConstantFit(const RegressionRows_& rows, const IncludedSums_& sums) {
            if (sums.count_ == 0)
                return 0.0;
            if (!std::isfinite(sums.sumTargets_))
                RequireFiniteIncluded(rows.targets_, rows.included_, rows.n_, "InvalidRegressionInput: non-finite included target");
            REQUIRE(std::isfinite(sums.sumTargets_), "InvalidRegressionInput: target sum overflow");
            return sums.sumTargets_ / static_cast<double>(sums.count_);
        }

        //  Count, mean and sigma of the regressor over the included paths, with the sigma floor
        struct RegressionStats_ {
            size_t count_ = 0;
            double mean_ = 0.0;
            double sigma_ = 0.0;
            double sigmaFloor_ = 0.0;
        };

        RegressionStats_ RegressionStats(const RegressionRows_& rows, const IncludedSums_& sums, const PathChunks_& chunks) {
            RegressionStats_ stats;
            stats.count_ = sums.count_;
            if (!std::isfinite(sums.sumX_))
                RequireFiniteIncluded(rows.x_, rows.included_, rows.n_, "InvalidRegressionInput: non-finite included regressor");
            if (stats.count_ != 0) {
                stats.mean_ = sums.sumX_ / static_cast<double>(stats.count_);
                const double mean = stats.mean_;
                double sumSq = 0.0;
                for (double chunk : chunks.Map<double>([&](const PathBatch_& c) { return SumSquaredDeviations(SubRows(rows, c), mean); }))
                    sumSq += chunk;
                stats.sigma_ = std::sqrt(sumSq / static_cast<double>(stats.count_));
                REQUIRE(std::isfinite(stats.mean_) && std::isfinite(stats.sigma_), "InvalidRegressionInput: regressor moments overflow");
            }
            //  the floor engages even on an empty regression set, so a degenerate day
            //  never carries sigma_ = 0 into RegressionPredict's z-normalization
            stats.sigmaFloor_ = SIGMA_FLOOR_SCALE * std::max(1.0, std::abs(stats.mean_));
            return stats;
        }

        void DegradeToConstant(ExerciseRegression_* result, const char* reason, double constant) {
            result->degenerate_ = true;
            result->degenerateReason_ = reason;
            result->fallbackReason_ = reason;
            result->effectiveRank_ = result->numCondTrue_ > 0 ? 1 : 0;
            result->solver_ = "Constant";
            result->basisDegree_ = 0;
            result->coefficients_.Resize(1);
            result->coefficients_[0] = constant;
        }

        //  Hankel moments sum(z^k), k < 2 * nBasis_ - 1, and the right-hand side
        //  sum(z^j * target), j < nBasis_: O(N*d), each moment accumulated in path order
        //  within a chunk. Each sum is independent of nBasis_, so one pass serves every
        //  lower degree.
        struct Moments_ {
            size_t nBasis_ = 0;
            std::array<double, 17> hankel_{};
            std::array<double, 9> rhs_{};
        };

        template <size_t N_BASIS> void AccumulateMoments(const RegressionRows_& rows, double mean, double sigma, Moments_* moments) {
            std::array<double, 2 * N_BASIS - 1> hankel{};
            std::array<double, N_BASIS> rhs{};
            for (size_t i = 0; i < rows.n_; ++i) {
                const bool included = rows.included_[i] != 0;
                const double z = Masked(included, (rows.x_[i] - mean) / sigma);
                const double target = Masked(included, rows.targets_[i]);
                double power = Masked(included, 1.0);
                for (size_t j = 0; j < N_BASIS; ++j) {
                    hankel[j] += power;
                    rhs[j] += power * target;
                    power *= z;
                }
                for (size_t j = N_BASIS; j < 2 * N_BASIS - 1; ++j) {
                    hankel[j] += power;
                    power *= z;
                }
            }
            moments->nBasis_ = N_BASIS;
            std::copy(hankel.begin(), hankel.end(), moments->hankel_.begin());
            std::copy(rhs.begin(), rhs.end(), moments->rhs_.begin());
        }

        using MomentAccumulator_ = void (*)(const RegressionRows_&, double, double, Moments_*);

        template <size_t... N_> constexpr std::array<MomentAccumulator_, sizeof...(N_)> MomentAccumulators(std::index_sequence<N_...>) {
            return {&AccumulateMoments<N_ + 2>...};
        }

        Moments_ AccumulateChunkMoments(const RegressionRows_& rows, double mean, double sigma, size_t nBasis) {
            //  degrees 1..8 have 2..9 basis functions
            static constexpr auto ACCUMULATORS = MomentAccumulators(std::make_index_sequence<8>());
            REQUIRE(nBasis >= 2 && nBasis - 2 < ACCUMULATORS.size(), "InvalidRegressionInput: unsupported basis size");
            Moments_ moments;
            ACCUMULATORS[nBasis - 2](rows, mean, sigma, &moments);
            return moments;
        }

        Moments_ AccumulateMoments(const RegressionRows_& rows, double mean, double sigma, size_t nBasis, const PathChunks_& chunks) {
            Moments_ moments;
            moments.nBasis_ = nBasis;
            for (const auto& chunk :
                 chunks.Map<Moments_>([&](const PathBatch_& c) { return AccumulateChunkMoments(SubRows(rows, c), mean, sigma, nBasis); })) {
                for (size_t k = 0; k < 2 * nBasis - 1; ++k)
                    moments.hankel_[k] += chunk.hankel_[k];
                for (size_t j = 0; j < nBasis; ++j)
                    moments.rhs_[j] += chunk.rhs_[j];
            }
            return moments;
        }

        //  A[j,k] = sum(z^(j+k)) on the upper triangle; the fit retains the monomial convention
        void NormalEquations(const Moments_& moments, size_t nBasis, SquareMatrix_<>* gram, Vector_<>* rhs) {
            for (size_t j = 0; j < nBasis; ++j) {
                (*rhs)[j] = moments.rhs_[j];
                for (size_t k = j; k < nBasis; ++k)
                    (*gram)(j, k) = moments.hankel_[j + k];
            }
        }

        bool HasIndependentColumns(const SquareMatrix_<>& gram, const std::array<double, 9>& scale) {
            std::array<std::array<double, 9>, 9> lower{};
            for (int j = 0; j < gram.Rows(); ++j) {
                for (int k = 0; k <= j; ++k) {
                    double residual = gram(k, j) / scale[j] / scale[k];
                    for (int l = 0; l < k; ++l)
                        residual -= lower[j][l] * lower[k][l];
                    if (!std::isfinite(residual) || (j == k && residual <= 1.0 / CONDITION_LIMIT))
                        return false;
                    lower[j][k] = j == k ? std::sqrt(residual) : residual / lower[k][k];
                }
            }
            return true;
        }

        //  Equal diagonal entries do not imply independent columns. Test the
        //  unregularized, column-scaled Gram matrix before ridge can hide rank loss.
        bool RankDeficient(const SquareMatrix_<>& gram) {
            std::array<double, 9> scale{};
            for (int j = 0; j < gram.Rows(); ++j) {
                if (!std::isfinite(gram(j, j)) || gram(j, j) <= 0.0)
                    return true;
                scale[j] = std::sqrt(gram(j, j));
            }
            return !HasIndependentColumns(gram, scale);
        }

        //  Explicit relative ridge, then the Gram diagonal-ratio conditioning guard
        const char* RidgeAndIllConditioned(SquareMatrix_<>* gram) {
            if (RankDeficient(*gram))
                return "GramRankLoss";
            const size_t nBasis = static_cast<size_t>(gram->Rows());
            double maxDiag = (*gram)(0, 0);
            double minDiag = (*gram)(0, 0);
            for (size_t j = 0; j < nBasis; ++j) {
                (*gram)(j, j) *= 1.0 + RIDGE_LAMBDA;
                maxDiag = std::max(maxDiag, (*gram)(j, j));
                minDiag = std::min(minDiag, (*gram)(j, j));
            }
            return minDiag <= 0.0 || maxDiag / minDiag > CONDITION_LIMIT ? "GramScale" : nullptr;
        }

        struct QrWorkspace_ {
            size_t nRows_ = 0;
            size_t nBasis_ = 0;
            std::array<Vector_<>, 9> columns_;
            std::array<double, 9> scales_{};
            std::array<size_t, 9> permutation_{};
            std::array<std::array<double, 9>, 9> upper_{};
            std::array<double, 9> projection_{};
            Vector_<> response_;
        };

        QrWorkspace_ MakeQrWorkspace(const RegressionRows_& rows, double mean, double sigma, int degree) {
            QrWorkspace_ ws;
            ws.nBasis_ = static_cast<size_t>(degree + 1);
            ws.nRows_ = static_cast<size_t>(std::count_if(rows.included_, rows.included_ + rows.n_, [](char value) { return value != 0; }));
            ws.response_.Resize(ws.nRows_);
            for (size_t j = 0; j < ws.nBasis_; ++j) {
                ws.columns_[j].Resize(ws.nRows_);
                ws.permutation_[j] = j;
            }
            size_t row = 0;
            for (size_t i = 0; i < rows.n_; ++i) {
                if (!rows.included_[i])
                    continue;
                const double z = (rows.x_[i] - mean) / sigma;
                ws.response_[row] = rows.targets_[i];
                double power = 1.0;
                for (size_t j = 0; j < ws.nBasis_; ++j) {
                    ws.columns_[j][row] = power;
                    power *= z;
                }
                ++row;
            }
            return ws;
        }

        size_t NormalizeQrColumns(QrWorkspace_* ws) {
            for (size_t j = 0; j < ws->nBasis_; ++j) {
                double norm = 0.0;
                for (double value : ws->columns_[j])
                    norm = std::hypot(norm, value);
                if (!std::isfinite(norm) || norm == 0.0)
                    return j;
                ws->scales_[j] = norm;
                for (double& value : ws->columns_[j])
                    value /= norm;
            }
            return ws->nBasis_;
        }

        std::pair<size_t, double> BestQrPivot(const QrWorkspace_& ws, size_t step) {
            size_t pivot = step;
            double bestNorm = 0.0;
            for (size_t j = step; j < ws.nBasis_; ++j) {
                double normSq = 0.0;
                for (double value : ws.columns_[j])
                    normSq += value * value;
                if (normSq > bestNorm) {
                    bestNorm = normSq;
                    pivot = j;
                }
            }
            return {pivot, bestNorm};
        }

        void PivotQrColumns(QrWorkspace_* ws, size_t step, size_t pivot) {
            if (pivot == step)
                return;
            std::swap(ws->columns_[step], ws->columns_[pivot]);
            std::swap(ws->scales_[step], ws->scales_[pivot]);
            std::swap(ws->permutation_[step], ws->permutation_[pivot]);
            for (size_t i = 0; i < step; ++i)
                std::swap(ws->upper_[i][step], ws->upper_[i][pivot]);
        }

        void OrthogonalizeQrStep(QrWorkspace_* ws, size_t step, double normSq) {
            ws->upper_[step][step] = std::sqrt(normSq);
            for (double& value : ws->columns_[step])
                value /= ws->upper_[step][step];
            for (size_t i = 0; i < ws->nRows_; ++i)
                ws->projection_[step] += ws->columns_[step][i] * ws->response_[i];
            for (size_t j = step + 1; j < ws->nBasis_; ++j)
                for (int pass = 0; pass < 2; ++pass) {
                    double dot = 0.0;
                    for (size_t i = 0; i < ws->nRows_; ++i)
                        dot += ws->columns_[step][i] * ws->columns_[j][i];
                    ws->upper_[step][j] += dot;
                    for (size_t i = 0; i < ws->nRows_; ++i)
                        ws->columns_[j][i] -= dot * ws->columns_[step][i];
                }
        }

        void RecoverQrCoefficients(const QrWorkspace_& ws, Vector_<>* coefficients) {
            std::array<double, 9> pivoted{};
            for (size_t k = ws.nBasis_; k-- > 0;) {
                double residual = ws.projection_[k];
                for (size_t j = k + 1; j < ws.nBasis_; ++j)
                    residual -= ws.upper_[k][j] * pivoted[j];
                pivoted[k] = residual / ws.upper_[k][k];
            }
            coefficients->Resize(ws.nBasis_);
            for (size_t k = 0; k < ws.nBasis_; ++k)
                (*coefficients)[ws.permutation_[k]] = pivoted[k] / ws.scales_[k];
        }

        //  Column-pivoted, twice-reorthogonalized QR on actual design rows runs
        //  only after the O(Md) moment solve rejects a fit.
        size_t PivotedQrFit(const RegressionRows_& rows, double mean, double sigma, int degree, Vector_<>* coefficients) {
            auto ws = MakeQrWorkspace(rows, mean, sigma, degree);
            const size_t normalized = NormalizeQrColumns(&ws);
            if (normalized != ws.nBasis_)
                return normalized;
            size_t rank = 0;
            for (size_t step = 0; step < ws.nBasis_; ++step) {
                const auto [pivot, normSq] = BestQrPivot(ws, step);
                if (!std::isfinite(normSq) || normSq < QR_RANK_TOLERANCE * QR_RANK_TOLERANCE)
                    break;
                PivotQrColumns(&ws, step, pivot);
                OrthogonalizeQrStep(&ws, step, normSq);
                ++rank;
            }
            if (rank == ws.nBasis_)
                RecoverQrCoefficients(ws, coefficients);
            return rank;
        }

        bool AllFinite(const Vector_<>& values) {
            return std::all_of(values.begin(), values.end(), [](double value) { return std::isfinite(value); });
        }

        //  Guard order: sample size, then sigma floor; nullptr when the fit may proceed
        const char* DegenerateReason(const RegressionStats_& stats, int degree) {
            if (stats.count_ == 0)
                return "ConditionPathsBelowMin";
            if (stats.sigma_ < stats.sigmaFloor_)
                return "SigmaFloor";
            if (stats.count_ < PATHS_PER_BASIS_FUNCTION * static_cast<size_t>(degree + 1))
                return "ConditionPathsBelowMin";
            return nullptr;
        }

        //  Rank-revealing fallback once the moment solve rejects the Gram matrix: lower
        //  the degree to the recovered rank before falling back to a constant
        void FitPivotedQr(const RegressionRows_& rows, int degree, const char* fallbackReason, double constantFit, ExerciseRegression_* result) {
            result->solver_ = "PivotedQR";
            result->fallbackReason_ = fallbackReason;
            int fitDegree = degree;
            size_t effectiveRank = static_cast<size_t>(degree) + 1;
            while (fitDegree > 0) {
                Vector_<> coefficients;
                const size_t rank = PivotedQrFit(rows, result->mean_, result->sigma_, fitDegree, &coefficients);
                effectiveRank = std::min(effectiveRank, rank);
                if (rank == static_cast<size_t>(fitDegree + 1)) {
                    if (!AllFinite(coefficients))
                        break;
                    result->coefficients_ = std::move(coefficients);
                    result->basisDegree_ = fitDegree;
                    result->effectiveRank_ = effectiveRank;
                    if (fitDegree < degree)
                        result->fallbackReason_ = "RankDeficient";
                    return;
                }
                fitDegree = std::min(fitDegree - 1, static_cast<int>(rank) - 1);
            }
            DegradeToConstant(result, "IllConditioned", constantFit);
        }

        void FitMomentsCholesky(SquareMatrix_<>* gram, const Vector_<>& rhs, int degree, double constantFit, ExerciseRegression_* result) {
            Vector_<Vector_<>> rhsWrapped(1, rhs);
            CholeskySolve(gram, &rhsWrapped);
            if (!AllFinite(rhsWrapped[0])) {
                DegradeToConstant(result, "IllConditioned", constantFit);
                return;
            }
            result->coefficients_ = rhsWrapped[0];
            result->basisDegree_ = degree;
            result->effectiveRank_ = static_cast<size_t>(degree) + 1;
            result->solver_ = "MomentsCholesky";
        }

        ExerciseRegression_
        FitFromMoments(const RegressionRows_& rows, const RegressionStats_& stats, double constantFit, const Moments_* moments, int degree) {
            ExerciseRegression_ result;
            result.numCondTrue_ = stats.count_;
            result.mean_ = stats.mean_;
            result.sigma_ = std::max(stats.sigma_, stats.sigmaFloor_);
            if (const char* reason = DegenerateReason(stats, degree)) {
                DegradeToConstant(&result, reason, constantFit);
                return result;
            }
            const size_t nBasis = static_cast<size_t>(degree) + 1;
            REQUIRE(moments && moments->nBasis_ >= nBasis, "InvalidRegressionInput: missing regression moments");
            SquareMatrix_<> gram(static_cast<int>(nBasis), 0.0);
            Vector_<> rhs(nBasis, 0.0);
            NormalEquations(*moments, nBasis, &gram, &rhs);
            //  Gram conditioning is the last guard
            if (const char* fallbackReason = RidgeAndIllConditioned(&gram))
                FitPivotedQr(rows, degree, fallbackReason, constantFit, &result);
            else
                FitMomentsCholesky(&gram, rhs, degree, constantFit, &result);
            return result;
        }

        //  Shared inputs of every candidate degree fitted on one regression set
        struct RegressionInputs_ {
            RegressionStats_ stats_;
            double constantFit_ = 0.0;
            std::optional<Moments_> moments_;
        };

        //  One statistics pass and one moment pass cover all degrees up to maxDegree;
        //  degrees that fail the sample-size guard never need the moments
        RegressionInputs_ PrepareRegression(const RegressionRows_& rows, const IncludedSums_& sums, int maxDegree, const PathChunks_& chunks) {
            RegressionInputs_ inputs;
            inputs.stats_ = RegressionStats(rows, sums, chunks);
            inputs.constantFit_ = ConstantFit(rows, sums);
            const auto& stats = inputs.stats_;
            if (stats.count_ == 0 || stats.sigma_ < stats.sigmaFloor_)
                return inputs;
            const size_t feasibleBasis = std::min(static_cast<size_t>(maxDegree) + 1, stats.count_ / PATHS_PER_BASIS_FUNCTION);
            if (feasibleBasis >= 2)
                inputs.moments_ = AccumulateMoments(rows, stats.mean_, std::max(stats.sigma_, stats.sigmaFloor_), feasibleBasis, chunks);
            return inputs;
        }

        ExerciseRegression_ FitRegression(const RegressionRows_& rows, const IncludedSums_& sums, int degree, const PathChunks_& chunks) {
            ValidateLsmcBasisDegree(degree);
            const auto inputs = PrepareRegression(rows, sums, degree, chunks);
            return FitFromMoments(rows, inputs.stats_, inputs.constantFit_, inputs.moments_ ? &*inputs.moments_ : nullptr, degree);
        }

        constexpr size_t NO_SLOT = static_cast<size_t>(-1);

        struct ExerciseDayPlan_ {
            size_t eventId_;
            bool conditional_;
            double eps_; //  S17: the node's :eps option resolved against the simulation smoothing width
        };

        bool EventHasPays(const Event_& event) {
            for (const auto& statement : event)
                if (FindNode(*statement, [](const Node_& visited) { return dynamic_cast<const NodePays_*>(&visited) != nullptr; }))
                    return true;
            return false;
        }

        //  EXERCISE is a top-level statement only (nesting is rejected at parse time)
        const NodeExercise_* EventExercise(const Event_& event) {
            for (const auto& statement : event)
                if (const auto* exercise = dynamic_cast<const NodeExercise_*>(statement.get()))
                    return exercise;
            return nullptr;
        }

        struct LsmcPlan_ {
            Vector_<size_t> eventToPays_;
            Vector_<size_t> eventToExercise_;
            Vector_<size_t> paysEventIds_;
            Vector_<ExerciseDayPlan_> days_;
            bool anyConditional_ = false;
        };

        LsmcPlan_ ScanEvents(const Vector_<Event_>& events, double smooth) {
            LsmcPlan_ scan;
            scan.eventToPays_.Resize(events.size());
            scan.eventToExercise_.Resize(events.size());
            for (size_t e = 0; e < events.size(); ++e)
                scan.eventToPays_[e] = scan.eventToExercise_[e] = NO_SLOT;
            for (size_t e = 0; e < events.size(); ++e) {
                if (EventHasPays(events[e])) {
                    scan.eventToPays_[e] = scan.paysEventIds_.size();
                    scan.paysEventIds_.push_back(e);
                }
                if (const NodeExercise_* exercise = EventExercise(events[e])) {
                    const bool conditional = exercise->arguments_.size() > 1;
                    scan.anyConditional_ |= conditional;
                    scan.eventToExercise_[e] = scan.days_.size();
                    scan.days_.push_back({e, conditional, exercise->eps_ < 0.0 ? smooth : exercise->eps_});
                }
            }
            return scan;
        }

        //  Training keeps only regression inputs; hard pricing uses worker-local scalars.
        struct LsmcStorage_ {
            LsmcRows_ pays_;
            LsmcRows_ xByDay_;
            LsmcRows_ hByDay_;
            Vector_<Vector_<char>> condByDay_; //  empty row = unconditional day

            //  Every recorded value of a path is rewritten by its forward evaluation;
            //  the zero fill keeps the rows deterministic regardless
            void ZeroPaths(size_t firstPath, size_t pathCount) {
                pays_.ZeroPaths(firstPath, pathCount);
                xByDay_.ZeroPaths(firstPath, pathCount);
                hByDay_.ZeroPaths(firstPath, pathCount);
            }
        };

        LsmcStorage_ MakeStorage(const LsmcPlan_& scan, size_t nPaths) {
            LsmcStorage_ storage;
            storage.pays_ = LsmcRows_(scan.paysEventIds_.size(), nPaths);
            storage.xByDay_ = LsmcRows_(scan.days_.size(), nPaths);
            storage.hByDay_ = LsmcRows_(scan.days_.size(), nPaths);
            if (scan.anyConditional_) {
                storage.condByDay_.Resize(scan.days_.size());
                for (size_t k = 0; k < scan.days_.size(); ++k)
                    if (scan.days_[k].conditional_)
                        storage.condByDay_[k] = Vector_<char>(nPaths, 1);
            }
            return storage;
        }

        struct PricingObservation_ {
            double x_;
            double h_;
            bool cond_;
        };

        struct ThreadState_ {
            std::unique_ptr<Random_> random_;
            Vector_<> gauss_;
            Scenario_<> path_;
            //  Shared with MCDoubleSimulation via Detail::LocalCheckedPaths_ (simulation.hpp)
            std::unique_ptr<Detail::LocalCheckedPaths_> bsPaths_;
            LsmcEvaluator_<double> evaluator_;
            //  Compiled mode: worker-local state feeding training or pricing sinks
            std::optional<EvalState_<double>> compiledState_;
            LsmcSinks_ sinks_;
            size_t exercisedDay_ = NO_SLOT;
            double preExercise_ = 0.0;
            double exerciseValue_ = 0.0;

            explicit ThreadState_(const struct LsmcContext_& ctx);

            [[nodiscard]] const Scenario_<>& Path() const { return bsPaths_ ? bsPaths_->Path() : path_; }
            [[nodiscard]] PricingObservation_ PricingObservation() const {
                if (compiledState_)
                    return {sinks_.pricingX_, sinks_.pricingH_, sinks_.pricingCond_};
                return {evaluator_.pricingX_, evaluator_.pricingH_, evaluator_.pricingCond_};
            }
        };

        //  Immutable per-run references shared by the three phases
        struct LsmcContext_ {
            const PreparedScript_& prepared_;
            AAD::Model_<double>* model_;
            const LsmcPlan_& scan_;
            LsmcStorage_& storage_;
            const ScriptCompiled_* compiled_ = nullptr;            //  shared immutable program for every worker
            const Vector_<ExerciseRegression_>* policy_ = nullptr; //  pricing stops evaluating after the first hard exercise
            std::optional<uint64_t> scrambleKey_;
            const Vector_<>* constValues_ = nullptr; //  optional bumped script constants for policy training

            const ScriptProduct_& Product() const { return prepared_.Product(); }
            const ObservationPlan_& Plan() const { return prepared_.Plan(); }
            [[nodiscard]] size_t PayOffIdx() const { return Product().PayOffIdx(); }
        };

        ThreadState_::ThreadState_(const LsmcContext_& ctx)
            : random_(CreateRNG(ctx.prepared_.Simulation().rsg_, *ctx.model_, ctx.prepared_.Simulation().useBb_, ctx.scrambleKey_)),
              gauss_(ctx.model_->SimDim()), evaluator_(ctx.Product().VarValues(), ctx.Product().ConstVarValues(), ctx.Product().VectorCapacities()) {
            evaluator_.SetHistoricalVectorSeed(TypedVectorValues<double>(ctx.Product().VectorValues()));
            if (typeid(*ctx.model_) == typeid(AAD::BlackScholes_<double>))
                bsPaths_ = std::make_unique<Detail::LocalCheckedPaths_>(static_cast<const AAD::BlackScholes_<double>&>(*ctx.model_));
            else {
                AllocatePath(ctx.Plan().DefLine(), path_);
                InitializePath(path_);
            }
            evaluator_.eventToPays_ = &ctx.scan_.eventToPays_;
            evaluator_.payoffIdx_ = ctx.PayOffIdx();
            evaluator_.eventToExercise_ = &ctx.scan_.eventToExercise_;
            evaluator_.paysStorage_ = ctx.policy_ ? nullptr : &ctx.storage_.pays_;
            evaluator_.xStorage_ = ctx.policy_ ? nullptr : &ctx.storage_.xByDay_;
            evaluator_.hStorage_ = ctx.policy_ ? nullptr : &ctx.storage_.hByDay_;
            evaluator_.condStorage_ = ctx.policy_ || ctx.storage_.condByDay_.empty() ? nullptr : &ctx.storage_.condByDay_;
            if (ctx.compiled_) {
                compiledState_.emplace(ctx.prepared_.BuildEvalState<double>());
                sinks_.eventToPays_ = &ctx.scan_.eventToPays_;
                sinks_.payoffIdx_ = ctx.PayOffIdx();
                sinks_.eventToExercise_ = &ctx.scan_.eventToExercise_;
                sinks_.pays_ = ctx.policy_ ? nullptr : &ctx.storage_.pays_;
                sinks_.x_ = ctx.policy_ ? nullptr : &ctx.storage_.xByDay_;
                sinks_.h_ = ctx.policy_ ? nullptr : &ctx.storage_.hByDay_;
                sinks_.cond_ = ctx.policy_ || ctx.storage_.condByDay_.empty() ? nullptr : &ctx.storage_.condByDay_;
                compiledState_->lsmcSinks_ = &sinks_;
            }
            if (ctx.constValues_) {
                evaluator_.ConstVarVals() = *ctx.constValues_;
                if (compiledState_)
                    compiledState_->ConstVarVals() = *ctx.constValues_;
                PastEvaluator_<double> past(Vector_<>(ctx.Product().VarNames().size(), 0.0), *ctx.constValues_, ctx.Product().VectorCapacities());
                past.SetObservations(&ctx.Plan());
                ctx.Product().Visit(past, true, false);
                evaluator_.SetHistoricalSeed(past.VarVals());
                evaluator_.SetHistoricalVectorSeed(past.VectorVals());
                if (compiledState_)
                    compiledState_->SetHistoricalSeed(past.VarVals());
                if (compiledState_)
                    compiledState_->SetHistoricalVectorSeed(past.VectorVals());
            }
        }

        //  S4: exercise replaces same-day and later payments only, so the payoff
        //  accumulated strictly before the exercise event survives (both engines)
        void SnapshotPreExercise(const LsmcContext_& ctx, const Vector_<>& variables, size_t event, ThreadState_* state) {
            const size_t slot = ctx.scan_.eventToExercise_[event];
            if (slot != NO_SLOT && ctx.policy_)
                state->preExercise_ = ctx.Product().HasPays() ? variables[ctx.PayOffIdx()] : 0.0;
        }

        bool StopAfterEvent(const LsmcContext_& ctx, size_t event, ThreadState_* state) {
            const size_t day = ctx.scan_.eventToExercise_[event];
            if (!ctx.policy_ || day == NO_SLOT)
                return false;
            const auto observation = state->PricingObservation();
            if (!(observation.cond_ && observation.h_ > 0.0 && observation.h_ > RegressionPredict((*ctx.policy_)[day], observation.x_)))
                return false;
            state->exercisedDay_ = day;
            state->exerciseValue_ = observation.h_;
            return true;
        }

        void TreeEvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            auto& eval = state.evaluator_;
            eval.pathSlot_ = pathSlot;
            eval.SetScenario(&state.Path());
            eval.SetObservations(&ctx.Plan());
            eval.Init();
            const auto& events = ctx.Product().Events();
            const auto& eventToSample = ctx.Plan().EventToSample();
            for (size_t e = 0; e < events.size(); ++e) {
                SnapshotPreExercise(ctx, eval.VarVals(), e, &state);
                eval.SetCurEvt(eventToSample[e]);
                eval.SetEventOrdinal(e);
                for (const auto& statement : events[e])
                    statement->Accept(eval);
                if (StopAfterEvent(ctx, e, &state))
                    break;
            }
        }

        //  Compiled engine mirror: one event stream per event, the driver installs the
        //  sinks and owns the event boundaries; EXERCISE is recording-only on the state
        void CompiledEvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            const ScriptCompiled_& compiled = *ctx.compiled_;
            auto& eval = *state.compiledState_;
            auto& sinks = state.sinks_;
            sinks.pathSlot_ = pathSlot;
            eval.Init();
            eval.observations_ = &ctx.Plan();
            eval.scenario_ = &state.Path();
            const auto& nodeStreams = compiled.NodeStreams();
            const auto& constStreams = compiled.ConstStreams();
            const auto& eventToSample = ctx.Plan().EventToSample();
            for (size_t e = 0; e < nodeStreams.size(); ++e) {
                sinks.eventOrdinal_ = e;
                SnapshotPreExercise(ctx, eval.variables_, e, &state);
                const Detail::CompiledEventView_<double> view{nodeStreams[e], constStreams[e], state.Path()[eventToSample[e]]};
                Detail::EvalCompiledEvents<true, true>(1, [&](size_t) { return view; }, &eval);
                if (StopAfterEvent(ctx, e, &state))
                    break;
            }
        }

        //  One forward evaluation: training records regression inputs, while hard
        //  pricing retains only the current decision and stops at first exercise.
        void EvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            state.exercisedDay_ = NO_SLOT;
            state.preExercise_ = 0.0;
            state.exerciseValue_ = 0.0;
            state.random_->FillNormal(&state.gauss_);
            if (state.bsPaths_) {
                if (!state.bsPaths_->Generate(state.gauss_))
                    Detail::DiagnoseInvalidSimulationPath(state.bsPaths_->Path());
            } else {
                ctx.model_->GeneratePath(state.gauss_, &state.path_);
                ValidateSimulationPath(state.path_);
            }
            if (state.compiledState_)
                CompiledEvaluateRecordedPath(state, ctx, pathSlot);
            else
                TreeEvaluateRecordedPath(state, ctx, pathSlot);
        }

        //  Phase A: forward storage over disjoint per-batch path slots
        void RunForwardPhase(LsmcContext_& ctx, const BatchPlan_& batchPlan, size_t pathOffset = 0) {
            ThreadPool_* pool = ThreadPool_::GetInstance();
            Vector_<std::unique_ptr<ThreadState_>> threadStates(pool->NumThreads());
            SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
            for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                tasks.Spawn([&, batch]() {
                    auto& local = threadStates[ThreadPool_::ThreadNum()];
                    if (!local)
                        local = std::make_unique<ThreadState_>(ctx);
                    ThreadState_& state = *local;
                    ctx.storage_.ZeroPaths(batch.firstPath_, batch.pathCount_);
                    state.random_->SkipTo(pathOffset + batch.firstPath_);
                    for (size_t i = 0; i < batch.pathCount_; ++i)
                        EvaluateRecordedPath(state, ctx, batch.firstPath_ + i);
                    return true;
                });
            }
            tasks.Complete();
        }

        //  N5: the backward discounting ratios anchor on one probe path's numeraires;
        //  Black-Scholes and Dupire, the only models the factory constructs, carry
        //  deterministic rates, so the numeraire is path-independent and any path
        //  pins the same ratios. The entry point debug-asserts that model set, and the
        //  probe goes through the same validation as every worker path
        Vector_<> SampleGridNumeraires(const LsmcContext_& ctx) {
            const auto& simulation = ctx.prepared_.Simulation();
            const auto& events = ctx.Product().Events();
            auto probe = CreateRNG(simulation.rsg_, *ctx.model_, simulation.useBb_);
            Vector_<> gauss(ctx.model_->SimDim());
            Scenario_<> path;
            AllocatePath(ctx.Plan().DefLine(), path);
            probe->SkipTo(0);
            probe->FillNormal(&gauss);
            ctx.model_->GeneratePath(gauss, &path);
            ValidateSimulationPath(path);
            Vector_<> eventNumeraire(events.size(), 1.0);
            for (size_t e = 0; e < events.size(); ++e)
                eventNumeraire[e] = path[ctx.Plan().EventToSample()[e]].numeraire_;
            return eventNumeraire;
        }

        //  The exercise decisions of a later day, applied at the start of the next sweep
        struct PendingDecision_ {
            const ExerciseRegression_* regression_ = nullptr;
            const double* h_ = nullptr;
            const double* x_ = nullptr;
        };

        //  The frozen fit copied into locals: the sweeps store chars, which may alias any
        //  heap state, so the fit's fields would otherwise be reloaded on every path
        struct LocalPredictor_ {
            std::array<double, 9> coefficients_{};
            size_t size_ = 0;
            double mean_ = 0.0;
            double sigma_ = 1.0;

            explicit LocalPredictor_(const ExerciseRegression_& regression)
                : size_(regression.coefficients_.size()), mean_(regression.mean_), sigma_(regression.sigma_) {
                REQUIRE(size_ >= 1 && size_ <= coefficients_.size(), "InvalidRegressionInput: unsupported coefficient count");
                std::copy(regression.coefficients_.begin(), regression.coefficients_.end(), coefficients_.begin());
            }

            FORCE_INLINE double operator()(double x) const { return PredictContinuation(coefficients_.data(), size_, mean_, sigma_, x); }
        };

        //  Backward holding values and the current regression set of one path block
        //  (training or validation), walked through every event
        struct BackwardRows_ {
            const LsmcStorage_& storage_;
            Vector_<> w_;
            Vector_<char> included_;
            PendingDecision_ pending_;
            PathChunks_ chunks_;

            BackwardRows_(const LsmcStorage_& storage, size_t nPaths, ChunkTeam_* team)
                : storage_(storage), w_(nPaths, 0.0), included_(nPaths, 1), chunks_(nPaths, team) {}
            [[nodiscard]] size_t Size() const { return w_.size(); }
        };

        //  One fused, branch-free sweep per event and path block:
        //   1. S3/S4: exercise on strictly-better continuation estimates of the later day
        //      replaces the future (W := h)
        //   2. holding value in date-i units, H = p + D * W, W := H in place
        //   3. Longstaff-Schwartz regression set of this day: the in-the-money
        //      condition-true paths (h > 0). Deep-OTM paths carry no exercise
        //      information, and a continuation estimate that undershoots below zero
        //      must not "exercise" a worthless option on them.
        //  Each path sees the per-path operations of the separate passes in the same
        //  order, so the result is bitwise identical to running them one after another.
        //  Storage rows one event's sweep reads; exercise rows stay null off exercise days
        struct EventRows_ {
            const double* pays_ = nullptr;
            const double* h_ = nullptr;
            const double* x_ = nullptr;
            const char* cond_ = nullptr;
        };

        EventRows_ SweepRows(const LsmcStorage_& storage, const LsmcPlan_& scan, size_t event) {
            EventRows_ rows;
            const size_t paysSlot = scan.eventToPays_[event];
            if (paysSlot != NO_SLOT)
                rows.pays_ = storage.pays_[paysSlot];
            const size_t day = scan.eventToExercise_[event];
            if (day == NO_SLOT)
                return rows;
            rows.h_ = storage.hByDay_[day];
            rows.x_ = storage.xByDay_[day];
            if (!storage.condByDay_.empty() && !storage.condByDay_[day].empty())
                rows.cond_ = RowData(storage.condByDay_[day]);
            return rows;
        }

        FORCE_INLINE double HoldingValue(const double* pays, size_t path, double dNext, bool hasNext, double next) {
            return (pays ? pays[path] : 0.0) + (hasNext ? dNext * next : 0.0);
        }

        FORCE_INLINE bool InRegressionSet(const char* cond, const double* h, size_t path) { return (!cond || cond[path] != 0) & (h[path] > 0.0); }

        template <bool APPLY, bool RECORD>
        IncludedSums_ SweepEvent(const LsmcPlan_& scan, size_t event, double dNext, bool hasNext, const PathBatch_& chunk, BackwardRows_* rows) {
            const EventRows_ eventRows = SweepRows(rows->storage_, scan, event);
            const double* pays = eventRows.pays_;
            const double* h = eventRows.h_;
            const double* x = eventRows.x_;
            const char* cond = eventRows.cond_;
            const PendingDecision_ pending = rows->pending_;
            std::optional<LocalPredictor_> predict;
            if constexpr (APPLY)
                predict.emplace(*pending.regression_);
            const double* pendingH = pending.h_;
            const double* pendingX = pending.x_;
            const size_t end = chunk.firstPath_ + chunk.pathCount_;
            double* w = &rows->w_[0];
            char* included = &rows->included_[0];
            IncludedSums_ sums;
            for (size_t j = chunk.firstPath_; j < end; ++j) {
                double value = w[j];
                if constexpr (APPLY)
                    value = Select((included[j] != 0) & (pendingH[j] > (*predict)(pendingX[j])), pendingH[j], value);
                value = HoldingValue(pays, j, dNext, hasNext, value);
                w[j] = value;
                if constexpr (RECORD) {
                    const bool in = InRegressionSet(cond, h, j);
                    included[j] = static_cast<char>(in);
                    AddIncluded(in, x[j], value, &sums);
                }
            }
            return sums;
        }

        IncludedSums_ SweepEvent(const LsmcPlan_& scan, size_t event, double dNext, bool hasNext, BackwardRows_* rows) {
            using Sweep_ = IncludedSums_ (*)(const LsmcPlan_&, size_t, double, bool, const PathBatch_&, BackwardRows_*);
            //  indexed by 2 * apply + record
            static constexpr std::array<Sweep_, 4> SWEEPS = {&SweepEvent<false, false>, &SweepEvent<false, true>, &SweepEvent<true, false>,
                                                             &SweepEvent<true, true>};
            const bool apply = rows->pending_.regression_ != nullptr;
            const bool record = scan.eventToExercise_[event] != NO_SLOT;
            const Sweep_ sweep = SWEEPS[2 * static_cast<size_t>(apply) + static_cast<size_t>(record)];
            return ReduceSums(
                rows->chunks_.Map<IncludedSums_>([&](const PathBatch_& chunk) { return sweep(scan, event, dNext, hasNext, chunk, rows); }));
        }

        RegressionRows_ DayRows(const BackwardRows_& rows, size_t day) {
            return {rows.storage_.xByDay_[day], RowData(rows.w_), RowData(rows.included_), rows.Size()};
        }

        void DeferDecision(const ExerciseRegression_& regression, size_t day, BackwardRows_* rows) {
            rows->pending_ = {&regression, rows->storage_.hByDay_[day], rows->storage_.xByDay_[day]};
        }

        struct ValidationLoss_ {
            double mse_ = std::numeric_limits<double>::infinity();
            double standardError_ = 0.0;
        };

        ValidationLoss_ EvaluateValidationLoss(const ExerciseRegression_& candidate, const RegressionRows_& validation, size_t count) {
            double sumLoss = 0.0;
            double sumLossSq = 0.0;
            for (size_t i = 0; i < validation.n_; ++i) {
                const double error = Masked(validation.included_[i] != 0, RegressionPredict(candidate, validation.x_[i]) - validation.targets_[i]);
                const double loss = error * error;
                sumLoss += loss;
                sumLossSq += loss * loss;
            }
            const double mse = sumLoss / static_cast<double>(count);
            const double secondMoment = sumLossSq / static_cast<double>(count);
            return {mse, std::sqrt(std::max(0.0, secondMoment - mse * mse) / static_cast<double>(count))};
        }

        int ChooseValidationDegree(const std::array<ValidationLoss_, 8>& losses, int maxDegree, const ValidationLoss_& best) {
            if (!std::isfinite(best.mse_))
                return 0;
            const double threshold = best.mse_ + best.standardError_ + 1e-10 * std::max(1.0, best.mse_);
            for (int degree = 1; degree <= maxDegree; ++degree)
                if (std::isfinite(losses[static_cast<size_t>(degree - 1)].mse_) && losses[static_cast<size_t>(degree - 1)].mse_ <= threshold)
                    return degree - 1;
            return 0;
        }

        ExerciseRegression_ SelectRegression(const RegressionRows_& training,
                                             const IncludedSums_& trainingSums,
                                             const PathChunks_& trainingChunks,
                                             const RegressionRows_& validation,
                                             size_t validationCount,
                                             int maxDegree) {
            if (validationCount == 0)
                return FitRegression(training, trainingSums, maxDegree, trainingChunks);

            ValidateLsmcBasisDegree(maxDegree);
            const auto inputs = PrepareRegression(training, trainingSums, maxDegree, trainingChunks);
            const Moments_* moments = inputs.moments_ ? &*inputs.moments_ : nullptr;
            std::array<ExerciseRegression_, 8> candidates;
            std::array<ValidationLoss_, 8> losses;
            ValidationLoss_ best;
            for (int degree = 1; degree <= maxDegree; ++degree) {
                auto& candidate = candidates[static_cast<size_t>(degree - 1)];
                candidate = FitFromMoments(training, inputs.stats_, inputs.constantFit_, moments, degree);
                auto& loss = losses[static_cast<size_t>(degree - 1)];
                loss = EvaluateValidationLoss(candidate, validation, validationCount);
                if (std::isfinite(loss.mse_) && loss.mse_ < best.mse_)
                    best = loss;
            }
            const int chosen = ChooseValidationDegree(losses, maxDegree, best);
            auto selected = std::move(candidates[static_cast<size_t>(chosen)]);
            if (std::isfinite(best.mse_))
                selected.validationMse_ = losses[static_cast<size_t>(chosen)].mse_;
            return selected;
        }

        //  Phase B: backward induction and continuation regressions over fixed path
        //  chunks on the pool, reduced in chunk order (thread-count independent, N9/N10).
        //  Each exercise day's decisions are applied by the next event's sweep; nothing
        //  reads the holding values after the first event, so its decisions are not applied.
        //  One helper per chunk beyond the caller's, capped by the pool's workers
        size_t HelperCount(size_t nPaths) {
            const size_t chunks = PathChunks_(nPaths).Count();
            return chunks > 1 ? std::min(ThreadPool_::GetInstance()->NumThreads(), chunks) - 1 : 0;
        }

        //  Sweeps one event over a path block, then leaves no decision pending
        IncludedSums_ SweepAndClear(const LsmcPlan_& scan, size_t event, double dNext, bool hasNext, BackwardRows_* rows) {
            const IncludedSums_ sums = SweepEvent(scan, event, dNext, hasNext, rows);
            rows->pending_ = {};
            return sums;
        }

        //  blocks[0] is the training block; an optional blocks[1] selects the degree
        ExerciseRegression_ FitExerciseDay(const Vector_<BackwardRows_*>& blocks, const std::array<IncludedSums_, 2>& sums, size_t day, int degree) {
            BackwardRows_& training = *blocks[0];
            if (blocks.size() == 1)
                return FitRegression(DayRows(training, day), sums[0], degree, training.chunks_);
            return SelectRegression(DayRows(training, day), sums[0], training.chunks_, DayRows(*blocks[1], day), sums[1].count_, degree);
        }

        Vector_<ExerciseRegression_> RunBackwardPhase(const LsmcContext_& ctx,
                                                      const Vector_<>& eventNumeraire,
                                                      size_t nPaths,
                                                      int degree,
                                                      const LsmcStorage_* validationStorage = nullptr,
                                                      size_t nValidation = 0) {
            const auto& scan = ctx.scan_;
            const auto& events = ctx.Product().Events();
            Vector_<ExerciseRegression_> regressions(scan.days_.size());
            ChunkTeamScope_ team(HelperCount(std::max(nPaths, nValidation)));
            BackwardRows_ training(ctx.storage_, nPaths, team.Team());
            std::optional<BackwardRows_> validation;
            Vector_<BackwardRows_*> blocks(1, &training);
            if (validationStorage)
                blocks.push_back(&validation.emplace(*validationStorage, nValidation, team.Team()));
            for (size_t ei = events.size(); ei-- > 0;) {
                const bool hasNext = ei + 1 < events.size();
                const double dNext = hasNext ? eventNumeraire[ei] / eventNumeraire[ei + 1] : 1.0;
                std::array<IncludedSums_, 2> sums;
                for (size_t b = 0; b < blocks.size(); ++b)
                    sums[b] = SweepAndClear(scan, ei, dNext, hasNext, blocks[b]);
                const size_t day = scan.eventToExercise_[ei];
                if (day == NO_SLOT)
                    continue;
                regressions[day] = FitExerciseDay(blocks, sums, day, degree);
                for (BackwardRows_* rows : blocks)
                    DeferDecision(regressions[day], day, rows);
            }
            team.Complete();
            return regressions;
        }

        struct TrainingOutcome_ {
            Vector_<> eventNumeraire_;
            Vector_<ExerciseRegression_> regressions_;
        };

        TrainingOutcome_ TrainFrozenPolicy(LsmcContext_& ctx, const PathCounts_& counts) {
            RunForwardPhase(ctx, BatchPlan_(counts.training_, 1));
            auto eventNumeraire = SampleGridNumeraires(ctx);
            LsmcStorage_ validationStorage;
            if (counts.validation_) {
                validationStorage = MakeStorage(ctx.scan_, counts.validation_);
                LsmcContext_ validationCtx{ctx.prepared_, ctx.model_, ctx.scan_, validationStorage, ctx.compiled_, nullptr, ctx.scrambleKey_};
                validationCtx.constValues_ = ctx.constValues_;
                RunForwardPhase(validationCtx, BatchPlan_(counts.validation_, 1), counts.training_);
            }
            auto regressions = RunBackwardPhase(ctx, eventNumeraire, counts.training_, ctx.prepared_.Simulation().lsmcBasisDegree_,
                                                counts.validation_ ? &validationStorage : nullptr, counts.validation_);
            return {std::move(eventNumeraire), std::move(regressions)};
        }

        //  First exercise wins (S3/S4): the earliest true decision replaces the payoff
        double PathPayoff(const LsmcContext_& ctx, const Vector_<>& eventNumeraire, const ThreadState_& state, Vector_<size_t>* exerciseCounts) {
            if (state.exercisedDay_ != NO_SLOT) {
                //  The event loop already selected the first exercise. Reusing it
                //  avoids a second scan and a second polynomial evaluation per date.
                const double payoff = state.preExercise_ + state.exerciseValue_ / eventNumeraire[ctx.scan_.days_[state.exercisedDay_].eventId_];
                REQUIRE2(std::isfinite(payoff), "InvalidPayoff: non-finite exercise value", ScriptError_);
                ++(*exerciseCounts)[state.exercisedDay_];
                return payoff;
            }
            if (!ctx.Product().HasPays())
                return 0.0;
            const double payoff = (state.compiledState_ ? state.compiledState_->VarVals() : state.evaluator_.VarVals())[ctx.PayOffIdx()];
            REQUIRE2(std::isfinite(payoff), "InvalidPayoff: non-finite path value", ScriptError_);
            return payoff;
        }

        struct ReplayOutcome_ {
            double sum_ = 0.0;
            double sumSq_ = 0.0;
            Vector_<size_t> exerciseCounts_;
        };

        struct ReplayWorkspace_ {
            LsmcStorage_ storage_;
            LsmcContext_ pricing_;
            ThreadState_ state_;

            ReplayWorkspace_(const LsmcContext_& ctx, const Vector_<ExerciseRegression_>& regressions)
                : pricing_{ctx.prepared_, ctx.model_, ctx.scan_, storage_, ctx.compiled_, &regressions, ctx.scrambleKey_}, state_(pricing_) {}
        };

        //  Phase C evaluates the frozen strategy on a disjoint Sobol block. Each
        //  worker reuses its evaluator and scalar state; batch-index reduction
        //  preserves thread invariance.
        ReplayOutcome_ RunReplayPhase(const LsmcContext_& ctx,
                                      const Vector_<>& eventNumeraire,
                                      const BatchPlan_& batchPlan,
                                      size_t pricingPathOffset,
                                      const Vector_<ExerciseRegression_>& regressions) {
            Vector_<ReplayOutcome_> outcomes(batchPlan.BatchCount());
            for (auto& outcome : outcomes)
                outcome.exerciseCounts_ = Vector_<size_t>(ctx.scan_.days_.size(), 0);

            ThreadPool_* pool = ThreadPool_::GetInstance();
            Vector_<std::unique_ptr<ReplayWorkspace_>> workspaces(pool->NumThreads());
            SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
            for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                tasks.Spawn([&, batch, batchIndex]() {
                    ReplayOutcome_& outcome = outcomes[batchIndex];
                    auto& local = workspaces[ThreadPool_::ThreadNum()];
                    if (!local)
                        local = std::make_unique<ReplayWorkspace_>(ctx, regressions);
                    auto& pricing = local->pricing_;
                    auto& state = local->state_;
                    state.random_->SkipTo(pricingPathOffset + batch.firstPath_);
                    for (size_t i = 0; i < batch.pathCount_; ++i) {
                        EvaluateRecordedPath(state, pricing, 0);
                        const double payoff = PathPayoff(pricing, eventNumeraire, state, &outcome.exerciseCounts_);
                        outcome.sum_ += payoff;
                        outcome.sumSq_ += payoff * payoff;
                    }
                    return true;
                });
            }
            tasks.Complete();

            //  N9: reduce the batch blocks in batch-index order
            ReplayOutcome_ reduction{0.0, 0.0, Vector_<size_t>(ctx.scan_.days_.size(), 0)};
            for (const auto& outcome : outcomes) {
                reduction.sum_ += outcome.sum_;
                reduction.sumSq_ += outcome.sumSq_;
                for (size_t k = 0; k < reduction.exerciseCounts_.size(); ++k)
                    reduction.exerciseCounts_[k] += outcome.exerciseCounts_[k];
            }
            return reduction;
        }

        void FillDiagnostics(LsmcDiagnostics_* diagnostics,
                             const PreparedScript_& prepared,
                             const LsmcPlan_& scan,
                             const Vector_<ExerciseRegression_>& regressions,
                             const Vector_<size_t>& exerciseCounts,
                             const ReplayOutcome_& reduction,
                             const PathCounts_& counts,
                             size_t pricingPathsPerReplicate,
                             const Vector_<>& replicateMeans) {
            diagnostics->events_.clear();
            diagnostics->payoffSum_ = reduction.sum_;
            diagnostics->payoffSumSq_ = reduction.sumSq_;
            diagnostics->nPaths_ = counts.replicates_ * pricingPathsPerReplicate;
            diagnostics->trainingPaths_ = counts.training_;
            diagnostics->validationPaths_ = counts.validation_;
            diagnostics->pricingPathsPerReplicate_ = pricingPathsPerReplicate;
            diagnostics->replicateCount_ = counts.replicates_;
            diagnostics->replicateMeans_ = replicateMeans;
            diagnostics->trainingSeed_ =
                counts.replicates_ > 1 ? std::optional<int>(prepared.Simulation().lsmcTrainingSeed_.value_or(0)) : std::nullopt;
            diagnostics->pricingSeed_ =
                counts.replicates_ > 1 ? std::optional<int>(prepared.Simulation().lsmcPricingSeed_.value_or(0)) : std::nullopt;
            diagnostics->scrambleIdentity_ = counts.replicates_ > 1 ? "sobol-digital-shift-splitmix64-v1" : "none";
            const auto& bindings = prepared.Plan().ModelBindingNames();
            for (size_t k = 0; k < scan.days_.size(); ++k) {
                ExerciseEventStats_ stats;
                stats.eventId_ = scan.days_[k].eventId_;
                stats.date_ = prepared.Product().EventDates()[scan.days_[k].eventId_];
                stats.requestedDegree_ = prepared.Simulation().lsmcBasisDegree_;
                stats.basisDegree_ = regressions[k].basisDegree_;
                stats.regressorIndex_ = bindings.empty() ? String_() : bindings.front();
                stats.numCondTruePaths_ = regressions[k].numCondTrue_;
                stats.coefficients_ = regressions[k].coefficients_;
                stats.degenerate_ = regressions[k].degenerate_;
                stats.degenerateReason_ = regressions[k].degenerateReason_;
                stats.effectiveRank_ = regressions[k].effectiveRank_;
                stats.solver_ = regressions[k].solver_;
                stats.fallbackReason_ = regressions[k].fallbackReason_;
                stats.validationMse_ = regressions[k].validationMse_;
                stats.exerciseRate_ = static_cast<double>(exerciseCounts[k]) / static_cast<double>(diagnostics->nPaths_);
                diagnostics->events_.push_back(stats);
            }
        }

        //  S9 recursive blend, live on the worker's tape: walks the recorded per-path rows
        //  backward, d_k = CSpr(h_k - C_k(z_k), eps) * CSpr(h_k, 0, eps) * condition degree
        //  — the fuzzy-AND of h_k > C_k with the h_k > 0 exercise gate (the one-sided ramp
        //  puts degree 0 on the h == 0 atom, matching the hard mode) — blending each fuzzy
        //  decision into the continuation; the explicit last step discounts to the
        //  evaluation date through the first event's numeraire (N5)
        template <class T_>
        T_ FuzzyPathValue(const LsmcPlan_& scan,
                          const Vector_<T_>& pays,
                          const Vector_<T_>& h,
                          const Vector_<T_>& cond,
                          const Vector_<ExerciseRegression_>& regressions,
                          const Vector_<size_t>& eventToSample,
                          const Scenario_<T_>& path) {
            T_ value(0.0);
            if (eventToSample.empty())
                return value;
            for (size_t e = scan.eventToExercise_.size(); e-- > 0;) {
                if (e + 1 < scan.eventToExercise_.size())
                    value *= path[eventToSample[e]].numeraire_ / path[eventToSample[e + 1]].numeraire_;
                value += pays[e];
                const size_t day = scan.eventToExercise_[e];
                if (day != NO_SLOT) {
                    //  materialize the continuation gap: CSpr's early-return constants need a
                    //  value type, not an expression proxy
                    const T_ continuation = RegressionPredict(regressions[day], path[eventToSample[e]].spot_);
                    const T_ gap = h[day] - continuation;
                    const T_ degree = CSpr(gap, scan.days_[day].eps_) * CSpr(h[day], 0.0, scan.days_[day].eps_) * cond[day];
                    value = degree * h[day] + (1.0 - degree) * value;
                }
            }
            return value / path[eventToSample.front()].numeraire_;
        }

        //  Phase C (fuzzy): raw adjoint sums per batch slot, reduced in batch-index order (N9)
        struct AadReplayOutcome_ {
            double sum_ = 0.0;
            Vector_<> risks_;
        };

        //  Per-worker replay state: active model, regenerated paths, and the per-path
        //  recording rows (payments reset between paths, h/cond overwritten per statement)
        template <class T_> struct FuzzyReplayWorkspace_ {
            std::unique_ptr<AAD::Model_<T_>> model_;
            std::unique_ptr<Random_> random_;
            Vector_<> gauss_;
            Scenario_<T_> path_;
            LsmcFuzzySinks_<T_> sinks_;
            Vector_<T_> pays_;
            Vector_<T_> h_;
            Vector_<T_> cond_;
        };

        template <class T_>
        FuzzyReplayWorkspace_<T_> MakeFuzzyReplayWorkspace(const PreparedScript_& prepared,
                                                           const Handle_<ModelData_>& modelData,
                                                           const LsmcPlan_& scan,
                                                           const PathBatch_& batch,
                                                           std::optional<uint64_t> scrambleKey) {
            const auto& simulation = prepared.Simulation();
            FuzzyReplayWorkspace_<T_> ws;
            ws.model_ = CreateModel<T_>(modelData);
            ws.model_->Allocate(prepared.TimeLine(), prepared.DefLine());
            if constexpr (std::is_same_v<T_, double>)
                ws.model_->Init(prepared.TimeLine(), prepared.DefLine());
            ws.random_ = CreateRNG(simulation.rsg_, *ws.model_, simulation.useBb_, scrambleKey);
            ws.gauss_.Resize(ws.model_->SimDim());
            AllocatePath(prepared.DefLine(), ws.path_);
            InitializePath(ws.path_);
            if (ws.random_)
                ws.random_->SkipTo(batch.firstPath_);
            ws.sinks_.eventToExercise_ = &scan.eventToExercise_;
            ws.sinks_.payoffIdx_ = prepared.PayOffIdx();
            ws.pays_ = Vector_<T_>(scan.eventToPays_.size(), 0.0);
            ws.h_ = Vector_<T_>(scan.days_.size(), 0.0);
            ws.cond_ = Vector_<T_>(scan.days_.size(), 1.0);
            return ws;
        }

        //  Mirrors the double recorder: the driver owns the event boundaries and the sinks'
        //  event ordinal while the fuzzy evaluator records through its installed sinks
        template <class T_>
        void TreeEvaluateFuzzyPath(FuzzyReplayWorkspace_<T_>& ws, const PreparedScript_& prepared, FuzzyEvaluator_<T_>& evaluator) {
            evaluator.lsmcFuzzySinks_ = &ws.sinks_;
            evaluator.SetScenario(&ws.path_);
            evaluator.SetObservations(&prepared.Plan());
            evaluator.Init();
            const auto& events = prepared.Product().Events();
            const auto& eventToSample = prepared.Plan().EventToSample();
            for (size_t e = 0; e < events.size(); ++e) {
                ws.sinks_.eventOrdinal_ = e;
                evaluator.SetCurEvt(eventToSample[e]);
                for (const auto& statement : events[e])
                    statement->Accept(evaluator);
            }
        }

        template <class T_>
        void CompiledEvaluateFuzzyPath(FuzzyReplayWorkspace_<T_>& ws,
                                       const PreparedScript_& prepared,
                                       const ScriptCompiled_& compiled,
                                       EvalState_<T_>& state) {
            state.lsmcFuzzySinks_ = &ws.sinks_;
            state.Init();
            state.observations_ = &prepared.Plan();
            state.scenario_ = &ws.path_;
            const auto& nodeStreams = compiled.NodeStreams();
            const auto& constStreams = compiled.ConstStreams();
            const auto& eventToSample = prepared.Plan().EventToSample();
            for (size_t e = 0; e < nodeStreams.size(); ++e) {
                ws.sinks_.eventOrdinal_ = e;
                const Detail::CompiledEventView_<T_> view{nodeStreams[e], constStreams[e], ws.path_[eventToSample[e]]};
                Detail::EvalCompiledEvents<true, true>(1, [&](size_t) { return view; }, &state);
            }
        }

        template <class E_, class F_>
        void FuzzyReplayPaths(const PreparedScript_& prepared,
                              const LsmcPlan_& scan,
                              const Vector_<ExerciseRegression_>& regressions,
                              const PathBatch_& batch,
                              FuzzyReplayWorkspace_<AAD::Number_>& ws,
                              E_& evaluator,
                              const F_& evaluate,
                              AadReplayOutcome_* outcome) {
            InitModel4ParallelAAD(prepared, *ws.model_, ws.path_, evaluator, nullptr);
            for (size_t i = 0; i < batch.pathCount_; ++i) {
                AAD::RewindToMark(*AAD::Tape());
                for (auto& payment : ws.pays_)
                    payment = 0.0;
                if (ws.random_)
                    ws.random_->FillNormal(&ws.gauss_);
                ws.model_->GeneratePath(ws.gauss_, &ws.path_);
                ValidateSimulationPath(ws.path_);
                evaluate(ws, prepared, evaluator);
                AAD::Number_ value = FuzzyPathValue(scan, ws.pays_, ws.h_, ws.cond_, regressions, prepared.Plan().EventToSample(), ws.path_);
                REQUIRE2(std::isfinite(Value(value)), "InvalidPayoff: non-finite path value", ScriptError_);
                Adjoint(value) = 1.0;
                AAD::PropagateToMark(*AAD::Tape());
                outcome->sum_ += Value(value);
            }
            AAD::PropagateMarkToStart(*AAD::Tape());
            size_t j = 0;
            for (const auto* parameter : ws.model_->Parameters())
                outcome->risks_[j++] += Adjoint(*parameter);
            for (const auto& constVar : evaluator.ConstVarVals())
                outcome->risks_[j++] += Adjoint(constVar);
        }

        void RunFuzzyReplayBatch(const PreparedScript_& prepared,
                                 const Handle_<ModelData_>& modelData,
                                 const LsmcPlan_& scan,
                                 const Vector_<ExerciseRegression_>& regressions,
                                 const ScriptCompiled_* fuzzyCompiled,
                                 const PathBatch_& batch,
                                 std::optional<uint64_t> scrambleKey,
                                 AadReplayOutcome_* outcome) {
            AAD::Activate(*AAD::Tape());
            AAD::Rewind(*AAD::Tape());
            FuzzyReplayWorkspace_<AAD::Number_> ws = MakeFuzzyReplayWorkspace<AAD::Number_>(prepared, modelData, scan, batch, scrambleKey);
            //  Bind after the return-by-value, without relying on optional NRVO.
            ws.sinks_.pays_ = &ws.pays_;
            ws.sinks_.h_ = &ws.h_;
            ws.sinks_.cond_ = &ws.cond_;
            if (fuzzyCompiled) {
                EvalState_<AAD::Number_> state = prepared.BuildEvalState<AAD::Number_>(0, prepared.Simulation().smooth_);
                FuzzyReplayPaths(
                    prepared, scan, regressions, batch, ws, state,
                    [fuzzyCompiled](FuzzyReplayWorkspace_<AAD::Number_>& w, const PreparedScript_& p, EvalState_<AAD::Number_>& s) {
                        CompiledEvaluateFuzzyPath(w, p, *fuzzyCompiled, s);
                    },
                    outcome);
            } else {
                FuzzyEvaluator_<AAD::Number_> evaluator = prepared.BuildFuzzyEvaluator<AAD::Number_>(0, prepared.Simulation().smooth_);
                FuzzyReplayPaths(prepared, scan, regressions, batch, ws, evaluator, TreeEvaluateFuzzyPath<AAD::Number_>, outcome);
            }
        }

        template <class E_, class F_>
        double ValueFuzzyReplayPaths(const PreparedScript_& prepared,
                                     const LsmcPlan_& scan,
                                     const Vector_<ExerciseRegression_>& regressions,
                                     const PathBatch_& batch,
                                     FuzzyReplayWorkspace_<double>& ws,
                                     E_& evaluator,
                                     const F_& evaluate) {
            double sum = 0.0;
            for (size_t i = 0; i < batch.pathCount_; ++i) {
                std::fill(ws.pays_.begin(), ws.pays_.end(), 0.0);
                ws.random_->FillNormal(&ws.gauss_);
                ws.model_->GeneratePath(ws.gauss_, &ws.path_);
                ValidateSimulationPath(ws.path_);
                evaluate(ws, prepared, evaluator);
                const double value = FuzzyPathValue(scan, ws.pays_, ws.h_, ws.cond_, regressions, prepared.Plan().EventToSample(), ws.path_);
                REQUIRE2(std::isfinite(value), "InvalidPayoff: non-finite path value", ScriptError_);
                sum += value;
            }
            return sum;
        }

        double RunValueFuzzyReplayBatch(const PreparedScript_& prepared,
                                        const Handle_<ModelData_>& modelData,
                                        const LsmcPlan_& scan,
                                        const Vector_<ExerciseRegression_>& regressions,
                                        const ScriptCompiled_* fuzzyCompiled,
                                        const PathBatch_& batch,
                                        std::optional<uint64_t> scrambleKey) {
            auto ws = MakeFuzzyReplayWorkspace<double>(prepared, modelData, scan, batch, scrambleKey);
            ws.sinks_.pays_ = &ws.pays_;
            ws.sinks_.h_ = &ws.h_;
            ws.sinks_.cond_ = &ws.cond_;
            if (fuzzyCompiled) {
                auto state = prepared.BuildEvalState<double>(0, prepared.Simulation().smooth_);
                return ValueFuzzyReplayPaths(prepared, scan, regressions, batch, ws, state,
                                             [fuzzyCompiled](FuzzyReplayWorkspace_<double>& w, const PreparedScript_& p, EvalState_<double>& s) {
                                                 CompiledEvaluateFuzzyPath(w, p, *fuzzyCompiled, s);
                                             });
            }
            auto evaluator = prepared.BuildFuzzyEvaluator<double>(0, prepared.Simulation().smooth_);
            return ValueFuzzyReplayPaths(prepared, scan, regressions, batch, ws, evaluator, TreeEvaluateFuzzyPath<double>);
        }

        double ValueFuzzyPolicy(const PreparedScript_& prepared,
                                const Handle_<ModelData_>& modelData,
                                const LsmcPlan_& scan,
                                const Vector_<ExerciseRegression_>& regressions,
                                const ScriptCompiled_* fuzzyCompiled,
                                const BatchPlan_& batchPlan,
                                const PathCounts_& counts,
                                size_t nPaths) {
            double sum = 0.0;
            for (size_t replicate = 0; replicate < counts.replicates_; ++replicate) {
                const std::optional<uint64_t> pricingKey =
                    counts.replicates_ > 1
                        ? std::optional<uint64_t>(LsmcScrambleKey(true, prepared.Simulation().lsmcPricingSeed_.value_or(0), replicate))
                        : std::nullopt;
                Vector_<> batchSums(batchPlan.BatchCount(), 0.0);
                SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), batchPlan.BatchCount());
                for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                    const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                    tasks.Spawn([&, batch, batchIndex]() {
                        const PathBatch_ pricingBatch{counts.pricingOffset_ + replicate * nPaths + batch.firstPath_, batch.pathCount_};
                        batchSums[batchIndex] =
                            RunValueFuzzyReplayBatch(prepared, modelData, scan, regressions, fuzzyCompiled, pricingBatch, pricingKey);
                        return true;
                    });
                }
                tasks.Complete();
                for (double batchSum : batchSums)
                    sum += batchSum;
            }
            return sum / static_cast<double>(counts.replicates_ * nPaths);
        }

        Vector_<ExerciseRegression_> TrainBumpedPolicy(const PreparedScript_& prepared,
                                                       const AAD::Model_<double>& baseModel,
                                                       const LsmcPlan_& scan,
                                                       const ScriptCompiled_* hardCompiled,
                                                       const PathCounts_& counts,
                                                       std::optional<uint64_t> trainingKey,
                                                       size_t parameter,
                                                       double bump) {
            auto model = baseModel.Clone();
            *model->Parameters()[parameter] += bump;
            model->Init(prepared.TimeLine(), prepared.DefLine());
            auto storage = MakeStorage(scan, counts.training_);
            LsmcContext_ ctx{prepared, model.get(), scan, storage, hardCompiled, nullptr, trainingKey};
            return TrainFrozenPolicy(ctx, counts).regressions_;
        }

        Vector_<ExerciseRegression_> TrainBumpedConstantPolicy(const PreparedScript_& prepared,
                                                               AAD::Model_<double>* model,
                                                               const LsmcPlan_& scan,
                                                               const ScriptCompiled_* hardCompiled,
                                                               const PathCounts_& counts,
                                                               std::optional<uint64_t> trainingKey,
                                                               size_t constant,
                                                               double bump) {
            auto constants = prepared.Product().ConstVarValues();
            constants[constant] += bump;
            auto storage = MakeStorage(scan, counts.training_);
            LsmcContext_ ctx{prepared, model, scan, storage, hardCompiled, nullptr, trainingKey, &constants};
            return TrainFrozenPolicy(ctx, counts).regressions_;
        }

        bool InvalidLowerModelBump(const AAD::Model_<double>& model, size_t parameter, double lower) {
            if (parameter == 0)
                return lower <= 0.0;
            if (typeid(model) == typeid(AAD::BlackScholes_<double>))
                return parameter == 1 && lower < 0.0;
            return parameter >= 3 && lower < 0.0; // Dupire local-volatility grid
        }

        bool ValidModelPolicyBump(double parameter, double step) {
            return std::isfinite(step) && std::isfinite(parameter + step) && parameter + step != parameter;
        }

        bool ValidConstantPolicyBump(double constant, double step) {
            return std::isfinite(step) && std::isfinite(constant + step) && std::isfinite(constant - step) && constant + step != constant &&
                   constant - step != constant;
        }

        void AddRetrainedPolicyRisks(const PreparedScript_& prepared,
                                     const Handle_<ModelData_>& modelData,
                                     AAD::Model_<double>& baseModel,
                                     const LsmcPlan_& scan,
                                     const Vector_<ExerciseRegression_>& basePolicy,
                                     const ScriptCompiled_* hardCompiled,
                                     const ScriptCompiled_* fuzzyCompiled,
                                     const BatchPlan_& batchPlan,
                                     const PathCounts_& counts,
                                     std::optional<uint64_t> trainingKey,
                                     size_t nPaths,
                                     SimResults_* results) {
            std::optional<double> basePolicyValue;
            const double relative = prepared.Simulation().lsmcPolicyBumpRelative_;
            for (size_t j = 0; j < baseModel.NumParams(); ++j) {
                const double parameter = *baseModel.Parameters()[j];
                const double step = relative * std::max(1.0, std::abs(parameter));
                REQUIRE2(ValidModelPolicyBump(parameter, step), "InvalidLsmcPolicyBump: model parameter cannot be bumped", ScriptError_);
                const auto upPolicy = TrainBumpedPolicy(prepared, baseModel, scan, hardCompiled, counts, trainingKey, j, step);
                const double up = ValueFuzzyPolicy(prepared, modelData, scan, upPolicy, fuzzyCompiled, batchPlan, counts, nPaths);
                double down;
                double denominator = 2.0 * step;
                if (InvalidLowerModelBump(baseModel, j, parameter - step)) {
                    if (!basePolicyValue)
                        basePolicyValue = ValueFuzzyPolicy(prepared, modelData, scan, basePolicy, fuzzyCompiled, batchPlan, counts, nPaths);
                    down = *basePolicyValue;
                    denominator = step;
                } else {
                    const auto downPolicy = TrainBumpedPolicy(prepared, baseModel, scan, hardCompiled, counts, trainingKey, j, -step);
                    down = ValueFuzzyPolicy(prepared, modelData, scan, downPolicy, fuzzyCompiled, batchPlan, counts, nPaths);
                }
                results->risks_[j] += (up - down) / denominator;
            }
            const auto& constants = prepared.Product().ConstVarValues();
            for (size_t k = 0; k < constants.size(); ++k) {
                const double step = relative * std::max(1.0, std::abs(constants[k]));
                REQUIRE2(ValidConstantPolicyBump(constants[k], step), "InvalidLsmcPolicyBump: script constant cannot be bumped", ScriptError_);
                const auto upPolicy = TrainBumpedConstantPolicy(prepared, &baseModel, scan, hardCompiled, counts, trainingKey, k, step);
                const auto downPolicy = TrainBumpedConstantPolicy(prepared, &baseModel, scan, hardCompiled, counts, trainingKey, k, -step);
                const double up = ValueFuzzyPolicy(prepared, modelData, scan, upPolicy, fuzzyCompiled, batchPlan, counts, nPaths);
                const double down = ValueFuzzyPolicy(prepared, modelData, scan, downPolicy, fuzzyCompiled, batchPlan, counts, nPaths);
                results->risks_[baseModel.NumParams() + k] += (up - down) / (2.0 * step);
            }
        }

        void AccumulateFuzzyReplicate(const PreparedScript_& prepared,
                                      const Handle_<ModelData_>& modelData,
                                      const LsmcPlan_& scan,
                                      const Vector_<ExerciseRegression_>& regressions,
                                      const ScriptCompiled_* fuzzyCompiled,
                                      const BatchPlan_& batchPlan,
                                      size_t pricingOffset,
                                      std::optional<uint64_t> pricingKey,
                                      SimResults_* results,
                                      Vector_<>* riskTotals) {
            Vector_<AadReplayOutcome_> outcomes(batchPlan.BatchCount());
            for (auto& outcome : outcomes)
                outcome.risks_ = Vector_<>(results->risks_.size(), 0.0);

            SimulationTaskGroup_ tasks(ThreadPool_::GetInstance(), batchPlan.BatchCount());
            for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                tasks.Spawn([&, batch, batchIndex]() {
                    const PathBatch_ pricingBatch{pricingOffset + batch.firstPath_, batch.pathCount_};
                    RunFuzzyReplayBatch(prepared, modelData, scan, regressions, fuzzyCompiled, pricingBatch, pricingKey, &outcomes[batchIndex]);
                    return true;
                });
            }
            tasks.Complete();

            for (const auto& outcome : outcomes)
                results->aggregated_ += outcome.sum_;
            for (size_t j = 0; j < results->risks_.size(); ++j)
                for (const auto& outcome : outcomes)
                    (*riskTotals)[j] += outcome.risks_[j];
        }

        //  N5: the probe-path discount ratios are path-independent only for
        //  deterministic-rate models; the model base class exposes no rate-kind
        //  query, so the factory's two models are pinned here unconditionally --
        //  a stochastic-rate model would otherwise be silently mis-discounted
        void RequireDeterministicRateModel(const AAD::Model_<double>& model) {
            REQUIRE2(typeid(model) == typeid(AAD::BlackScholes_<double>) || typeid(model) == typeid(AAD::Dupire_<double>),
                     "UnsupportedModel: LSMC requires a deterministic-rate model (BlackScholes or Dupire)", ScriptError_);
        }
    } // namespace

    ExerciseRegression_ SolveExerciseRegression(const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included, int degree) {
        ValidateLsmcBasisDegree(degree);
        REQUIRE(x.size() == targets.size() && x.size() == included.size(), "InvalidRegressionInput: mismatched regression vectors");
        const auto rows = Rows(x, targets, included);
        const PathChunks_ chunks(rows.n_);
        const auto sums = ReduceSums(chunks.Map<IncludedSums_>([&](const PathBatch_& c) { return SumIncluded(SubRows(rows, c)); }));
        return FitRegression(rows, sums, degree, chunks);
    }

    SimResults_ MCLsmcSimulation(const PreparedScript_& prepared, AAD::Model_<double>* mdl, size_t nPaths, LsmcDiagnostics_* diagnostics) {
        const auto& product = prepared.Product();
        const auto& simulation = prepared.Simulation();
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of Monte Carlo paths must be positive", ScriptError_);
        REQUIRE2(!simulation.enableAad_,
                 "UnsupportedExecutionMode: the double LSMC driver values hard decisions only; AAD products route to the fuzzy driver", ScriptError_);
        RequireDeterministicRateModel(*mdl);

        const auto scan = ScanEvents(product.Events(), simulation.smooth_);
        const auto counts = LsmcPathCounts(simulation, nPaths);
        auto storage = MakeStorage(scan, counts.training_);
        const BatchPlan_ batchPlan(nPaths, 1);
        const ScriptCompiled_* compiled = simulation.compiled_.value_or(false) ? &prepared.CompiledProgram() : nullptr;
        const std::optional<uint64_t> trainingKey =
            counts.replicates_ > 1 ? std::optional<uint64_t>(LsmcScrambleKey(false, simulation.lsmcTrainingSeed_.value_or(0))) : std::nullopt;
        LsmcContext_ ctx{prepared, mdl, scan, storage, compiled, nullptr, trainingKey};
        const auto trained = TrainFrozenPolicy(ctx, counts);
        storage = LsmcStorage_();
        ReplayOutcome_ reduction;
        Vector_<> replicateMeans;
        if (counts.replicates_ == 1) {
            reduction = RunReplayPhase(ctx, trained.eventNumeraire_, batchPlan, counts.pricingOffset_, trained.regressions_);
        } else {
            reduction.exerciseCounts_ = Vector_<size_t>(scan.days_.size(), 0);
            for (size_t replicate = 0; replicate < counts.replicates_; ++replicate) {
                auto pricingCtx = ctx;
                pricingCtx.scrambleKey_ = LsmcScrambleKey(true, simulation.lsmcPricingSeed_.value_or(0), replicate);
                const auto one =
                    RunReplayPhase(pricingCtx, trained.eventNumeraire_, batchPlan, counts.pricingOffset_ + replicate * nPaths, trained.regressions_);
                replicateMeans.push_back(one.sum_ / static_cast<double>(nPaths));
                reduction.sum_ += one.sum_;
                reduction.sumSq_ += one.sumSq_;
                for (size_t day = 0; day < reduction.exerciseCounts_.size(); ++day)
                    reduction.exerciseCounts_[day] += one.exerciseCounts_[day];
            }
        }

        SimResults_ results(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        results.aggregated_ = reduction.sum_ / static_cast<double>(counts.replicates_);
        if (diagnostics)
            FillDiagnostics(diagnostics, prepared, scan, trained.regressions_, reduction.exerciseCounts_, reduction, counts, nPaths, replicateMeans);
        return results;
    }

    SimResults_ MCLsmcAadSimulation(const PreparedScript_& prepared, const Handle_<ModelData_>& modelData, size_t nPaths) {
        const auto& product = prepared.Product();
        const auto& simulation = prepared.Simulation();
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of Monte Carlo paths must be positive", ScriptError_);
        REQUIRE2(simulation.enableAad_, "UnsupportedExecutionMode: the fuzzy LSMC driver requires AAD preparation", ScriptError_);

        //  Phases A/B run exactly as the double driver so the frozen policy is the
        //  thread-count independent hard-decision artifact; the recording stream
        //  therefore lowers hard even though the replay itself is fuzzy
        auto doubleModel = CreateModel<double>(modelData);
        RequireDeterministicRateModel(*doubleModel);
        doubleModel->Allocate(prepared.TimeLine(), prepared.DefLine());
        doubleModel->Init(prepared.TimeLine(), prepared.DefLine());

        const auto scan = ScanEvents(product.Events(), simulation.smooth_);
        const auto counts = LsmcPathCounts(simulation, nPaths);
        auto storage = MakeStorage(scan, counts.training_);
        const BatchPlan_ batchPlan(nPaths, 1);
        std::optional<ScriptCompiled_> hardCompiled;
        if (simulation.compiled_.value_or(false))
            hardCompiled.emplace(ScriptCompiled_::Build(product.Events(), false, prepared.PlanHandle(), false, true));
        const std::optional<uint64_t> trainingKey =
            counts.replicates_ > 1 ? std::optional<uint64_t>(LsmcScrambleKey(false, simulation.lsmcTrainingSeed_.value_or(0))) : std::nullopt;
        LsmcContext_ ctx{prepared, doubleModel.get(), scan, storage, hardCompiled ? &*hardCompiled : nullptr, nullptr, trainingKey};
        const auto trained = TrainFrozenPolicy(ctx, counts);
        storage = LsmcStorage_();

        const ScriptCompiled_* fuzzyCompiled = simulation.compiled_.value_or(false) ? &prepared.CompiledProgram(true) : nullptr;
        SimResults_ results(Vector::Join(doubleModel->ParameterLabels(), product.ConstVarNames()));
        Vector_<> riskTotals(results.risks_.size(), 0.0);
        for (size_t replicate = 0; replicate < counts.replicates_; ++replicate) {
            const std::optional<uint64_t> pricingKey =
                counts.replicates_ > 1 ? std::optional<uint64_t>(LsmcScrambleKey(true, simulation.lsmcPricingSeed_.value_or(0), replicate))
                                       : std::nullopt;
            AccumulateFuzzyReplicate(prepared, modelData, scan, trained.regressions_, fuzzyCompiled, batchPlan,
                                     counts.pricingOffset_ + replicate * nPaths, pricingKey, &results, &riskTotals);
        }
        results.aggregated_ /= static_cast<double>(counts.replicates_);
        for (size_t j = 0; j < results.risks_.size(); ++j)
            results.risks_[j] = riskTotals[j] / static_cast<double>(counts.replicates_ * nPaths);
        if (simulation.lsmcPolicyRiskMode_ == "RetrainedBump")
            AddRetrainedPolicyRisks(prepared, modelData, *doubleModel, scan, trained.regressions_, hardCompiled ? &*hardCompiled : nullptr,
                                    fuzzyCompiled, batchPlan, counts, trainingKey, nPaths, &results);
        return results;
    }
} // namespace Dal::Script
