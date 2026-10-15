//
// Created by Codex on 2026/10/16.
//

#pragma once

#include <array>
#include <cstdint>
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

        void Decode(size_t event, size_t first, size_t last, size_t* doubles, size_t* booleans) {
            const auto& stream = scalar_.NodeStreams()[event];
            const auto& values = scalar_.ConstStreams()[event];
            auto& out = events_[event];
            auto operand = [&](size_t* pc) {
                REQUIRE(++*pc < last && stream[*pc] >= 0, "SoA: missing or negative operand");
                return static_cast<size_t>(stream[*pc]);
            };
            auto constant = [&](size_t* pc) {
                const size_t i = operand(pc);
                REQUIRE(i < values.size(), "SoA: constant index out of range");
                return values[i];
            };
            auto pop = [](size_t* depth) {
                REQUIRE(*depth > 0, "SoA: stack underflow");
                return --*depth;
            };
            for (size_t pc = first; pc < last; ++pc) {
                Instruction_ item{stream[pc]};
                const int op = item.op_;
                if (op == If || op == IfElse) {
                    item.left_ = pop(booleans);
                    REQUIRE(*doubles == 0 && *booleans == 0, "SoA: IF must be a statement");
                    item.sourceFalse_ = operand(&pc);
                    item.sourceEnd_ = op == IfElse ? operand(&pc) : item.sourceFalse_;
                    item.sourceTrue_ = pc + 1;
                    REQUIRE(item.sourceTrue_ <= item.sourceFalse_ && item.sourceFalse_ <= item.sourceEnd_ && item.sourceEnd_ <= last,
                            "SoA: invalid IF range");
                    const size_t slot = out.size();
                    out.push_back(item);
                    Decode(event, item.sourceTrue_, item.sourceFalse_, doubles, booleans);
                    REQUIRE(*doubles == 0 && *booleans == 0, "SoA: unbalanced true branch");
                    out[slot].trueEnd_ = out.size();
                    Decode(event, item.sourceFalse_, item.sourceEnd_, doubles, booleans);
                    REQUIRE(*doubles == 0 && *booleans == 0, "SoA: unbalanced false branch");
                    out[slot].end_ = out.size();
                    pc = item.sourceEnd_ - 1;
                    continue;
                }
                if (op == Spot || op == Const || op == Var || op == ConstVar) {
                    item.dst_ = (*doubles)++;
                    if (op == Const)
                        item.constant_ = constant(&pc);
                    if (op == Var || op == ConstVar) {
                        item.left_ = operand(&pc);
                        REQUIRE(item.left_ < (op == Var ? seed_.size() : constants_.size()), "SoA: variable index out of range");
                        if (op == ConstVar)
                            item.constant_ = constants_[item.left_];
                    }
                } else if (op == Assign || op == Pays || op == AssignConst || op == PaysConst) {
                    if (op == AssignConst || op == PaysConst)
                        item.constant_ = constant(&pc);
                    else
                        item.left_ = pop(doubles);
                    item.dst_ = operand(&pc);
                    REQUIRE(item.dst_ < seed_.size(), "SoA: store index out of range");
                } else if (op == Add || op == Sub || op == Multi || op == Div || op == Max2 || op == Min2) {
                    item.right_ = pop(doubles);
                    REQUIRE(*doubles > 0, "SoA: binary stack underflow");
                    item.dst_ = *doubles - 1;
                } else if (op == AddConst || op == SubConst || op == ConstSub || op == MultiConst || op == DivConst || op == ConstDiv ||
                           op == Max2Const || op == Min2Const || op == ConstMax2 || op == ConstMin2) {
                    REQUIRE(*doubles > 0, "SoA: scalar stack underflow");
                    item.dst_ = *doubles - 1;
                    item.constant_ = constant(&pc);
                } else if (op == Sup || op == SupEqual || op == Equal) {
                    item.left_ = pop(doubles);
                    item.dst_ = (*booleans)++;
                } else if (op == True || op == False) {
                    item.dst_ = (*booleans)++;
                } else if (op == Not) {
                    REQUIRE(*booleans > 0, "SoA: NOT stack underflow");
                    item.dst_ = *booleans - 1;
                } else if (op == And || op == Or) {
                    item.right_ = pop(booleans);
                    REQUIRE(*booleans > 0, "SoA: boolean stack underflow");
                    item.dst_ = *booleans - 1;
                } else if (op == UMinus || op == Sqrt || op == Log || op == Exp) {
                    REQUIRE(*doubles > 0, "SoA: unary stack underflow");
                    item.dst_ = *doubles - 1;
                } else {
                    THROW("SoA: unsupported opcode " + std::to_string(op));
                }
                maxDouble_ = std::max(maxDouble_, *doubles);
                maxBool_ = std::max(maxBool_, *booleans);
                out.push_back(item);
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
            auto& d = state->stack_;
            auto& b = state->booleans_;
            auto& v = state->variables_;
            const auto& code = events_[event];
            for (size_t pc = first; pc < last; ++pc) {
                const auto& i = code[pc];
                const size_t x = i.dst_, y = i.right_;
                const double k = i.constant_;
                if (i.op_ == If || i.op_ == IfElse) {
                    uint64_t yes = 0;
                    size_t active = 0, selected = 0;
                    Each<L_>(mask, count, [&](size_t lane) {
                        ++active;
                        if (b[i.left_][lane]) {
                            yes |= uint64_t(1) << lane;
                            ++selected;
                        }
                    });
                    const uint64_t no = mask ^ yes;
                    if (yes && no)
                        ++counters->mixed_;
                    if (fallback && yes && no && 4 * std::min(selected, active - selected) >= active) {
                        Each<L_>(mask, count, [&](size_t lane) {
                            auto& scalar = state->fallback_;
                            for (size_t variable = 0; variable < v.size(); ++variable)
                                scalar.variables_[variable] = v[variable][lane];
                            AAD::Sample_<double> sample{};
                            sample.spot_ = spots[lane];
                            sample.numeraire_ = numeraire;
                            const bool takeTrue = (yes & (uint64_t(1) << lane)) != 0;
                            const size_t begin = takeTrue ? i.sourceTrue_ : i.sourceFalse_;
                            const size_t end = takeTrue ? i.sourceFalse_ : i.sourceEnd_;
                            if (begin < end)
                                Detail::EvalCompiledRange<false>(scalar_.NodeStreams()[event], scalar_.ConstStreams()[event], sample, scalar, begin,
                                                                 end, true);
                            for (size_t variable = 0; variable < v.size(); ++variable)
                                v[variable][lane] = scalar.variables_[variable];
                            ++counters->fallbackLanes_;
                        });
                    } else {
                        if (yes)
                            Range(event, pc + 1, i.trueEnd_, yes, count, spots, numeraire, fallback, state, counters);
                        if (no && i.trueEnd_ < i.end_)
                            Range(event, i.trueEnd_, i.end_, no, count, spots, numeraire, fallback, state, counters);
                    }
                    pc = i.end_ - 1;
                    continue;
                }
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
        }

    public:
        template <size_t L_>
        void Evaluate(const std::vector<Row_<L_>>& spots,
                      const Vector_<>& numeraires,
                      size_t count,
                      bool fallback,
                      State_<L_>* state,
                      Counters_* counters) const {
            static_assert(L_ == 8 || L_ == 16 || L_ == 32 || L_ == 64, "SoA study supports 8-64 lanes");
            REQUIRE(count > 0 && count <= L_, "SoA: invalid lane count");
            REQUIRE(spots.size() == events_.size() && numeraires.size() == events_.size(), "SoA: scenario size mismatch");
            REQUIRE(state->owner_ == this && state->variables_.size() == seed_.size() && state->stack_.size() == maxDouble_ &&
                        state->booleans_.size() == maxBool_,
                    "SoA: workspace size mismatch");
            for (size_t variable = 0; variable < seed_.size(); ++variable)
                std::fill_n(state->variables_[variable].begin(), count, seed_[variable]);
            const uint64_t mask = count == 64 ? ~uint64_t(0) : (uint64_t(1) << count) - 1;
            for (size_t event = 0; event < events_.size(); ++event)
                Range(event, 0, events_[event].size(), mask, count, spots[event], numeraires[event], fallback, state, counters);
        }
    };
} // namespace Dal::Script::SoaStudy
