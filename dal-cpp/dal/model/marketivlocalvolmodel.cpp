#include <dal/platform/platform.hpp>

#include <dal/concurrency/threadpool.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/model/marketivlocalvolcomponent.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
#include <dal/time/datetime.hpp>

namespace Dal::AAD {
    namespace {
        using namespace MarketIvLocalVolDetail;

        void CheckTime(double time, double horizon, const String_& field) {
            if (!std::isfinite(time) || time < 0.)
                Fail("InvalidMarketIvLocalVolRequest", "Prepare", field, "finite nonnegative time", ToString(time));
            if (time > horizon)
                Fail("MarketIvLocalVolHorizonExceeded", "Prepare", field, "time <= " + ToString(horizon), ToString(time));
        }

        bool Supports(const Index_& index, const MarketIvLocalVolModelSettings_& settings) {
            if (const auto* df = dynamic_cast<const Index::DF_*>(&index))
                return df->ccy_.String() == settings.currency_;
            return IsPlainEquity(index) && index.Name() == settings.index_;
        }

        std::pair<double, double> DiscountInterval(const Index::DF_& df, double time, const Date_& date, double horizon) {
            const double days = time * 365.;
            if (days > Date::Maximum() - date || std::abs(days - std::round(days)) > 1e-7)
                Fail("InvalidMarketIvLocalVolRequest", "Prepare", "/product/discount_observations", "calendar sample date", ToString(time));
            const DateTime_ sample(date.AddDays(static_cast<int>(std::round(days))));
            const double start = (df.StartDate(sample) - date) / 365., maturity = (df.Maturity(sample) - date) / 365.;
            CheckTime(start, horizon, "/product/discount_start");
            CheckTime(maturity, horizon, "/product/discount_maturity");
            if (start < time || maturity < start)
                Fail("InvalidMarketIvLocalVolRequest", "Prepare", "/product/discount_maturity", "maturity >= start >= sample", ToString(maturity));
            return {start, maturity};
        }

        // Keep curve work in preparation; path evaluation only copies frozen discount values.
        class MarketRate_ final : public HybridComponent_<double> {
            HybridLogDfRate_<double> rate_;
            Date_ date_;
            double horizon_;
            Vector_<> timeline_, observations_;
            Vector_<Vector_<>> discounts_;

        public:
            MarketRate_(const Source_& source, const HybridLogDfRateData_& data)
                : rate_(data), date_(source.Snapshot().Request().market_.EvaluationDate()), horizon_(source.Configuration().horizon_) {}
            [[nodiscard]] const String_& Name() const override { return rate_.Name(); }
            [[nodiscard]] const String_& Currency() const override { return rate_.Currency(); }
            [[nodiscard]] size_t StateDim() const override { return 0; }
            [[nodiscard]] size_t FactorDim() const override { return 0; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return rate_.FactorNames(); }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return rate_.ObservableNames(); }
            [[nodiscard]] const Vector_<double*>& Parameters() const override { return rate_.Parameters(); }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return rate_.ParameterLabels(); }
            [[nodiscard]] bool ProvidesNumeraire() const override { return true; }
            [[nodiscard]] bool NumeraireIsDeterministic() const override { return true; }
            [[nodiscard]] bool ProvidesDiscountFactors() const override { return true; }
            [[nodiscard]] bool SupportsIndex(const Index_& index) const override {
                const auto* df = dynamic_cast<const Index::DF_*>(&index);
                return df && df->ccy_.String() == Currency();
            }
            [[nodiscard]] double LogDiscount(double time) const override { return rate_.LogDiscount(time); }
            void BeginAllocate(const Vector_<>& timeline) override {
                timeline_ = timeline;
                discounts_.Resize(timeline.size());
                observations_.clear();
            }
            size_t RegisterObservation(size_t sample, const String_& name) override {
                const Handle_<Index_> index(Index::Parse(name));
                const auto* df = dynamic_cast<const Index::DF_*>(index.get());
                REQUIRE(df && SupportsIndex(*df), "UnsupportedModelObservation: " + name);
                const auto interval = DiscountInterval(*df, timeline_[sample], date_, horizon_);
                observations_.push_back(
                    CheckedExp(LogDiscount(interval.second) - LogDiscount(interval.first), "/product/discount_observations", "Prepare"));
                return observations_.size() - 1;
            }
            void RegisterDiscounts(size_t sample, const Vector_<>& maturities) override {
                auto& values = discounts_[sample];
                values.reserve(maturities.size());
                for (const double maturity : maturities)
                    values.push_back(CheckedExp(LogDiscount(maturity) - LogDiscount(timeline_[sample]), "/product/discount_maturities", "Prepare"));
            }
            void FillDiscounts(size_t sample, double, const Vector_<>&, const Vector_<>&, size_t, Vector_<>* values) const override {
                std::copy(discounts_[sample].begin(), discounts_[sample].end(), values->begin());
            }
            void Prepare(const Vector_<>&, const Vector_<>&) override {}
            void ResetState(Vector_<>*, size_t) const override {}
            void Evolve(size_t, const Vector_<>&, const Vector_<size_t>&, const double&, Vector_<>*, size_t) const override {}
            [[nodiscard]] double Observe(size_t, size_t slot, const Vector_<>&, size_t, bool) const override { return observations_[slot]; }
            [[nodiscard]] std::unique_ptr<HybridComponent_<double>> Clone() const override { return std::make_unique<MarketRate_>(*this); }
        };

        std::unique_ptr<HybridModel_<double>> Delegate(const Source_& source, const Vector_<>& anchors) {
            const auto settings = source.Configuration();
            Vector_<> times(source.Carry().Times()), logs(source.Carry().LogDiscounts());
            if (times.back() < settings.horizon_) {
                times.push_back(settings.horizon_);
                logs.push_back(source.Carry().At(settings.horizon_).logDiscount_);
            }
            if (source.Snapshot().Request().market_.IsFlatCarry())
                for (size_t i = 0; i < times.size(); ++i)
                    logs[i] = source.Carry().At(times[i]).logDiscount_;
            const HybridLogDfRateData_ rate("rate", settings.currency_, times, logs, "LOG_LINEAR");
            Vector_<std::unique_ptr<HybridComponent_<double>>> components;
            components.push_back(std::make_unique<MarketRate_>(source, rate));
            components.push_back(std::make_unique<TermCarryEquity_>(source.Snapshot().Request().market_, source.Surface(), settings, anchors));
            const HybridConstantCorrelationData_ correlation("correlation", {settings.factor_}, Matrix_<>(1, 1, 1.));
            return std::make_unique<HybridModel_<double>>(settings.currency_, std::move(components), correlation);
        }

        double ObservationRequiredTime(const Source_& source, double time, const SampleDef_& definition) {
            const auto settings = source.Configuration();
            double required = time;
            for (const auto& name : definition.indexNames_) {
                const Handle_<Index_> index(Index::Parse(name));
                if (!index || !Supports(*index, settings))
                    Fail("InvalidMarketIvLocalVolRequest", "Prepare", "/product/index", settings.index_ + " or domestic DF", name);
                if (const auto* df = dynamic_cast<const Index::DF_*>(index.get()))
                    required = std::max(required,
                                        DiscountInterval(*df, time, source.Snapshot().Request().market_.EvaluationDate(), settings.horizon_).second);
            }
            if (!definition.liborDefs_.empty() || !definition.forwardMats_.empty())
                Fail("InvalidMarketIvLocalVolRequest", "Prepare", "/product/definitions", "equity and domestic DF only", "rate/forward request");
            return required;
        }

        double RequiredTime(const Source_& source, const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) {
            const auto settings = source.Configuration();
            double required = 0.;
            for (size_t sample = 0; sample < timeline.size(); ++sample) {
                CheckTime(timeline[sample], settings.horizon_, "/product/timeline/" + String_(std::to_string(sample)));
                required = std::max(required, timeline[sample]);
                for (const double maturity : definitions[sample].discountMats_) {
                    CheckTime(maturity, settings.horizon_, "/product/discount_maturities/" + String_(std::to_string(sample)));
                    required = std::max(required, maturity);
                }
                required = std::max(required, ObservationRequiredTime(source, timeline[sample], definitions[sample]));
            }
            return required;
        }

        void AddKnots(Vector_<>* anchors, const Vector_<>& knots, double required, size_t* count) {
            for (const double time : knots)
                if (time > 0. && time <= required) {
                    anchors->push_back(time);
                    ++*count;
                }
        }

        size_t StepCount(double from, double to, const MarketIvLocalVolModelSettings_& settings) {
            const double count = std::ceil((to - from) / settings.maxStep_);
            const size_t limit =
                std::min({settings.maxInternalSteps_, settings.maxSimulationDimension_, static_cast<size_t>(std::numeric_limits<int>::max())});
            if (!std::isfinite(count) || count < 1. || count > static_cast<double>(limit))
                Fail("MarketIvLocalVolBudgetExceeded", "Prepare", "/settings/max_internal_steps", String_(std::to_string(limit)), ToString(count));
            return static_cast<size_t>(count);
        }

        void CheckRng(size_t dimension, const String_& rsg) {
            // Sobol's implementation requires size < N_KNOWN (21201).
            if (rsg == "sobol" && dimension >= 21201)
                Fail("MarketIvLocalVolBudgetExceeded", "Prepare", "/simulation/rsg", "Sobol dimension < 21201", String_(std::to_string(dimension)));
        }

        void CheckPathAddress(size_t paths, size_t dimension, const String_& rsg) {
            const size_t maximum = std::numeric_limits<size_t>::max();
            if (rsg == "sobol" && paths > std::numeric_limits<uint32_t>::max())
                Fail("MarketIvLocalVolBudgetExceeded", "Prepare", "/simulation/rsg", "Sobol path ordinal fits uint32",
                     String_(std::to_string(paths)));
            if (dimension && paths > maximum / dimension)
                Fail("MarketIvLocalVolBudgetExceeded", "Prepare", "/simulation/rsg", "representable path/dimension address", "overflow");
        }

        size_t AnchorCapacity(const Source_& source, const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) {
            size_t count = CheckedAdd(2, CheckedAdd(timeline.size(), CheckedAdd(source.Carry().Times().size(), source.Surface().times_.size())));
            for (const auto& definition : definitions)
                count = CheckedAdd(count, CheckedAdd(definition.discountMats_.size(), CheckedMultiply(2, definition.indexNames_.size())));
            CheckLimit(CheckedMultiply(count, 2 * sizeof(double)), source.Configuration().workspaceBudgetBytes_, "/settings/workspace_budget_bytes",
                       "Prepare");
            return count;
        }

        MarketIvLocalVolExecution_ Plan(
            const Source_& source, const Vector_<>& timeline, const Vector_<SampleDef_>& definitions, const String_& rsg, Vector_<>* plannedAnchors) {
            const auto settings = source.Configuration();
            MarketIvLocalVolExecution_ result;
            result.sourceIdentity_ = source.SourceIdentity();
            result.modelIdentity_ = source.ModelIdentity();
            result.evaluationDate_ = source.Snapshot().Request().market_.EvaluationDate();
            result.requestedMaxStep_ = settings.maxStep_;
            result.requiredTime_ = RequiredTime(source, timeline, definitions);
            Vector_<> anchors;
            anchors.reserve(AnchorCapacity(source, timeline, definitions));
            anchors.push_back(0.);
            anchors.push_back(result.requiredTime_);
            anchors.Append(timeline);
            for (const auto& definition : definitions)
                anchors.Append(definition.discountMats_);
            for (size_t sample = 0; sample < definitions.size(); ++sample)
                for (const auto& name : definitions[sample].indexNames_) {
                    const Handle_<Index_> index(Index::Parse(name));
                    if (const auto* df = dynamic_cast<const Index::DF_*>(index.get())) {
                        const auto interval = DiscountInterval(*df, timeline[sample], *result.evaluationDate_, settings.horizon_);
                        anchors.push_back(interval.first);
                        anchors.push_back(interval.second);
                    }
                }
            AddKnots(&anchors, source.Carry().Times(), result.requiredTime_, &result.carryKnots_);
            AddKnots(&anchors, source.Surface().times_, result.requiredTime_, &result.gridTimeKnots_);
            std::sort(anchors.begin(), anchors.end());
            anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());
            result.anchorCount_ = anchors.size();
            for (size_t i = 1; i < anchors.size(); ++i) {
                const size_t steps = StepCount(anchors[i - 1], anchors[i], settings);
                result.internalSteps_ = CheckedAdd(result.internalSteps_, steps);
                result.resolvedMaxStep_ = std::max(result.resolvedMaxStep_, (anchors[i] - anchors[i - 1]) / steps);
            }
            CheckLimit(result.internalSteps_, settings.maxInternalSteps_, "/settings/max_internal_steps", "Prepare");
            CheckLimit(result.internalSteps_, settings.maxSimulationDimension_, "/settings/max_simulation_dimension", "Prepare");
            result.simulationDimension_ = result.internalSteps_;
            CheckLimit(result.simulationDimension_, static_cast<size_t>(std::numeric_limits<int>::max()), "/settings/max_simulation_dimension",
                       "Prepare");
            CheckRng(result.simulationDimension_, rsg);
            result.preparedColumns_ = result.internalSteps_;
            result.preparedMarketQueries_ = CheckedMultiply(4, CheckedAdd(result.internalSteps_, 1));
            result.plannedNumericPayloadBytes_ = source.ConstructionExecution().plannedNumericPayloadBytes_;
            result.numericPayloadBytes_ = source.ConstructionExecution().numericPayloadBytes_;
            plannedAnchors->Swap(&anchors);
            return result;
        }

        size_t OutputCount(const Vector_<SampleDef_>& definitions) {
            size_t count = 0;
            for (const auto& definition : definitions)
                count = CheckedAdd(count, CheckedAdd(definition.indexNames_.size(), definition.discountMats_.size()));
            return count;
        }

        size_t PreparedQueries(const Source_& source, size_t steps, const Vector_<SampleDef_>& definitions) {
            size_t queries = CheckedAdd(CheckedMultiply(3, steps), 1);
            for (const auto& definition : definitions) {
                queries = CheckedAdd(queries, CheckedMultiply(2, definition.discountMats_.size()));
                for (const auto& name : definition.indexNames_) {
                    const Handle_<Index_> index(Index::Parse(name));
                    if (dynamic_cast<const Index::DF_*>(index.get()))
                        queries = CheckedAdd(queries, 2);
                }
            }
            const bool terminal = source.Configuration().horizon_ > source.Carry().Times().back();
            queries = CheckedAdd(queries, static_cast<size_t>(terminal));
            if (source.Snapshot().Request().market_.IsFlatCarry())
                queries = CheckedAdd(queries, CheckedAdd(source.Carry().Times().size(), static_cast<size_t>(terminal)));
            return queries;
        }

        size_t PreparationEnvelope(const Source_& source, const MarketIvLocalVolExecution_& execution, const Vector_<SampleDef_>& definitions) {
            const size_t grid = CheckedMultiply(source.Surface().spots_.size(), source.Surface().times_.size());
            const size_t columns = CheckedMultiply(source.Surface().spots_.size(), execution.internalSteps_);
            // Growth, old/new temporaries and nested-vector metadata are included in each capacity envelope.
            const size_t axes = CheckedAdd(source.Carry().Times().size(), CheckedAdd(source.Surface().spots_.size(), source.Surface().times_.size()));
            const size_t buffers = CheckedAdd(CheckedMultiply(4 * sizeof(double), columns), CheckedMultiply(128, CheckedAdd(grid, axes)));
            const size_t mapping = CheckedMultiply(1024, CheckedAdd(CheckedAdd(execution.internalSteps_, definitions.size()), 1));
            return CheckedAdd(buffers, CheckedAdd(mapping, CheckedMultiply(256, OutputCount(definitions))));
        }

        size_t WorkerBytes(const Vector_<SampleDef_>& definitions, size_t dimension) {
            BufferCapacityBudget_ budget(std::numeric_limits<size_t>::max());
            BufferCapacityScope_ scope(&budget);
            Vector_<> normals(dimension);
            Scenario_<> path;
            AllocatePath(definitions, path);
            path.front().modelScratch_.Resize(1);
            path.front().modelFactorScratch_.Resize(1);
            return budget.CapacityBytes();
        }

        size_t CoordinatorBytes(size_t workers) {
            // Scalar MC bounds its chunk reduction ring at 256 slots, independently of path count.
            const size_t chunks = CheckedMultiply(256, sizeof(double) + sizeof(size_t) + sizeof(bool));
            const size_t dispatch = CheckedMultiply(workers, sizeof(TaskHandle_) + sizeof(std::unique_ptr<Model_<double>>));
            return CheckedAdd(chunks, dispatch);
        }

        double ResolvedMaxStep(const Vector_<>& anchors, const MarketIvLocalVolModelSettings_& settings) {
            double maximum = 0.;
            for (size_t interval = 1; interval < anchors.size(); ++interval) {
                const double from = anchors[interval - 1], to = anchors[interval];
                const size_t count = StepCount(from, to, settings);
                double previous = from;
                for (size_t step = 1; step <= count; ++step) {
                    const double time = step == count ? to : from + (to - from) * step / count;
                    if (time <= previous)
                        Fail("MarketIvLocalVolNumericRange", "Prepare", "/execution/timeline", "distinct representable substeps", ToString(time));
                    maximum = std::max(maximum, time - previous);
                    previous = time;
                }
            }
            return maximum;
        }
    } // namespace

    struct MarketIvLocalVolModel_::State_ {
        Vector_<> timeline_, anchors_;
        Vector_<SampleDef_> definitions_;
        std::unique_ptr<HybridModel_<double>> delegate_;
        MarketIvLocalVolExecution_ execution_;
        size_t preparationPeak_ = 0, preparationEnvelope_ = 0;
        bool initialized_ = false;
    };

    MarketIvLocalVolModel_::MarketIvLocalVolModel_(const MarketIvLocalVolModelData_& data)
        : source_(data.source_), assets_({data.Configuration().index_}) {}
    MarketIvLocalVolModel_::~MarketIvLocalVolModel_() = default;
    bool MarketIvLocalVolModel_::SupportsIndex(const Index_& index) const { return Supports(index, source_->Configuration()); }
    std::optional<Date_> MarketIvLocalVolModel_::EvaluationDate() const { return source_->Snapshot().Request().market_.EvaluationDate(); }

    void MarketIvLocalVolModel_::ValidateRequest(bool aad, bool exercise, const std::optional<Date_>& date, const String_& rsg, size_t workers) {
        if (aad)
            Fail("DerivativeUnavailable", "Prepare", "/simulation/enable_aad", "passive Price", "true");
        if (exercise)
            Fail("UnsupportedMarketIvLocalVolExercise", "Prepare", "/product/exercise/0", "product without EXERCISE", "EXERCISE");
        if (date != EvaluationDate())
            Fail("InvalidModelEvaluationDate", "Prepare", "/valuation/evaluation_date", Date::ToString(*EvaluationDate()),
                 date ? Date::ToString(*date) : "missing");
        rsg_ = rsg;
        workers_ = workers;
    }

    void MarketIvLocalVolModel_::Allocate(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) {
        this->ValidateTimeline(timeline, definitions);
        auto next = std::make_unique<State_>();
        next->execution_ = Plan(*source_, timeline, definitions, rsg_, &next->anchors_);
        CheckPathAddress(pathCount_, next->execution_.simulationDimension_, rsg_);
        next->execution_.preparedMarketQueries_ = PreparedQueries(*source_, next->execution_.internalSteps_, definitions);
        next->preparationEnvelope_ = PreparationEnvelope(*source_, next->execution_, definitions);
        const size_t workerEstimate =
            CheckedAdd(CheckedMultiply(definitions.size(), sizeof(Sample_<>)),
                       CheckedMultiply(sizeof(double), CheckedAdd(CheckedAdd(next->execution_.simulationDimension_, OutputCount(definitions)), 2)));
        const size_t oldBytes = state_ ? state_->execution_.sharedNumericBytes_ : 0;
        const size_t planned = std::max(source_->ConstructionExecution().plannedWorkspaceBytes_,
                                        CheckedAdd(CheckedAdd(next->preparationEnvelope_, oldBytes),
                                                   CheckedAdd(CheckedMultiply(workers_, workerEstimate), CoordinatorBytes(workers_))));
        CheckLimit(planned, source_->Configuration().workspaceBudgetBytes_, "/settings/workspace_budget_bytes", "Prepare");
        next->execution_.resolvedMaxStep_ = ResolvedMaxStep(next->anchors_, source_->Configuration());
        BufferCapacityBudget_ budget(next->preparationEnvelope_);
        BufferCapacityScope_ scope(&budget);
        next->timeline_ = timeline;
        next->definitions_ = definitions;
        next->delegate_ = Delegate(*source_, next->anchors_);
        next->delegate_->AllocateTo(next->timeline_, next->definitions_, next->execution_.requiredTime_);
        next->delegate_->Init(next->timeline_, next->definitions_);
        const size_t anchorsBytes = CheckedMultiply(sizeof(double), next->anchors_.capacity());
        next->execution_.sharedNumericBytes_ = CheckedAdd(budget.CapacityBytes(), anchorsBytes);
        next->preparationPeak_ = CheckedAdd(CheckedAdd(budget.PeakCapacityBytes(), anchorsBytes), oldBytes);
        REQUIRE(next->delegate_->SimDim() == next->execution_.simulationDimension_, "InvalidMarketIvLocalVolRequest: planned/delegate grid mismatch");
        scope.Close();
        next->execution_.perWorkerNumericBytes_ = WorkerBytes(definitions, next->execution_.simulationDimension_);
        CheckLimit(next->execution_.perWorkerNumericBytes_, workerEstimate, "/ledger/per_worker_numeric_bytes", "Prepare");
        next->execution_.workers_ = workers_;
        next->execution_.plannedWorkspaceBytes_ = planned;
        next->execution_.peakWorkspaceBytes_ =
            std::max({source_->ConstructionExecution().peakWorkspaceBytes_, next->preparationPeak_,
                      CheckedAdd(next->execution_.sharedNumericBytes_,
                                 CheckedAdd(CheckedMultiply(workers_, next->execution_.perWorkerNumericBytes_), CoordinatorBytes(workers_)))});
        CheckLimit(next->execution_.peakWorkspaceBytes_, planned, "/ledger/peak_workspace_bytes", "Prepare");
        state_.swap(next);
    }

    void MarketIvLocalVolModel_::Init(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) {
        REQUIRE(state_ && timeline == state_->timeline_ && definitions.size() == state_->definitions_.size(),
                "InvalidMarketIvLocalVolRequest: Allocate before Init");
        state_->initialized_ = true;
    }
    size_t MarketIvLocalVolModel_::SimDim() const { return state_ ? state_->execution_.simulationDimension_ : 0; }
    void MarketIvLocalVolModel_::GeneratePath(const Vector_<>& normals, Scenario_<>* path) const {
        REQUIRE(state_ && state_->initialized_, "InvalidMarketIvLocalVolRequest: Init before GeneratePath");
        state_->delegate_->GeneratePath(normals, path);
    }
    std::unique_ptr<Model_<double>> MarketIvLocalVolModel_::Clone() const {
        const MarketIvLocalVolModelData_ data("clone", source_);
        auto copy = std::make_unique<MarketIvLocalVolModel_>(data);
        copy->rsg_ = rsg_;
        copy->workers_ = workers_;
        copy->pathCount_ = pathCount_;
        if (state_) {
            copy->Allocate(state_->timeline_, state_->definitions_);
            if (state_->initialized_)
                copy->Init(state_->timeline_, state_->definitions_);
        }
        return copy;
    }
    void MarketIvLocalVolModel_::PrepareWorkers(size_t workers) {
        REQUIRE(state_ && workers > 0, "InvalidMarketIvLocalVolRequest: positive worker count after Allocate");
        auto execution = state_->execution_;
        const auto settings = source_->Configuration();
        execution.workers_ = workers;
        const size_t privateBytes = CheckedAdd(CheckedMultiply(workers, execution.perWorkerNumericBytes_), CoordinatorBytes(workers));
        execution.plannedWorkspaceBytes_ = std::max(execution.plannedWorkspaceBytes_, CheckedAdd(state_->preparationEnvelope_, privateBytes));
        execution.peakWorkspaceBytes_ = std::max({source_->ConstructionExecution().peakWorkspaceBytes_, state_->preparationPeak_,
                                                  CheckedAdd(execution.sharedNumericBytes_, privateBytes)});
        CheckLimit(execution.plannedWorkspaceBytes_, settings.workspaceBudgetBytes_, "/settings/workspace_budget_bytes", "Prepare");
        CheckLimit(execution.peakWorkspaceBytes_, execution.plannedWorkspaceBytes_, "/ledger/peak_workspace_bytes", "Prepare");
        CheckRng(execution.simulationDimension_, rsg_);
        state_->execution_ = execution;
    }
    MarketIvLocalVolExecution_ MarketIvLocalVolModel_::Execution() const {
        REQUIRE(state_, "InvalidMarketIvLocalVolRequest: Allocate before inspection");
        return state_->execution_;
    }
    void MarketIvLocalVolModel_::ValidatePaths(size_t paths, const String_& rsg) {
        CheckPathAddress(paths, SimDim(), rsg);
        pathCount_ = paths;
    }
} // namespace Dal::AAD
