#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <dal/script/event.hpp>

namespace Dal::Script::SoaStudy {
    template <size_t L_> using Row_ = std::array<double, L_>;

    struct Counters_ {
        size_t mixed_ = 0;
        size_t fallbackLanes_ = 0;
    };

    struct Instruction_ {
        int op_;
        size_t dst_ = 0, left_ = 0, right_ = 0;
        double constant_ = 0.0;
        size_t trueEnd_ = 0, end_ = 0;
        size_t sourceTrue_ = 0, sourceFalse_ = 0, sourceEnd_ = 0;
    };

    class Plan_ {
        ScriptCompiled_ scalar_;
        Vector_<> seed_, constants_;
        std::vector<std::vector<Instruction_>> events_;
        size_t maxDouble_ = 0, maxBool_ = 0;

        static bool IsOneOf(int op, std::initializer_list<int> choices) { return std::find(choices.begin(), choices.end(), op) != choices.end(); }

        size_t Operand(size_t event, size_t last, size_t* pc) const {
            const auto& stream = scalar_.NodeStreams()[event];
            REQUIRE(++*pc < last && stream[*pc] >= 0, "SoA: missing or negative operand");
            return static_cast<size_t>(stream[*pc]);
        }

        double Constant(size_t event, size_t last, size_t* pc) const {
            const size_t index = Operand(event, last, pc);
            const auto& values = scalar_.ConstStreams()[event];
            REQUIRE(index < values.size(), "SoA: constant index out of range");
            return values[index];
        }

        static size_t Top(size_t depth) {
            REQUIRE(depth > 0, "SoA: stack underflow");
            return depth - 1;
        }

        static size_t Pop(size_t* depth) {
            const size_t slot = Top(*depth);
            --*depth;
            return slot;
        }

        static void RequireEmpty(size_t doubles, size_t booleans) { REQUIRE(doubles == 0 && booleans == 0, "SoA: unbalanced statement or branch"); }

        void DecodeLoad(size_t event, size_t last, size_t* pc, size_t* doubles, Instruction_* item) const {
            item->dst_ = (*doubles)++;
            if (item->op_ == Const)
                item->constant_ = Constant(event, last, pc);
            if (IsOneOf(item->op_, {Var, ConstVar})) {
                item->left_ = Operand(event, last, pc);
                REQUIRE(item->left_ < (item->op_ == Var ? seed_.size() : constants_.size()), "SoA: variable index out of range");
                if (item->op_ == ConstVar)
                    item->constant_ = constants_[item->left_];
            }
        }

        void DecodeStore(size_t event, size_t last, size_t* pc, size_t* doubles, Instruction_* item) const {
            if (IsOneOf(item->op_, {AssignConst, PaysConst}))
                item->constant_ = Constant(event, last, pc);
            else
                item->left_ = Pop(doubles);
            item->dst_ = Operand(event, last, pc);
            REQUIRE(item->dst_ < seed_.size(), "SoA: store index out of range");
        }

        void DecodeStack(size_t event, size_t last, size_t* pc, size_t* doubles, size_t* booleans, Instruction_* item) const {
            const int op = item->op_;
            if (IsOneOf(op, {Add, Sub, Multi, Div, Max2, Min2})) {
                item->right_ = Pop(doubles);
                item->dst_ = Top(*doubles);
            } else if (IsOneOf(op, {AddConst, SubConst, ConstSub, MultiConst, DivConst, ConstDiv, Max2Const, Min2Const, ConstMax2, ConstMin2})) {
                item->dst_ = Top(*doubles);
                item->constant_ = Constant(event, last, pc);
            } else if (IsOneOf(op, {Sup, SupEqual, Equal})) {
                item->left_ = Pop(doubles);
                item->dst_ = (*booleans)++;
            } else if (IsOneOf(op, {True, False})) {
                item->dst_ = (*booleans)++;
            } else if (op == Not) {
                item->dst_ = Top(*booleans);
            } else if (IsOneOf(op, {And, Or})) {
                item->right_ = Pop(booleans);
                item->dst_ = Top(*booleans);
            } else if (IsOneOf(op, {UMinus, Sqrt, Log, Exp})) {
                item->dst_ = Top(*doubles);
            } else {
                THROW("SoA: unsupported opcode " + std::to_string(op));
            }
        }

        void DecodeInstruction(size_t event, size_t last, size_t* pc, size_t* doubles, size_t* booleans, Instruction_* item) const {
            if (IsOneOf(item->op_, {Spot, Const, Var, ConstVar}))
                DecodeLoad(event, last, pc, doubles, item);
            else if (IsOneOf(item->op_, {Assign, Pays, AssignConst, PaysConst}))
                DecodeStore(event, last, pc, doubles, item);
            else
                DecodeStack(event, last, pc, doubles, booleans, item);
        }

        void DecodeBranch(size_t event, size_t last, size_t* pc, size_t* doubles, size_t* booleans, Instruction_ item) {
            item.left_ = Pop(booleans);
            RequireEmpty(*doubles, *booleans);
            item.sourceFalse_ = Operand(event, last, pc);
            item.sourceEnd_ = item.op_ == IfElse ? Operand(event, last, pc) : item.sourceFalse_;
            item.sourceTrue_ = *pc + 1;
            REQUIRE(item.sourceTrue_ <= item.sourceFalse_ && item.sourceFalse_ <= item.sourceEnd_ && item.sourceEnd_ <= last,
                    "SoA: invalid IF range");
            auto& out = events_[event];
            const size_t slot = out.size();
            out.push_back(item);
            Decode(event, item.sourceTrue_, item.sourceFalse_, doubles, booleans);
            RequireEmpty(*doubles, *booleans);
            out[slot].trueEnd_ = out.size();
            Decode(event, item.sourceFalse_, item.sourceEnd_, doubles, booleans);
            RequireEmpty(*doubles, *booleans);
            out[slot].end_ = out.size();
            *pc = item.sourceEnd_ - 1;
        }

        void Decode(size_t event, size_t first, size_t last, size_t* doubles, size_t* booleans) {
            const auto& stream = scalar_.NodeStreams()[event];
            for (size_t pc = first; pc < last; ++pc) {
                Instruction_ item{stream[pc]};
                if (IsOneOf(item.op_, {If, IfElse})) {
                    DecodeBranch(event, last, &pc, doubles, booleans, item);
                } else {
                    DecodeInstruction(event, last, &pc, doubles, booleans, &item);
                    maxDouble_ = std::max(maxDouble_, *doubles);
                    maxBool_ = std::max(maxBool_, *booleans);
                    events_[event].push_back(item);
                }
            }
        }

    public:
        Plan_(ScriptCompiled_ scalar, Vector_<> seed, Vector_<> constants = {})
            : scalar_(std::move(scalar)), seed_(std::move(seed)), constants_(std::move(constants)), events_(scalar_.NodeStreams().size()) {
            REQUIRE(events_.size() == scalar_.ConstStreams().size(), "SoA: event count mismatch");
            for (size_t e = 0; e < events_.size(); ++e) {
                size_t doubles = 0, booleans = 0;
                Decode(e, 0, scalar_.NodeStreams()[e].size(), &doubles, &booleans);
                REQUIRE(doubles == 0 && booleans == 0, "SoA: unbalanced event");
            }
        }

        template <size_t L_> struct State_ {
            const Plan_* owner_;
            std::vector<Row_<L_>> variables_, stack_;
            std::vector<std::array<unsigned char, L_>> booleans_;
            EvalState_<double> fallback_;
            explicit State_(const Plan_& plan)
                : owner_(&plan), variables_(plan.seed_.size()), stack_(plan.maxDouble_), booleans_(plan.maxBool_),
                  fallback_(plan.seed_, plan.constants_) {}
        };

    private:
        template <size_t L_, class F_> static void Each(uint64_t mask, size_t count, const F_& f) {
            const size_t limit = std::min(count, L_);
            const uint64_t full = count == 64 ? ~uint64_t(0) : (uint64_t(1) << count) - 1;
            if (mask == full) {
                for (size_t lane = 0; lane < limit; ++lane)
                    f(lane);
            } else {
                for (size_t lane = 0; lane < limit; ++lane)
                    if (mask & (uint64_t(1) << lane))
                        f(lane);
            }
        }

        struct BranchMask_ {
            uint64_t yes_ = 0, no_ = 0;
            size_t active_ = 0, selected_ = 0;
            [[nodiscard]] bool Mixed() const { return yes_ && no_; }
            [[nodiscard]] bool UseFallback(bool enabled) const {
                return enabled && Mixed() && 4 * std::min(selected_, active_ - selected_) >= active_;
            }
        };

        template <size_t L_> static BranchMask_ Partition(uint64_t mask, size_t count, size_t condition, const State_<L_>& state) {
            BranchMask_ result;
            Each<L_>(mask, count, [&](size_t lane) {
                ++result.active_;
                if (state.booleans_[condition][lane]) {
                    result.yes_ |= uint64_t(1) << lane;
                    ++result.selected_;
                }
            });
            result.no_ = mask ^ result.yes_;
            return result;
        }

        template <size_t L_>
        void FallbackLane(size_t event, const Instruction_& i, size_t lane, bool takeTrue, double spot, double numeraire, State_<L_>* state) const {
            auto& scalar = state->fallback_;
            auto& variables = state->variables_;
            for (size_t variable = 0; variable < variables.size(); ++variable)
                scalar.variables_[variable] = variables[variable][lane];
            AAD::Sample_<double> sample{};
            sample.spot_ = spot;
            sample.numeraire_ = numeraire;
            const size_t begin = takeTrue ? i.sourceTrue_ : i.sourceFalse_;
            const size_t end = takeTrue ? i.sourceFalse_ : i.sourceEnd_;
            if (begin < end)
                Detail::EvalCompiledRange<false>(scalar_.NodeStreams()[event], scalar_.ConstStreams()[event], sample, scalar, begin, end, true);
            for (size_t variable = 0; variable < variables.size(); ++variable)
                variables[variable][lane] = scalar.variables_[variable];
        }

        template <size_t L_>
        void Branch(size_t event,
                    size_t pc,
                    uint64_t mask,
                    size_t count,
                    const Row_<L_>& spots,
                    double numeraire,
                    bool fallback,
                    State_<L_>* state,
                    Counters_* counters) const {
            const auto& i = events_[event][pc];
            const auto partition = Partition(mask, count, i.left_, *state);
            if (partition.Mixed())
                ++counters->mixed_;
            if (partition.UseFallback(fallback)) {
                Each<L_>(mask, count, [&](size_t lane) {
                    const bool takeTrue = (partition.yes_ & (uint64_t(1) << lane)) != 0;
                    FallbackLane(event, i, lane, takeTrue, spots[lane], numeraire, state);
                    ++counters->fallbackLanes_;
                });
            } else {
                if (partition.yes_)
                    Range(event, pc + 1, i.trueEnd_, partition.yes_, count, spots, numeraire, fallback, state, counters);
                if (partition.no_ && i.trueEnd_ < i.end_)
                    Range(event, i.trueEnd_, i.end_, partition.no_, count, spots, numeraire, fallback, state, counters);
            }
        }

        template <size_t L_>
        static void Execute(const Instruction_& i, uint64_t mask, size_t count, const Row_<L_>& spots, double numeraire, State_<L_>* state) {
            auto& d = state->stack_;
            auto& b = state->booleans_;
            auto& v = state->variables_;
            const size_t x = i.dst_, y = i.right_;
            const double k = i.constant_;
            // Dispatch once per opcode; the full-mask loop has contiguous lane storage.
#define SOA_CASE(OP, EXPR)                                                                                                                           \
    case OP:                                                                                                                                         \
        Each<L_>(mask, count, [&](size_t lane) { EXPR; });                                                                                           \
        break
            switch (i.op_) {
                SOA_CASE(Spot, d[x][lane] = spots[lane]);
                SOA_CASE(Var, d[x][lane] = v[i.left_][lane]);
                SOA_CASE(Const, d[x][lane] = k);
                SOA_CASE(ConstVar, d[x][lane] = k);
                SOA_CASE(Assign, v[x][lane] = d[i.left_][lane]);
                SOA_CASE(AssignConst, v[x][lane] = k);
                SOA_CASE(Pays, v[x][lane] += d[i.left_][lane] / numeraire);
                SOA_CASE(PaysConst, v[x][lane] += k / numeraire);
                SOA_CASE(Add, d[x][lane] += d[y][lane]);
                SOA_CASE(Sub, d[x][lane] -= d[y][lane]);
                SOA_CASE(Multi, d[x][lane] *= d[y][lane]);
                SOA_CASE(Div, d[x][lane] /= d[y][lane]);
                SOA_CASE(Max2, d[x][lane] = std::max(d[x][lane], d[y][lane]));
                SOA_CASE(Min2, d[x][lane] = std::min(d[x][lane], d[y][lane]));
                SOA_CASE(AddConst, d[x][lane] += k);
                SOA_CASE(SubConst, d[x][lane] -= k);
                SOA_CASE(ConstSub, d[x][lane] = k - d[x][lane]);
                SOA_CASE(MultiConst, d[x][lane] *= k);
                SOA_CASE(DivConst, d[x][lane] /= k);
                SOA_CASE(ConstDiv, d[x][lane] = k / d[x][lane]);
                SOA_CASE(Max2Const, d[x][lane] = std::max(d[x][lane], k));
                SOA_CASE(Min2Const, d[x][lane] = std::min(d[x][lane], k));
                SOA_CASE(ConstMax2, d[x][lane] = std::max(k, d[x][lane]));
                SOA_CASE(ConstMin2, d[x][lane] = std::min(k, d[x][lane]));
                SOA_CASE(UMinus, d[x][lane] = -d[x][lane]);
                SOA_CASE(Sqrt, d[x][lane] = std::sqrt(d[x][lane]));
                SOA_CASE(Log, d[x][lane] = std::log(d[x][lane]));
                SOA_CASE(Exp, d[x][lane] = std::exp(d[x][lane]));
                SOA_CASE(Equal, b[x][lane] = d[i.left_][lane] == 0.0);
                SOA_CASE(Sup, b[x][lane] = d[i.left_][lane] > 0.0);
                SOA_CASE(SupEqual, b[x][lane] = d[i.left_][lane] >= 0.0);
                SOA_CASE(True, b[x][lane] = true);
                SOA_CASE(False, b[x][lane] = false);
                SOA_CASE(Not, b[x][lane] = !b[x][lane]);
                SOA_CASE(And, b[x][lane] = b[x][lane] && b[y][lane]);
                SOA_CASE(Or, b[x][lane] = b[x][lane] || b[y][lane]);
            default:
                THROW("SoA: invalid decoded instruction");
            }
#undef SOA_CASE
        }

        template <size_t L_>
        void Range(size_t event,
                   size_t first,
                   size_t last,
                   uint64_t mask,
                   size_t count,
                   const Row_<L_>& spots,
                   double numeraire,
                   bool fallback,
                   State_<L_>* state,
                   Counters_* counters) const {
            const auto& code = events_[event];
            for (size_t pc = first; pc < last; ++pc) {
                const auto& i = code[pc];
                if (i.op_ == If || i.op_ == IfElse) {
                    Branch(event, pc, mask, count, spots, numeraire, fallback, state, counters);
                    pc = i.end_ - 1;
                } else {
                    Execute(i, mask, count, spots, numeraire, state);
                }
            }
        }

        template <size_t L_> void ValidateWorkspace(const State_<L_>& state) const {
            REQUIRE(state.owner_ == this && state.variables_.size() == seed_.size() && state.stack_.size() == maxDouble_ &&
                        state.booleans_.size() == maxBool_,
                    "SoA: workspace size mismatch");
        }

        template <size_t L_> void ValidateScenario(const std::vector<Row_<L_>>& spots, const Vector_<>& numeraires, size_t count) const {
            static_assert(L_ == 8 || L_ == 16 || L_ == 32 || L_ == 64, "SoA study supports 8-64 lanes");
            REQUIRE(count > 0 && count <= L_, "SoA: invalid lane count");
            REQUIRE(spots.size() == events_.size() && numeraires.size() == events_.size(), "SoA: scenario size mismatch");
        }

    public:
        template <size_t L_>
        void Evaluate(const std::vector<Row_<L_>>& spots,
                      const Vector_<>& numeraires,
                      size_t count,
                      bool fallback,
                      State_<L_>* state,
                      Counters_* counters) const {
            ValidateScenario(spots, numeraires, count);
            ValidateWorkspace(*state);
            for (size_t variable = 0; variable < seed_.size(); ++variable)
                std::fill_n(state->variables_[variable].begin(), count, seed_[variable]);
            const uint64_t mask = count == 64 ? ~uint64_t(0) : (uint64_t(1) << count) - 1;
            for (size_t event = 0; event < events_.size(); ++event)
                Range(event, 0, events_[event].size(), mask, count, spots[event], numeraires[event], fallback, state, counters);
        }
    };
} // namespace Dal::Script::SoaStudy
