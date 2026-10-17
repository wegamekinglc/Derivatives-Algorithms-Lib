#include <array>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/storage/globals.hpp>

#include "replaygraph.hpp"

namespace Dal::ReplayStudy {
    using Clock_ = std::chrono::steady_clock;
    using Inputs_ = std::array<double, 6>;
    constexpr Inputs_ BASE = {100.0, 0.2, 0.05, 0.02, 100.0, 150.0};
    constexpr const char* LABELS[] = {"spot", "vol", "rate", "div", "STRIKE", "BARRIER"};

    struct Case_ {
        size_t steps_ = 1;
        double width_ = 0.1;
        bool barrier_ = false;
        std::vector<double> times_;
    };

    void Check(bool value, const char* message) {
        if (!value)
            throw std::runtime_error(message);
    }

    template <class F_> void MustThrow(const F_& operation) {
        bool threw = false;
        try {
            operation();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Check(threw, "expected rejection");
    }

    double Elapsed(Clock_::time_point start) { return std::chrono::duration<double>(Clock_::now() - start).count(); }
    void ValidateShape(const Case_& c, const Vector_<>& normals) {
        Check(c.steps_ > 0 && c.times_.size() == c.steps_ && normals.size() == c.steps_ && c.width_ > 0.0 && std::isfinite(c.width_),
              "invalid graph shape or smoothing width");
    }
    void ValidateInputs(const Inputs_& inputs) {
        Check(std::isfinite(inputs[0]) && inputs[0] > 0.0 && std::isfinite(inputs[1]) && inputs[1] >= 0.0, "invalid BS parameters");
        for (double value : inputs)
            Check(std::isfinite(value), "nonfinite input");
    }
    void ValidateTimeline(const std::vector<double>& times) {
        double previous = 0.0;
        for (double time : times) {
            Check(std::isfinite(time) && time > previous, "invalid timeline");
            previous = time;
        }
    }
    void Validate(const Case_& c, const Inputs_& inputs, const Vector_<>& normals) {
        ValidateShape(c, normals);
        ValidateInputs(inputs);
        ValidateTimeline(c.times_);
        for (double normal : normals)
            Check(std::isfinite(normal), "nonfinite normal");
    }
    double Primal(const AAD::Number_& x) { return AAD::Value(x); }
    double Primal(const Scalar_& x) { return x.Value(); }

    struct NativeBuilder_ {
        using Number_ = AAD::Number_;
        const std::vector<double>* frame_;
        Number_ Input(size_t source) const { return Number_(frame_->at(source)); }
        Number_ Constant(double value) const { return Number_(value); }
        double Normal(size_t source) const { return frame_->at(source); }
        Number_ Maximum(const Number_& value, double floor) const { return floor > Primal(value) ? Number_(floor) : value; }
        int Region(const Number_& l, const Number_& r, double lower, double upper) const {
            return Graph_::Region(Primal(l) - Primal(r), lower, upper);
        }
    };

    struct ReplayBuilder_ {
        using Number_ = Scalar_;
        Graph_* graph_;
        const std::vector<double>* frame_;
        Number_ Input(size_t source) const { return Leaf(graph_, frame_, frame_->at(source), source); }
        Number_ Constant(double value) const { return Leaf(graph_, frame_, value); }
        Number_ Normal(size_t source) const { return Input(source); }
        Number_ Maximum(const Number_& value, double floor) const {
            return Apply(Op_::SCRIPT_MAX, value, {std::numeric_limits<size_t>::max(), floor});
        }
        int Region(const Number_& l, const Number_& r, double lower, double upper) const { return Guard(l, r, lower, upper); }
    };

    template <class T_> struct Prefix_ {
        std::vector<T_> inputs_, drifts_, stds_, numeraires_;
        T_ logSpot_, zero_, one_;
    };

    template <class B_> auto BuildPrefix(const Case_& c, const B_& b) {
        using T_ = typename B_::Number_;
        using Dal::exp;
        using Dal::log;
        std::vector<T_> inputs;
        for (size_t i = 0; i < BASE.size(); ++i)
            inputs.push_back(b.Input(i));
        Prefix_<T_> p{inputs, {}, {}, {}, log(inputs[0]), b.Constant(0.0), b.Constant(1.0)};
        const T_ mu = inputs[2] - inputs[3];
        double previous = 0.0;
        for (double time : c.times_) {
            const double dt = time - previous;
            p.stds_.push_back(inputs[1] * std::sqrt(dt));
            p.drifts_.push_back((mu - 0.5 * inputs[1] * inputs[1]) * dt);
            p.numeraires_.push_back(exp(inputs[2] * time));
            previous = time;
        }
        return p;
    }

    template <class B_> auto Path(const Case_& c, const B_& b, const Prefix_<typename B_::Number_>& p) {
        using T_ = typename B_::Number_;
        using Dal::exp;
        T_ logSpot = p.logSpot_, spot = p.inputs_[0], alive = p.one_;
        for (size_t i = 0; i < c.steps_; ++i) {
            logSpot = logSpot + (p.drifts_[i] + p.stds_[i] * b.Normal(BASE.size() + i));
            spot = exp(logSpot);
            if (c.barrier_) {
                const int region = b.Region(spot, p.inputs_[5], -0.5 * c.width_, 0.5 * c.width_);
                T_ degree = region < 0 ? p.zero_ : p.one_;
                if (region == 0) {
                    const T_ gap = spot - p.inputs_[5];
                    degree = (gap + 0.5 * c.width_) / c.width_;
                }
                const int branch = b.Region(degree, p.zero_, EPSILON, 1.0 - EPSILON);
                if (branch > 0)
                    alive = b.Constant(0.0);
                else if (branch == 0) {
                    const T_ killed = b.Constant(0.0);
                    alive = degree * killed + (1.0 - degree) * alive;
                }
            }
        }
        const T_ intrinsic = b.Maximum(T_(spot - p.inputs_[4]), 0.0);
        const T_ amount = c.barrier_ ? T_(alive * intrinsic) : intrinsic;
        // An explicit terminal add also roots prefix aliases and constant payoffs.
        const T_ payoff = p.zero_ + amount / p.numeraires_.back();
        return payoff;
    }

    struct Result_ {
        double pv_ = 0.0;
        Inputs_ risks_{};
        size_t hits_ = 0, recordings_ = 0, guardFailures_ = 0, nodes_ = 0, bytes_ = 0, capacity_ = 0;
        double forward_ = 0.0, reverse_ = 0.0, hitSeconds_ = 0.0, missSeconds_ = 0.0;
    };

    void Compare(const Result_& a, const Result_& b, const std::string& context = "") {
        const auto same = [&](double l, double r, const char* label) {
            if (!std::isfinite(l) || !std::isfinite(r) || std::abs(l - r) > 2e-10 * std::max({1.0, std::abs(l), std::abs(r)})) {
                std::ostringstream error;
                error << std::setprecision(17) << context << " " << label << " mismatch: " << l << " vs " << r;
                throw std::runtime_error(error.str());
            }
        };
        same(a.pv_, b.pv_, "PV");
        for (size_t i = 0; i < BASE.size(); ++i)
            same(a.risks_[i], b.risks_[i], LABELS[i]);
    }

    std::vector<double> Frame(const Inputs_& inputs, const Vector_<>& normals) {
        std::vector<double> result(inputs.begin(), inputs.end());
        result.insert(result.end(), normals.begin(), normals.end());
        return result;
    }

    Result_ NativePath(const Case_& c, const Inputs_& inputs, const Vector_<>& normals) {
        AAD::RecordingScope_ scope;
        auto frame = Frame(inputs, normals);
        const NativeBuilder_ b{&frame};
        const auto prefix = BuildPrefix(c, b);
        scope.StartRecording();
        const auto output = Path(c, b, prefix);
        scope.FinishRecording();
        AAD::Adjoint(output) = 1.0;
        scope.Reverse();
        Result_ result;
        result.pv_ = Primal(output);
        for (size_t i = 0; i < BASE.size(); ++i)
            result.risks_[i] = AAD::Adjoint(prefix.inputs_[i]);
        return result;
    }

    class Replay_ {
        Graph_ graph_;
        std::vector<double> frame_;
        std::optional<Prefix_<Scalar_>> prefix_;
        Case_ case_;
        Inputs_ inputs_{};
        size_t root_ = 0;

        [[nodiscard]] bool KeyChanged(const Case_& c, const Inputs_& inputs) const {
            return !prefix_ || c.times_ != case_.times_ || c.width_ != case_.width_ || c.barrier_ != case_.barrier_ || inputs != inputs_;
        }

    public:
        Result_ counts_;
        bool Evaluate(const Case_& c, const Inputs_& inputs, const Vector_<>& normals, bool forceRecord = false) {
            try {
                Validate(c, inputs, normals);
            } catch (...) {
                prefix_.reset();
                graph_.Reset();
                throw;
            }
            frame_ = Frame(inputs, normals);
            const ReplayBuilder_ b{&graph_, &frame_};
            if (KeyChanged(c, inputs)) {
                prefix_.reset();
                graph_.Reset();
                prefix_ = BuildPrefix(c, b);
                graph_.Mark();
                case_ = c;
                inputs_ = inputs;
            } else if (!forceRecord && graph_.Replay(frame_)) {
                ++counts_.hits_;
                return true;
            } else if (!forceRecord) {
                ++counts_.guardFailures_;
                Check(!graph_.Ready(), "guard failure published a graph");
            }
            graph_.Restore();
            try {
                root_ = Path(c, b, *prefix_).slot_;
                graph_.Finish();
            } catch (...) {
                prefix_.reset();
                graph_.Reset();
                throw;
            }
            ++counts_.recordings_;
            counts_.nodes_ = std::max(counts_.nodes_, graph_.Size());
            counts_.bytes_ = std::max(counts_.bytes_, graph_.OccupiedBytes());
            counts_.capacity_ = std::max(counts_.capacity_, graph_.CapacityBytes());
            return false;
        }
        Result_ Gradient(bool suffixOnly = false) {
            graph_.Reverse(root_, suffixOnly);
            Result_ result;
            result.pv_ = graph_.Value(root_);
            if (!suffixOnly)
                for (size_t i = 0; i < BASE.size(); ++i)
                    result.risks_[i] = graph_.Adjoint(prefix_->inputs_[i].slot_);
            return result;
        }
        Inputs_ FinishRisks() {
            graph_.ReversePrefix();
            Inputs_ risks{};
            for (size_t i = 0; i < BASE.size(); ++i)
                risks[i] = graph_.Adjoint(prefix_->inputs_[i].slot_);
            return risks;
        }
    };

    Script::ScriptProduct_ Product(const Case_& c, const Inputs_& inputs) {
        Vector_<Cell_> dates{Cell_("STRIKE"), Cell_("BARRIER")};
        const auto number = [](double x) {
            std::ostringstream out;
            out << std::setprecision(17) << x;
            return String_(out.str().c_str());
        };
        Vector_<String_> events{number(inputs[4]), number(inputs[5])};
        dates.push_back(Cell_(Date_(2024, 1, 1)));
        events.push_back("alive = 1");
        for (size_t i = 0; i < c.steps_; ++i) {
            dates.push_back(Cell_(Date_(2024, 1, 1).AddDays(static_cast<int>(7 * (i + 1)))));
            String_ event = c.barrier_ ? "if spot() >= BARRIER:" + number(c.width_) + " then alive = 0 end " : "";
            if (i + 1 == c.steps_)
                event += c.barrier_ ? "pay PAYS alive * MAX(spot() - STRIKE, 0)" : "pay PAYS MAX(spot() - STRIKE, 0)";
            events.push_back(event);
        }
        return {dates, events, "pay"};
    }

    template <class F_> void WithCaller(const Case_& c, const Inputs_& inputs, const F_& work) {
        AAD::RecordingScope_ scope;
        auto product = Product(c, inputs);
        const auto nesting = product.PreProcess(true, true);
        auto compiled = product.Compile(true);
        auto evaluator = product.BuildEvalState<AAD::Number_>(nesting, c.width_);
        AAD::BlackScholes_<AAD::Number_> model(inputs[0], inputs[1], inputs[2], inputs[3]);
        model.Allocate(product.TimeLine(), product.DefLine());
        AAD::Scenario_<AAD::Number_> path;
        AAD::AllocatePath(product.DefLine(), path);
        AAD::InitializePath(path);
        for (size_t i = 0; i < 4; ++i)
            scope.RegisterInput(*model.Parameters()[i], inputs[i]);
        for (size_t i = 0; i < product.ConstVarNames().size(); ++i)
            scope.RegisterInput(evaluator.ConstVarVals()[i], product.ConstVarNames()[i] == "STRIKE" ? inputs[4] : inputs[5]);
        AAD::Number_ zero;
        scope.RegisterInput(zero, 0.0);
        scope.StartRecording();
        model.Init(product.TimeLine(), product.DefLine());
        evaluator.InitRecordingConstants();
        const auto checkpoint = scope.MakeCheckpoint();
        scope.FinishRecording();
        work(
            [&](const Vector_<>& normals) {
                scope.Restore(checkpoint);
                model.GeneratePath(normals, &path);
                compiled.Evaluate(path, evaluator);
                const auto output = AAD::PayoffRoot(evaluator.VarVals()[product.PayOffIdx()], zero);
                scope.FinishRecording();
                AAD::Adjoint(output) = 1.0;
                scope.ReverseSuffix(checkpoint);
                return AAD::Value(output);
            },
            [&] {
                scope.ReversePrefix(checkpoint);
                Inputs_ result{};
                for (size_t i = 0; i < 4; ++i)
                    result[i] = AAD::Adjoint(*model.Parameters()[i]);
                for (size_t i = 0; i < product.ConstVarNames().size(); ++i)
                    result[product.ConstVarNames()[i] == "STRIKE" ? 4 : 5] = AAD::Adjoint(evaluator.ConstVarVals()[i]);
                return result;
            });
    }

    Result_ CallerPath(const Case_& c, const Inputs_& inputs, const Vector_<>& normals) {
        Result_ result;
        WithCaller(c, inputs, [&](const auto& evaluate, const auto& finish) {
            result.pv_ = evaluate(normals);
            result.risks_ = finish();
        });
        return result;
    }

    void VerifySelectors() {
        Graph_ graph;
        std::vector<double> frame{2.0, 3.0};
        auto x = Leaf(&graph, &frame, 2.0, 0), y = Leaf(&graph, &frame, 3.0, 1);
        const auto a = Leaf(&graph, &frame, 0.0), b = Leaf(&graph, &frame, 0.0);
        Check(a.slot_ != b.slot_, "equal active constants merged");
        const auto alias = x;
        const auto maximum = max(x, y), minimum = min(x, y);
        auto root = maximum + minimum;
        graph.Finish();
        graph.Reverse(root.slot_);
        Check(graph.Adjoint(alias.slot_) == 1.0, "copy alias lost");
        frame = {4.0, 1.0};
        Check(graph.Replay(frame), "MAX/MIN changed structure");
        graph.Reverse(maximum.slot_);
        Check(graph.Adjoint(x.slot_) == 1.0 && graph.Adjoint(y.slot_) == 0.0, "MAX selector derivative stale");
        graph.Reverse(minimum.slot_);
        Check(graph.Adjoint(x.slot_) == 0.0 && graph.Adjoint(y.slot_) == 1.0, "MIN selector derivative stale");
        graph.Reverse(root.slot_);
        Check(graph.Value(root.slot_) == 5.0 && graph.Adjoint(x.slot_) == 1.0 && graph.Adjoint(y.slot_) == 1.0, "selector derivative stale");
        frame = {2.0, 2.0};
        Check(graph.Replay(frame), "tie replay rejected");
        graph.Reverse(root.slot_);
        Check(graph.Adjoint(x.slot_) == 0.0 && graph.Adjoint(y.slot_) == 0.0, "tie derivative changed");
        const auto scriptMax = Apply(Op_::SCRIPT_MAX, x, y.Operand());
        graph.Finish();
        graph.Reverse(scriptMax.slot_);
        Check(graph.Adjoint(x.slot_) == 1.0 && graph.Adjoint(y.slot_) == 0.0, "script MAX tie alias changed");
        const auto scriptMin = Apply(Op_::SCRIPT_MIN, x, y.Operand());
        graph.Finish();
        graph.Reverse(scriptMin.slot_);
        Check(graph.Adjoint(x.slot_) == 1.0 && graph.Adjoint(y.slot_) == 0.0, "script MIN tie alias changed");
    }

    void VerifyGraph() {
        VerifySelectors();
        Graph_ graph;
        std::vector<double> frame{2.0, 2.0};
        auto x = Leaf(&graph, &frame, 2.0, 0), y = Leaf(&graph, &frame, 2.0, 1);
        graph.Mark();
        const auto oldSuffix = x + y;
        graph.Restore();
        const auto newSuffix = x * y;
        MustThrow([&] { (void)oldSuffix.Operand(); });
        Check(newSuffix.Value() == 4.0, "suffix recovery failed");
        graph.Reset();
        MustThrow([&] { (void)x.Operand(); });
        x = Leaf(&graph, &frame, 2.0, 0);
        y = Leaf(&graph, &frame, 2.0, 1);
        Guard(x, y, -1.0, 1.0);
        graph.Finish();
        frame[0] = 4.0;
        Check(!graph.Replay(frame) && !graph.Ready(), "guard accepted invalid structure");
        MustThrow([&] { graph.Reverse(x.slot_); });
        frame[0] = std::numeric_limits<double>::quiet_NaN();
        Check(!graph.Replay(frame), "nonfinite replay accepted");
        graph.Reset();
        frame = {3.0, 4.0};
        x = Leaf(&graph, &frame, 3.0, 0);
        const auto zero = Leaf(&graph, &frame, 0.0);
        graph.Mark();
        const auto root = x + zero;
        graph.Finish();
        graph.Reverse(root.slot_, true);
        graph.ReversePrefix();
        Check(graph.Adjoint(x.slot_) == 1.0, "prefix alias root failed");
        for (double boundary : {-1.0, 1.0, EPSILON, 1.0 - EPSILON}) {
            graph.Reset();
            frame = {boundary, 0.0};
            x = Leaf(&graph, &frame, boundary, 0);
            y = Leaf(&graph, &frame, 0.0, 1);
            Guard(x, y, boundary, boundary);
            graph.Finish();
            Check(graph.Replay(frame), "exact guard boundary rejected");
            for (double direction : {-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()}) {
                frame[0] = std::nextafter(boundary, direction);
                Check(!graph.Replay(frame), "adjacent guard region not invalidated");
            }
        }
        {
            AAD::RecordingScope_ scope;
            AAD::Number_ input, zero;
            scope.RegisterInput(input, 3.0);
            scope.RegisterInput(zero, 0.0);
            scope.StartRecording();
            const auto checkpoint = scope.MakeCheckpoint();
            const auto output = AAD::PayoffRoot(input, zero);
            Check(&AAD::Adjoint(output) != &AAD::Adjoint(input), "native prefix alias not rooted");
            scope.FinishRecording();
            AAD::Adjoint(output) = 1.0;
            scope.ReverseSuffix(checkpoint);
            scope.ReversePrefix(checkpoint);
            Check(AAD::Adjoint(input) == 1.0, "native alias root gradient lost");
        }
    }

    size_t VerifyRandom(const Case_& vanilla, const Case_& barrier) {
        size_t comparisons = 0;
        Replay_ replay;
        for (const auto& c : {vanilla, barrier}) {
            auto rng = NewSobol(static_cast<int>(c.steps_), 1);
            Vector_<> normals(c.steps_);
            for (size_t i = 0; i < 96; ++i) {
                rng->FillNormal(&normals);
                Inputs_ inputs = BASE;
                if (i % 3 == 0)
                    inputs[5] = 110.0;
                if (i % 11 == 0)
                    inputs[i % BASE.size()] *= 1.001;
                replay.Evaluate(c, inputs, normals);
                const auto native = NativePath(c, inputs, normals);
                Compare(replay.Gradient(), native, "random replay " + std::to_string(c.steps_) + "/" + std::to_string(i));
                Compare(native, CallerPath(c, inputs, normals), "random caller " + std::to_string(c.steps_) + "/" + std::to_string(i));
                ++comparisons;
            }
        }
        return comparisons;
    }

    void Verify(const Case_& vanilla, const Case_& barrier) {
        VerifyGraph();
        size_t comparisons = VerifyRandom(vanilla, barrier);
        Replay_ replay;
        Case_ single = vanilla;
        single.barrier_ = true;
        single.width_ = 2.0;
        Vector_<> normals(1, 0.0);
        Inputs_ inputs = BASE;
        inputs[1] = inputs[2] = inputs[3] = 0.0;
        const double spot = std::exp(std::log(inputs[0]));
        // Perturb the barriers themselves so adjacent offsets survive subtraction at spot scale.
        const double barriers[] = {spot + 2.0,
                                   spot + 1.0,
                                   std::nextafter(spot + 1.0, 200.0),
                                   std::nextafter(spot + 1.0, 0.0),
                                   spot,
                                   std::nextafter(spot - 1.0, 200.0),
                                   spot - 1.0,
                                   std::nextafter(spot - 1.0, 0.0),
                                   spot - 2.0};
        for (double barrierValue : barriers) {
            inputs[5] = barrierValue;
            inputs[4] = spot - 5.0;
            replay.Evaluate(single, inputs, normals);
            const auto native = NativePath(single, inputs, normals);
            Compare(replay.Gradient(), native, "boundary replay " + std::to_string(barrierValue));
            Compare(native, CallerPath(single, inputs, normals), "boundary caller " + std::to_string(barrierValue));
            ++comparisons;
        }
        inputs[5] = 150.0;
        inputs[4] = spot;
        for (double strike : {std::nextafter(spot, 0.0), spot, std::nextafter(spot, 200.0)}) {
            inputs[4] = strike;
            replay.Evaluate(single, inputs, normals);
            Compare(replay.Gradient(), NativePath(single, inputs, normals), "strike replay");
            Compare(replay.Gradient(), CallerPath(single, inputs, normals), "strike caller");
            ++comparisons;
        }
        inputs = BASE;
        inputs[2] = inputs[3] = 0.0;
        const double atBoundary = std::exp(std::log(inputs[0]) - 0.5 * inputs[1] * inputs[1] * single.times_[0]);
        for (double endpoint : {-1.0, 1.0}) {
            Replay_ boundaryReplay;
            inputs[5] = atBoundary - endpoint;
            inputs[4] = atBoundary - 5.0;
            Check(atBoundary - inputs[5] == endpoint, "boundary construction rounded away");
            for (double normal : {-0.1, 0.0, 1e-8, -1e-8, 0.0}) {
                normals[0] = normal;
                boundaryReplay.Evaluate(single, inputs, normals);
                const auto native = NativePath(single, inputs, normals);
                Compare(boundaryReplay.Gradient(), native, "fixed-input endpoint replay");
                Compare(native, CallerPath(single, inputs, normals), "fixed-input endpoint caller");
                ++comparisons;
            }
            Check(boundaryReplay.counts_.guardFailures_ >= 2, "smoothing boundary did not invalidate graph");
        }
        Replay_ crossing;
        inputs = BASE;
        Vector_<> low(1, -2.0), high(1, 20.0);
        crossing.Evaluate(single, inputs, low);
        crossing.Gradient();
        crossing.Evaluate(single, inputs, high);
        Compare(crossing.Gradient(), NativePath(single, inputs, high));
        crossing.Evaluate(single, inputs, low);
        Compare(crossing.Gradient(), NativePath(single, inputs, low));
        Check(crossing.counts_.guardFailures_ == 2, "branch crossing did not invalidate graph");
        Check(crossing.Evaluate(single, inputs, low), "stable path did not hit cache");
        Case_ changed = single;
        changed.width_ = 1.0;
        Check(!crossing.Evaluate(changed, inputs, low), "smoothing key change reused graph");
        Compare(crossing.Gradient(), NativePath(changed, inputs, low));
        inputs[0] = -1.0;
        MustThrow([&] { crossing.Evaluate(single, inputs, low); });
        inputs = BASE;
        Vector_<> invalidNormal(1, 1e308);
        MustThrow([&] { crossing.Evaluate(single, inputs, invalidNormal); });
        MustThrow([&] { crossing.Gradient(); });
        crossing.Evaluate(single, inputs, low);
        Compare(crossing.Gradient(), NativePath(single, inputs, low));
        std::cout << "{\"verification\":\"passed\",\"path_comparisons\":" << comparisons
                  << ",\"named_risks_per_path\":6,\"forced_guard_failures\":2}\n";
    }

    void Normalize(size_t paths, Result_* result) {
        result->pv_ /= paths;
        for (auto& risk : result->risks_)
            risk /= paths;
    }

    void CaptureNativeMemory(Result_* result) {
        const auto stats = AAD::MeasureTape(*AAD::Tape());
        result->nodes_ = std::max(result->nodes_, stats.nodes_);
        result->bytes_ = std::max(result->bytes_, stats.occupiedBytes_);
        result->capacity_ = std::max(result->capacity_, stats.capacityBytes_);
    }

    Clock_::time_point PhaseStart(bool phases) { return phases ? Clock_::now() : Clock_::time_point{}; }

    void AddElapsed(bool phases, Clock_::time_point start, double* seconds) {
        if (phases)
            *seconds += Elapsed(start);
    }

    Result_ MeasureCaller(const Case_& c, const Inputs_& inputs, size_t paths, bool phases, Random_* rng, Vector_<>* normals) {
        Result_ result;
        WithCaller(c, inputs, [&](const auto& evaluate, const auto& finish) {
            for (size_t i = 0; i < paths; ++i) {
                rng->FillNormal(normals);
                result.pv_ += evaluate(*normals);
                if (phases)
                    CaptureNativeMemory(&result);
            }
            result.risks_ = finish();
        });
        return result;
    }

    Result_ MeasureNative(const Case_& c, const Inputs_& inputs, size_t paths, bool phases, Random_* rng, Vector_<>* normals) {
        Result_ result;
        AAD::RecordingScope_ scope;
        auto frame = Frame(inputs, *normals);
        const NativeBuilder_ builder{&frame};
        const auto prefix = BuildPrefix(c, builder);
        scope.StartRecording();
        const auto checkpoint = scope.MakeCheckpoint();
        scope.FinishRecording();
        for (size_t i = 0; i < paths; ++i) {
            rng->FillNormal(normals);
            frame = Frame(inputs, *normals);
            const auto start = PhaseStart(phases);
            scope.Restore(checkpoint);
            const auto output = Path(c, builder, prefix);
            scope.FinishRecording();
            AddElapsed(phases, start, &result.forward_);
            const auto reverse = PhaseStart(phases);
            AAD::Adjoint(output) = 1.0;
            scope.ReverseSuffix(checkpoint);
            result.pv_ += Primal(output);
            AddElapsed(phases, reverse, &result.reverse_);
            if (phases)
                CaptureNativeMemory(&result);
        }
        scope.ReversePrefix(checkpoint);
        for (size_t i = 0; i < BASE.size(); ++i)
            result.risks_[i] = AAD::Adjoint(prefix.inputs_[i]);
        return result;
    }

    Result_
    MeasureReplay(const std::string& mode, const Case_& c, const Inputs_& inputs, size_t paths, bool phases, Random_* rng, Vector_<>* normals) {
        Result_ result;
        Replay_ replay;
        for (size_t i = 0; i < paths; ++i) {
            rng->FillNormal(normals);
            const auto start = PhaseStart(phases);
            const bool hit = replay.Evaluate(c, inputs, *normals, mode == "record");
            if (phases) {
                const double forward = Elapsed(start);
                result.forward_ += forward;
                (hit ? result.hitSeconds_ : result.missSeconds_) += forward;
            }
            const auto reverse = PhaseStart(phases);
            result.pv_ += replay.Gradient(true).pv_;
            AddElapsed(phases, reverse, &result.reverse_);
        }
        result.risks_ = replay.FinishRisks();
        const auto counts = replay.counts_;
        result.hits_ = counts.hits_;
        result.recordings_ = counts.recordings_;
        result.guardFailures_ = counts.guardFailures_;
        result.nodes_ = counts.nodes_;
        result.bytes_ = counts.bytes_;
        result.capacity_ = counts.capacity_;
        return result;
    }

    Result_ Measure(const std::string& mode, const Case_& c, const Inputs_& inputs, size_t paths, bool phases) {
        auto rng = NewSobol(static_cast<int>(c.steps_), 1);
        Vector_<> normals(c.steps_);
        Result_ result;
        if (mode == "caller")
            result = MeasureCaller(c, inputs, paths, phases, rng.get(), &normals);
        else if (mode == "native")
            result = MeasureNative(c, inputs, paths, phases, rng.get(), &normals);
        else
            result = MeasureReplay(mode, c, inputs, paths, phases, rng.get(), &normals);
        Normalize(paths, &result);
        return result;
    }
    struct Options_ {
        std::string mode_ = "verify", scenario_ = "barrier";
        size_t paths_ = 4096;
        bool phases_ = false;
    };

    size_t ReadPaths(const std::string& value) {
        size_t paths = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), paths);
        Check(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && paths > 0 && paths <= 1000000, "invalid paths");
        return paths;
    }

    void SetOption(const std::string& name, const std::string& value, Options_* options) {
        if (name == "--mode")
            options->mode_ = value;
        else if (name == "--case")
            options->scenario_ = value;
        else if (name == "--phases") {
            Check(value == "true" || value == "false", "invalid phases flag");
            options->phases_ = value == "true";
        } else if (name == "--paths")
            options->paths_ = ReadPaths(value);
        else
            throw std::runtime_error("unknown option");
    }

    Options_ ReadOptions(int argc, char** argv) {
        Check(argc % 2 == 1, "expected option/value pairs");
        Options_ options;
        for (int i = 1; i < argc; i += 2) {
            SetOption(argv[i], argv[i + 1], &options);
        }
        const std::array<std::string, 5> modes{"verify", "native", "replay", "record", "caller"};
        const std::array<std::string, 3> scenarios{"vanilla", "barrier", "crossing"};
        Check(std::find(modes.begin(), modes.end(), options.mode_) != modes.end(), "invalid mode");
        Check(std::find(scenarios.begin(), scenarios.end(), options.scenario_) != scenarios.end(), "invalid case");
        return options;
    }

    Case_ BarrierCase() {
        Case_ barrier{52, 0.1, true, {}};
        for (size_t i = 1; i <= barrier.steps_; ++i)
            barrier.times_.push_back(7.0 * i / 365.0);
        return barrier;
    }

    void VerifyAggregates(const Case_& vanilla, const Case_& barrier) {
        for (const auto& c : {vanilla, barrier}) {
            const auto reference = Measure("native", c, BASE, 257, false);
            for (const auto& method : {"replay", "record", "caller"})
                Compare(reference, Measure(method, c, BASE, 257, false));
        }
    }

    void PrintResult(const Options_& options, const Result_& result, double total) {
        std::cout << std::setprecision(17) << "{\"mode\":\"" << options.mode_ << "\",\"case\":\"" << options.scenario_
                  << "\",\"paths\":" << options.paths_ << ",\"seconds\":" << total << ",\"pv\":" << result.pv_ << ",\"risks\":{";
        for (size_t i = 0; i < BASE.size(); ++i)
            std::cout << (i ? "," : "") << '"' << LABELS[i] << "\":" << result.risks_[i];
        std::cout << "},\"hits\":" << result.hits_ << ",\"recordings\":" << result.recordings_ << ",\"guard_failures\":" << result.guardFailures_
                  << ",\"peak_nodes\":" << result.nodes_ << ",\"graph_bytes\":" << result.bytes_ << ",\"graph_capacity\":" << result.capacity_
                  << ",\"forward_seconds\":" << result.forward_ << ",\"reverse_seconds\":" << result.reverse_
                  << ",\"hit_seconds\":" << result.hitSeconds_ << ",\"miss_seconds\":" << result.missSeconds_ << "}\n";
    }

    int RunStudy(const Options_& options) {
        const auto storage = XGLOBAL::SetEvaluationDateInScope(Date_(2024, 1, 1));
        const Case_ vanilla{1, 0.1, false, {7.0 / 365.0}};
        const auto barrier = BarrierCase();
        if (options.mode_ == "verify") {
            Verify(vanilla, barrier);
            VerifyAggregates(vanilla, barrier);
            return 0;
        }
        Inputs_ inputs = BASE;
        const auto& c = options.scenario_ == "vanilla" ? vanilla : barrier;
        if (options.scenario_ == "crossing")
            inputs[5] = 110.0;
        const auto start = Clock_::now();
        const auto result = Measure(options.mode_, c, inputs, options.paths_, options.phases_);
        PrintResult(options, result, Elapsed(start));
        (void)storage;
        return 0;
    }
} // namespace Dal::ReplayStudy

int main(int argc, char** argv) {
    try {
        return Dal::ReplayStudy::RunStudy(Dal::ReplayStudy::ReadOptions(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
