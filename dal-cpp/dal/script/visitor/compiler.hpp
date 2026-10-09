//
// Created by wegam on 2023/1/26.
//

/*
Written by Antoine Savine in 2018

This code is the strict IP of Antoine Savine

License to use and alter this code for personal and commercial applications
is freely granted to any person or company who purchased a copy of the book

Modern Computational Finance: Scripting for Derivatives and XVA
Jesper Andreasen & Antoine Savine
Wiley, 2018

As long as this comment is preserved at the Top of the file
*/

#pragma once

#include <algorithm>
#include <array>
#include <dal/math/aad/sample.hpp>
#include <dal/math/stacks.hpp>
#include <dal/script/node.hpp>
#include <dal/script/observationplan.hpp>
#include <dal/script/visitor.hpp>
#include <dal/script/visitor/evalstate.hpp>
#include <dal/script/visitor/smoothing.hpp>
#include <dal/script/visitor/vectorops.hpp>
#include <dal/utilities/exceptions.hpp>
#include <functional>
#include <iostream>

namespace Dal::Script {
    //  Recording sinks installed by the LSMC driver (dal/script/lsmc.cpp) before
    //  evaluating a compiled EXERCISE stream: raw payments per PAYS event and the
    //  (x, h, condition) triple per exercise event land in the driver's storage
    //  rows; the driver advances eventOrdinal_ and pathSlot_ between events/paths.
    struct LsmcSinks_ {
        const ObservationPlan_* plan_ = nullptr;
        const Vector_<size_t>* eventToPays_ = nullptr;
        const Vector_<size_t>* eventToExercise_ = nullptr;
        LsmcRows_* pays_ = nullptr;
        LsmcRows_* x_ = nullptr;
        LsmcRows_* h_ = nullptr;
        Vector_<Vector_<char>>* cond_ = nullptr; //  empty row = unconditional day
        std::array<double, 3> pricingFeatures_{};
        double pricingH_ = 0.0;
        bool pricingCond_ = true;
        size_t eventOrdinal_ = 0;
        size_t pathSlot_ = 0;
        size_t payoffIdx_ = static_cast<size_t>(-1);
    };

    template <class T_> struct EvalState_ : EvalStateCore_<T_> {
        //  Fuzzy If blend state.
        double defEps_ = 0.0;
        size_t nestedIfLvl_ = 0;
        Vector_<Vector_<T_>> varStore0_;
        Vector_<Vector_<T_>> varStore1_;
        Vector_<Vector_<Vector_<T_>>> vectorStore0_;
        Vector_<Vector_<Vector_<T_>>> vectorStore1_;
        const ObservationPlan_* observations_ = nullptr;
        const AAD::Scenario_<T_>* scenario_ = nullptr;
        //  Engaged only while the LSMC driver evaluates a recording stream
        LsmcSinks_* lsmcSinks_ = nullptr;
        //  Engaged only while the LSMC driver replays a fuzzy (AAD) recording stream
        LsmcFuzzySinks_<T_>* lsmcFuzzySinks_ = nullptr;

        explicit EvalState_(const Vector_<>& variables,
                            const Vector_<T_>& constVariables = Vector_<T_>(),
                            size_t maxNestedIfs = 0,
                            double defEps = 0.0,
                            const Vector_<size_t>& vectorCapacities = {})
            : EvalStateCore_<T_>(variables, constVariables, vectorCapacities), defEps_(defEps), varStore0_(maxNestedIfs), varStore1_(maxNestedIfs),
              vectorStore0_(maxNestedIfs), vectorStore1_(maxNestedIfs) {
            ResizeVarStores(&varStore0_, &varStore1_, variables.size());
            ResizeVectorStores(&vectorStore0_, &vectorStore1_, this->vectors_.size());
        }

        void Init() {
            EvalStateCore_<T_>::Init();
            nestedIfLvl_ = 0;
        }
    };

    //  Hand-written because opcodes are NTTPs and serialized stream integers.
    //  Streams are in-memory only (never archived), so the numbering may be rearranged;
    //  the dispatch tiers below rely on the tier blocks staying contiguous.
    enum NodeType_ {
        Add = 0,
        AddConst = 1,
        Sub = 2,
        SubConst = 3,
        ConstSub = 4,
        Multi = 5,
        MultiConst = 6,
        Div = 7,
        DivConst = 8,
        ConstDiv = 9,
        Pow = 10,
        PowConst = 11,
        ConstPow = 12,
        Max2 = 13,
        Max2Const = 14,
        Min2 = 15,
        Min2Const = 16,
        Spot = 17,
        Var = 18,
        Const = 19,
        Assign = 20,
        AssignConst = 21,
        Pays = 22,
        PaysConst = 23,
        //  Delayed payments: the extra leading slot operand indexes the event sample's
        //  discount factors; layout PaysOn slot idx / PaysOnConst constIdx slot idx.
        PaysOn = 24,
        PaysOnConst = 25,
        If = 26,
        IfElse = 27,
        Equal = 28,
        Sup = 29,
        SupEqual = 30,
        And = 31,
        Or = 32,
        Sqrt = 33,
        Log = 34,
        Exp = 35,
        Not = 36,
        UMinus = 37,
        True = 38,
        False = 39,
        ConstVar = 40,
        //  Fuzzy opcodes.
        FuzzyEqual = 41,
        FuzzyEqualDiscrete = 42,
        FuzzyComp = 43,
        FuzzyCompDiscrete = 44,
        FuzzyAnd = 45,
        FuzzyOr = 46,
        FuzzyNot = 47,
        FuzzyTrue = 48,
        FuzzyFalse = 49,
        FuzzyIf = 50, //  operands: lastTrue, lastFalse, nAff, aff..., nVec, vec...
        LoadObservation = 51,
        Discard = 52,
        //  LSMC recording opcodes (prepared streams only): the LSMC driver installs
        //  LsmcSinks_ into EvalState_ before evaluating; operands mirror Pays/PaysConst
        //  (plus the PaysOn slot variants), LsmcExercise carries a single hasCond operand.
        //  The fuzzy variants record into the driver's typed LsmcFuzzySinks_ instead:
        //  conditions land on the double stack as degrees, so LsmcFuzzyExercise pops its
        //  condition from there.
        LsmcPays = 53,
        LsmcPaysConst = 54,
        LsmcPaysOn = 55,
        LsmcPaysOnConst = 56,
        LsmcExercise = 57,
        LsmcFuzzyPays = 58,
        LsmcFuzzyPaysConst = 59,
        LsmcFuzzyPaysOn = 60,
        LsmcFuzzyPaysOnConst = 61,
        LsmcFuzzyExercise = 62,
        VectorRead = 63,
        VectorAssign = 64,
        VectorAppend = 65,
        VectorReduce = 66
    };

    class Compiler_ : public ConstVisitor_<Compiler_> {
        Vector_<int> nodeStream_;
        Vector_<double> constStream_;
        const bool fuzzy_;
        const ObservationPlan_* observations_;
        const bool historical_;
        //  LSMC recording mode: EXERCISE products lowered for the LSMC driver, which
        //  reads the recorded payments and exercise triples through LsmcSinks_
        const bool lsmc_;

    public:
        explicit Compiler_(bool fuzzy = false, const ObservationPlan_* observations = nullptr, bool historical = false, bool lsmc = false)
            : fuzzy_(fuzzy && !historical), observations_(observations), historical_(historical), lsmc_(lsmc && !historical) {}

        using ConstVisitor_<Compiler_>::Visit;
        [[nodiscard]] const Vector_<int>& NodeStream() const { return nodeStream_; }
        [[nodiscard]] const Vector_<double>& ConstStream() const { return constStream_; }

        void EmitVectorSource(const SourceLocation_& source) {
            nodeStream_.emplace_back(static_cast<int>(source.line_));
            nodeStream_.emplace_back(static_cast<int>(source.column_));
            nodeStream_.emplace_back(static_cast<int>(source.row_));
        }

        template <NodeType_ IfBin, NodeType_ IfConstLeft, NodeType_ IfConstRight> void VisitBinary(const ExprNode_& node) {
            if (node.isConst_) {
                nodeStream_.emplace_back(Const);
                nodeStream_.emplace_back(int(constStream_.size()));
                constStream_.emplace_back(node.constVal_);
            } else {
                const auto* lhs = Downcast<ExprNode_>(node.arguments_[0]);
                const auto* rhs = Downcast<ExprNode_>(node.arguments_[1]);

                if (lhs->isConst_) {
                    node.arguments_[1]->Accept(*this);
                    nodeStream_.emplace_back(IfConstLeft);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(lhs->constVal_);
                } else if (rhs->isConst_) {
                    node.arguments_[0]->Accept(*this);
                    nodeStream_.emplace_back(IfConstRight);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(rhs->constVal_);
                } else {
                    node.arguments_[0]->Accept(*this);
                    node.arguments_[1]->Accept(*this);
                    nodeStream_.emplace_back(IfBin);
                }
            }
        }

        void Visit(const NodeAdd_& node) { VisitBinary<Add, AddConst, AddConst>(node); }
        void Visit(const NodeSub_& node) { VisitBinary<Sub, ConstSub, SubConst>(node); }
        void Visit(const NodeMulti_& node) { VisitBinary<Multi, MultiConst, MultiConst>(node); }
        void Visit(const NodeDiv_& node) { VisitBinary<Div, ConstDiv, DivConst>(node); }
        void Visit(const NodePow_& node) { VisitBinary<Pow, ConstPow, PowConst>(node); }

        void Visit(const NodeMax_& node) { VisitBinary<Max2, Max2Const, Max2Const>(node); }

        void Visit(const NodeMin_& node) { VisitBinary<Min2, Min2Const, Min2Const>(node); }

        template <NodeType_ NT> void VisitUnary(const ExprNode_& node) {
            if (node.isConst_) {
                nodeStream_.emplace_back(Const);
                nodeStream_.emplace_back(int(constStream_.size()));
                constStream_.emplace_back(node.constVal_);
            } else {
                node.arguments_[0]->Accept(*this);
                nodeStream_.emplace_back(NT);
            }
        }

        void Visit(const NodeUPlus_& node) { node.arguments_[0]->Accept(*this); }

        void Visit(const NodeUMinus_& node) { VisitUnary<UMinus>(node); }
        void Visit(const NodeLog_& node) { VisitUnary<Log>(node); }
        void Visit(const NodeSqrt_& node) { VisitUnary<Sqrt>(node); }
        void Visit(const NodeExp_& node) { VisitUnary<Exp>(node); }

        template <NodeType_ NT, typename OP> void VisitCondition(const CompNode_& node, OP op) {
            if (fuzzy_) {
                //  Keep fuzzy conditions as smoothed runtime opcodes.
                node.arguments_[0]->Accept(*this);
                if (node.isDiscrete_) {
                    nodeStream_.emplace_back(NT == Equal ? FuzzyEqualDiscrete : FuzzyCompDiscrete);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(node.lb_);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(node.rb_);
                } else {
                    nodeStream_.emplace_back(NT == Equal ? FuzzyEqual : FuzzyComp);
                    nodeStream_.emplace_back(int(constStream_.size()));
                    constStream_.emplace_back(node.eps_);
                }
                return;
            }

            const auto* arg = Downcast<ExprNode_>(node.arguments_[0]);

            if (arg->isConst_) {
                nodeStream_.emplace_back(op(arg->constVal_) ? True : False);

            } else {
                node.arguments_[0]->Accept(*this);
                nodeStream_.emplace_back(NT);
            }
        }

        void Visit(const NodeEqual_& node) {
            VisitCondition<Equal>(node, [](double x) { return x == 0.0; });
        }

        void Visit(const NodeSup_& node) {
            VisitCondition<Sup>(node, [](double x) { return x > 0.0; });
        }
        void Visit(const NodeSupEqual_& node) {
            VisitCondition<SupEqual>(node, [](double x) { return x >= 0.0; });
        }

        template <NodeType_ Hard, NodeType_ Soft> void VisitBoolBinary(const Node_& node) {
            node.arguments_[0]->Accept(*this);
            node.arguments_[1]->Accept(*this);
            nodeStream_.emplace_back(fuzzy_ ? Soft : Hard);
        }

        void Visit(const NodeAnd_& node) { VisitBoolBinary<And, FuzzyAnd>(node); }
        void Visit(const NodeOr_& node) { VisitBoolBinary<Or, FuzzyOr>(node); }

        void Visit(const NodeNot_& node) {
            node.arguments_[0]->Accept(*this);
            nodeStream_.emplace_back(fuzzy_ ? FuzzyNot : Not);
        }

        template <NodeType_ NT, NodeType_ NTConst> void VisitAssignLike(const Node_& node) {
            const auto* var = Downcast<NodeVar_>(node.arguments_[0]);
            const auto* rhs = Downcast<ExprNode_>(node.arguments_[1]);
            if (rhs->isConst_) {
                nodeStream_.emplace_back(NTConst);
                nodeStream_.emplace_back(static_cast<int>(constStream_.size()));
                constStream_.emplace_back(rhs->constVal_);
            } else {
                node.arguments_[1]->Accept(*this);
                nodeStream_.emplace_back(NT);
            }
            nodeStream_.emplace_back(static_cast<int>(var->index_));
        }

        void Visit(const NodeAssign_& node) { VisitAssignLike<Assign, AssignConst>(node); }

        //  Emit [rhs] op [constIdx] [slot] varIdx: the slot operand is present only on the
        //  PaysOn variants and addresses the event sample's discount factors. The historical
        //  stream discards the RHS without reading a discount (past payments are settled).
        void Visit(const NodePays_& node) {
            if (historical_) {
                node.arguments_[1]->Accept(*this);
                nodeStream_.emplace_back(Discard);
                return;
            }
            const bool delayed = node.discountId_.has_value();
            if (!delayed && node.paymentDate_)
                THROW2("PreparationRequired: PAYS ... ON " + Date::ToString(*node.paymentDate_) +
                           " requires model-aware preparation; " + node.source_.Describe(),
                       ScriptError_);
            const int slot = delayed ? static_cast<int>(*node.discountId_) : -1;
            const auto* var = Downcast<NodeVar_>(node.arguments_[0]);
            const auto* rhs = Downcast<ExprNode_>(node.arguments_[1]);
            NodeType_ plain, plainConst, on, onConst;
            if (lsmc_) {
                if (fuzzy_) {
                    plain = LsmcFuzzyPays;
                    plainConst = LsmcFuzzyPaysConst;
                    on = LsmcFuzzyPaysOn;
                    onConst = LsmcFuzzyPaysOnConst;
                } else {
                    plain = LsmcPays;
                    plainConst = LsmcPaysConst;
                    on = LsmcPaysOn;
                    onConst = LsmcPaysOnConst;
                }
            } else {
                plain = Pays;
                plainConst = PaysConst;
                on = PaysOn;
                onConst = PaysOnConst;
            }
            if (rhs->isConst_) {
                nodeStream_.emplace_back(delayed ? onConst : plainConst);
                nodeStream_.emplace_back(int(constStream_.size()));
                constStream_.emplace_back(rhs->constVal_);
            } else {
                node.arguments_[1]->Accept(*this);
                nodeStream_.emplace_back(delayed ? on : plain);
            }
            if (delayed)
                nodeStream_.emplace_back(slot);
            nodeStream_.emplace_back(static_cast<int>(var->index_));
        }

        //  Evaluation order mirrors the tree-walk recorder: the exercise value lands on
        //  the double stack; a hard condition goes to the boolean stack, a fuzzy condition
        //  is a degree on the double stack (matching the fuzzy comparison opcodes)
        void Visit(const NodeExercise_& node) {
            REQUIRE2(lsmc_, "UnsupportedExecutionMode: EXERCISE requires the LSMC simulation driver; " + node.source_.Describe(), ScriptError_);
            node.arguments_[0]->Accept(*this);
            int hasCond = 0;
            if (node.arguments_.size() > 1) {
                node.arguments_[1]->Accept(*this);
                hasCond = 1;
            }
            nodeStream_.emplace_back(fuzzy_ ? LsmcFuzzyExercise : LsmcExercise);
            nodeStream_.emplace_back(hasCond);
        }

        void Visit(const NodeVar_& node) {
            nodeStream_.emplace_back(Var);
            nodeStream_.emplace_back(node.index_);
        }

        void Visit(const NodeConstVar_& node) {
            nodeStream_.emplace_back(ConstVar);
            nodeStream_.emplace_back(node.index_);
        }

        void Visit(const NodeVectorEntry_& node) {
            nodeStream_.emplace_back(VectorRead);
            nodeStream_.emplace_back(node.index_);
            nodeStream_.emplace_back(static_cast<int>(node.entry_));
            EmitVectorSource(node.source_);
        }

        void Visit(const NodeVectorAssign_& node) {
            const auto* entry = Downcast<NodeVectorEntry_>(node.arguments_[0]);
            node.arguments_[1]->Accept(*this);
            nodeStream_.emplace_back(VectorAssign);
            nodeStream_.emplace_back(entry->index_);
            nodeStream_.emplace_back(static_cast<int>(entry->entry_));
        }

        void Visit(const NodeVectorAppend_& node) {
            node.arguments_[0]->Accept(*this);
            nodeStream_.emplace_back(VectorAppend);
            nodeStream_.emplace_back(node.index_);
        }

        void Visit(const NodeVectorReduce_& node) {
            nodeStream_.emplace_back(VectorReduce);
            nodeStream_.emplace_back(node.index_);
            nodeStream_.emplace_back(static_cast<int>(node.kind_));
            EmitVectorSource(node.source_);
        }

        void Visit(const NodeConst_& node) {
            nodeStream_.emplace_back(Const);
            nodeStream_.emplace_back(static_cast<int>(constStream_.size()));
            constStream_.emplace_back(node.constVal_);
        }

        void Visit(const NodeTrue_&) { nodeStream_.emplace_back(fuzzy_ ? FuzzyTrue : True); }

        void Visit(const NodeFalse_&) { nodeStream_.emplace_back(fuzzy_ ? FuzzyFalse : False); }

        void LoadPreparedObservation(size_t id) {
            REQUIRE2(observations_, "PreparationRequired: compiled observation requires a plan", ScriptError_);
            const auto& request = observations_->Request(id);
            REQUIRE2(request.historyValueId_ || (!historical_ && request.modelSlot_), "UnresolvedModelObservation", ScriptError_);
            nodeStream_.emplace_back(LoadObservation);
            nodeStream_.emplace_back(static_cast<int>(id));
        }

        void Visit(const NodeSpot_& node) {
            if (node.observationId_)
                LoadPreparedObservation(*node.observationId_);
            else {
                REQUIRE2(!historical_, "UnboundHistoricalSpot: SPOT() requires a default index", ScriptError_);
                nodeStream_.emplace_back(Spot);
            }
        }
        void Visit(const NodeFix_& node) {
            if (!node.observationId_)
                node.RequirePreparation();
            LoadPreparedObservation(*node.observationId_);
        }

        void Visit(const NodeCollect_& node) { VisitArguments(node); }

        void CompileHardIf(const NodeIf_& node, size_t lastTrue, size_t n) {
            nodeStream_.emplace_back(node.HasElse() ? IfElse : If);
            const size_t thisSpace = nodeStream_.size() - 1;
            nodeStream_.emplace_back(0);
            if (node.HasElse())
                nodeStream_.emplace_back(0);

            for (size_t i = 1; i <= lastTrue; ++i) {
                node.arguments_[i]->Accept(*this);
            }
            nodeStream_[thisSpace + 1] = int(nodeStream_.size());

            if (node.HasElse()) {
                for (size_t i = node.firstElse_; i < n; ++i) {
                    node.arguments_[i]->Accept(*this);
                }
                nodeStream_[thisSpace + 2] = int(nodeStream_.size());
            }
        }

        void CompileFuzzyIf(const NodeIf_& node, size_t lastTrue, size_t n) {
            //  Layout: FuzzyIf lastTrue lastFalse nAff aff... nVec vec... [true][false]
            nodeStream_.emplace_back(FuzzyIf);
            const size_t thisSpace = nodeStream_.size() - 1;
            nodeStream_.emplace_back(0);
            nodeStream_.emplace_back(0);
            nodeStream_.emplace_back(int(node.affectedVars_.size()));
            for (const auto idx : node.affectedVars_)
                nodeStream_.emplace_back(int(idx));
            nodeStream_.emplace_back(int(node.affectedVectors_.size()));
            for (const auto idx : node.affectedVectors_)
                nodeStream_.emplace_back(int(idx));

            for (size_t i = 1; i <= lastTrue; ++i)
                node.arguments_[i]->Accept(*this);
            nodeStream_[thisSpace + 1] = int(nodeStream_.size());

            if (node.HasElse())
                for (size_t i = node.firstElse_; i < n; ++i)
                    node.arguments_[i]->Accept(*this);
            nodeStream_[thisSpace + 2] = int(nodeStream_.size());
        }

        void Visit(const NodeIf_& node) {
            node.arguments_[0]->Accept(*this);

            const auto lastTrue = node.LastTrueIndex();
            const auto n = node.arguments_.size();

            if (fuzzy_)
                CompileFuzzyIf(node, lastTrue, n);
            else
                CompileHardIf(node, lastTrue, n);
        }
    };

    template <class T_>
    inline void EvalCompiled(const Vector_<int>& nodeStream,
                             const Vector_<double>& constStream,
                             const AAD::Sample_<T_>& scenario,
                             EvalState_<T_>& state,
                             size_t first = 0,
                             size_t last = 0,
                             bool reset = true);

    namespace Detail {
        struct DefaultCompiledPolicy_ {
            static constexpr bool trace_ = false;
            template <class T_> T_ Read(size_t requestId, const EvalState_<T_>& state) const {
                REQUIRE2(state.observations_, "PreparationRequired: compiled observation requires a plan", ScriptError_);
                return state.observations_->Read(requestId, state.scenario_);
            }
        };

        template <bool Prepared_, bool Lsmc_ = false, class T_, class P_ = DefaultCompiledPolicy_>
        inline void EvalCompiledRange(const Vector_<int>& nodeStream,
                                      const Vector_<double>& constStream,
                                      const AAD::Sample_<T_>& scenario,
                                      EvalState_<T_>& state,
                                      size_t first,
                                      size_t last,
                                      bool reset,
                                      const P_& policy = {});

        template <class T_, class P_ = DefaultCompiledPolicy_> struct CompiledEventView_ {
            const Vector_<int>& nodeStream_;
            const Vector_<double>& constStream_;
            const AAD::Sample_<T_>& scenario_;
            size_t first_ = 0;
            size_t last_ = 0;
            bool reset_ = true;
            P_ policy_ = {};
        };

        [[noreturn]] inline void ThrowUnknownCompiledOpcode(int op) { THROW("unknown compiled script opcode: " + std::to_string(op)); }

        template <class T_, class C_> FORCE_INLINE void UpdateCompiledExtremum(T_* value, const T_& other, C_ compare) {
            if (compare(other, *value))
                *value = other;
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledSum(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Add: {
                dStack[1] += dStack.Top();
                dStack.Pop();
                ++i;
                return i;
            }
            case AddConst: {
                dStack.Top() += constStream[nodeStream[++i]];
                ++i;
                return i;
            }
            case Sub: {
                dStack[1] -= dStack.Top();
                dStack.Pop();
                ++i;
                return i;
            }
            case SubConst: {
                dStack.Top() -= constStream[nodeStream[++i]];
                ++i;
                return i;
            }
            case ConstSub: {
                dStack.Top() = constStream[nodeStream[++i]] - dStack.Top();
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledProduct(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Multi: {
                dStack[1] *= dStack.Top();
                dStack.Pop();
                ++i;
                return i;
            }
            case MultiConst: {
                dStack.Top() *= constStream[nodeStream[++i]];
                ++i;
                return i;
            }
            case Div: {
                dStack[1] /= dStack.Top();
                dStack.Pop();
                ++i;
                return i;
            }
            case DivConst: {
                dStack.Top() /= constStream[nodeStream[++i]];
                ++i;
                return i;
            }
            case ConstDiv: {
                dStack.Top() = constStream[nodeStream[++i]] / dStack.Top();
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledPower(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Pow: {
                dStack[1] = pow(dStack[1], dStack.Top());
                dStack.Pop();
                ++i;
                return i;
            }
            case PowConst: {
                dStack.Top() = pow(dStack.Top(), constStream[nodeStream[++i]]);
                ++i;
                return i;
            }
            case ConstPow: {
                dStack.Top() = pow(constStream[nodeStream[++i]], dStack.Top());
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_, class C_>
        FORCE_INLINE void
        TraceCompiledExtremum(const CompiledEventView_<T_, P_>& event, size_t pc, const StaticStack_<T_>& stack, const T_& other, C_ compare) {
            if constexpr (P_::trace_)
                event.policy_.Extremum(pc, stack.Top(), other, compare);
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledExtremum(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Max2: {
                const T_ y = dStack.TopAndPop();
                TraceCompiledExtremum(event, i, dStack, y, std::greater<>());
                UpdateCompiledExtremum(&dStack.Top(), y, std::greater<>());
                ++i;
                return i;
            }
            case Max2Const: {
                const T_ y(constStream[nodeStream[++i]]);
                TraceCompiledExtremum(event, i - 1, dStack, y, std::greater<>());
                UpdateCompiledExtremum(&dStack.Top(), y, std::greater<>());
                ++i;
                return i;
            }
            case Min2: {
                const T_ y = dStack.TopAndPop();
                TraceCompiledExtremum(event, i, dStack, y, std::less<>());
                UpdateCompiledExtremum(&dStack.Top(), y, std::less<>());
                ++i;
                return i;
            }
            case Min2Const: {
                const T_ y(constStream[nodeStream[++i]]);
                TraceCompiledExtremum(event, i - 1, dStack, y, std::less<>());
                UpdateCompiledExtremum(&dStack.Top(), y, std::less<>());
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledArithmetic(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            const int op = event.nodeStream_[i];
            if (op <= ConstSub)
                return EvalCompiledSum(event, i, statePtr);
            if (op <= ConstDiv)
                return EvalCompiledProduct(event, i, statePtr);
            if (op <= ConstPow)
                return EvalCompiledPower(event, i, statePtr);
            return EvalCompiledExtremum(event, i, statePtr);
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledLoad(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            auto& scenario = event.scenario_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Spot: {
                dStack.Push(scenario.spot_);
                ++i;
                return i;
            }
            case Var: {
                dStack.Push(state.variables_[nodeStream[++i]]);
                ++i;
                return i;
            }
            case Const: {
                dStack.Push(constStream[nodeStream[++i]]);
                ++i;
                return i;
            }
            case ConstVar: {
                dStack.Push(state.constVariables_[nodeStream[++i]]);
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledStore(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            auto& scenario = event.scenario_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Assign: {
                const size_t idx = nodeStream[++i];
                state.variables_[idx] = dStack.TopAndPop();
                ++i;
                return i;
            }
            case AssignConst: {
                const double val = constStream[nodeStream[++i]];
                const size_t idx = nodeStream[++i];
                state.variables_[idx] = T_(val);
                ++i;
                return i;
            }
            case Pays: {
                const size_t idx = nodeStream[++i];
                state.variables_[idx] += dStack.TopAndPop() / scenario.numeraire_;
                ++i;
                return i;
            }
            case PaysConst: {
                const double val = constStream[nodeStream[++i]];
                const size_t idx = nodeStream[++i];
                state.variables_[idx] += T_(val) / scenario.numeraire_;
                ++i;
                return i;
            }
            case PaysOn: {
                const size_t slot = nodeStream[++i];
                const size_t idx = nodeStream[++i];
                state.variables_[idx] += dStack.TopAndPop() * scenario.discounts_[slot] / scenario.numeraire_;
                ++i;
                return i;
            }
            case PaysOnConst: {
                const double val = constStream[nodeStream[++i]];
                const size_t slot = nodeStream[++i];
                const size_t idx = nodeStream[++i];
                state.variables_[idx] += T_(val) * scenario.discounts_[slot] / scenario.numeraire_;
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledData(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            if (event.nodeStream_[i] <= Const)
                return EvalCompiledLoad(event, i, statePtr);
            return EvalCompiledStore(event, i, statePtr);
        }

        template <bool Prepared_, bool Lsmc_, class T_, class P_>
        FORCE_INLINE size_t EvalCompiledBranch(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& bStack = state.bStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            auto& scenario = event.scenario_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case If: {
                if constexpr (P_::trace_)
                    event.policy_.Branch(i, bStack.Top());
                if (bStack.Top()) {
                    i += 2;
                } else {
                    i = nodeStream[++i];
                }
                bStack.Pop();
                return i;
            }
            case IfElse: {
                if constexpr (P_::trace_)
                    event.policy_.Branch(i, bStack.Top());
                if (!bStack.Top()) {
                    i = nodeStream[++i];
                } else {
                    //  Preserve parent stacks while running the true branch.
                    EvalCompiledRange<Prepared_, Lsmc_>(nodeStream, constStream, scenario, state, i + 3, nodeStream[i + 1], false, event.policy_);
                    i = nodeStream[i + 2];
                }
                bStack.Pop();
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledBoolean(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& bStack = state.bStack_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Equal: {
                bStack.Push(dStack.TopAndPop() == 0);
                ++i;
                return i;
            }
            case Sup: {
                bStack.Push(dStack.TopAndPop() > 0);
                ++i;
                return i;
            }
            case SupEqual: {
                bStack.Push(dStack.TopAndPop() >= 0);
                ++i;
                return i;
            }
            case And: {
                if (bStack[1])
                    bStack[1] = bStack.Top();
                bStack.Pop();
                ++i;
                return i;
            }
            case Or: {
                if (!bStack[1])
                    bStack[1] = bStack.Top();
                bStack.Pop();
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <bool Prepared_, bool Lsmc_, class T_, class P_>
        FORCE_INLINE size_t EvalCompiledControl(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            if (event.nodeStream_[i] <= IfElse)
                return EvalCompiledBranch<Prepared_, Lsmc_>(event, i, statePtr);
            return EvalCompiledBoolean(event, i, statePtr);
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledUnary(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& bStack = state.bStack_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case Sqrt: {
                dStack.Top() = sqrt(dStack.Top());
                ++i;
                return i;
            }
            case Log: {
                dStack.Top() = log(dStack.Top());
                ++i;
                return i;
            }
            case Exp: {
                dStack.Top() = exp(dStack.Top());
                ++i;
                return i;
            }
            case Not: {
                bStack.Top() = !bStack.Top();
                ++i;
                return i;
            }
            case UMinus: {
                dStack.Top() = -dStack.Top();
                ++i;
                return i;
            }
            case True: {
                bStack.Push(true);
                ++i;
                return i;
            }
            case False: {
                bStack.Push(false);
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledScalar(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            if (event.nodeStream_[i] == ConstVar)
                return EvalCompiledLoad(event, i, statePtr);
            return EvalCompiledUnary(event, i, statePtr);
        }

        template <class T_, class P_>
        FORCE_INLINE void TraceCompiledFuzzyComparison(const CompiledEventView_<T_, P_>& event, size_t pc, const EvalState_<T_>& state) {
            if constexpr (P_::trace_) {
                const int op = event.nodeStream_[pc];
                const bool equality = op == FuzzyEqual || op == FuzzyEqualDiscrete;
                const bool discrete = op == FuzzyEqualDiscrete || op == FuzzyCompDiscrete;
                double lower = event.constStream_[event.nodeStream_[pc + 1]];
                double upper;
                if (discrete) {
                    upper = event.constStream_[event.nodeStream_[pc + 2]];
                } else {
                    upper = 0.5 * (lower < 0 ? state.defEps_ : lower);
                    lower = -upper;
                }
                event.policy_.FuzzyComparison(pc, state.dStack_.Top(), lower, upper, equality);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledFuzzyComparison(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            const int op = event.nodeStream_[i];
            TraceCompiledFuzzyComparison(event, i, state);
            switch (op) {
            case FuzzyEqual: {
                const double eps = constStream[nodeStream[++i]];
                dStack.Top() = BFly(dStack.Top(), eps < 0 ? state.defEps_ : eps);
                ++i;
                return i;
            }
            case FuzzyEqualDiscrete: {
                const double lb = constStream[nodeStream[++i]];
                const double rb = constStream[nodeStream[++i]];
                dStack.Top() = BFly(dStack.Top(), lb, rb);
                ++i;
                return i;
            }
            case FuzzyComp: {
                const double eps = constStream[nodeStream[++i]];
                dStack.Top() = CSpr(dStack.Top(), eps < 0 ? state.defEps_ : eps);
                ++i;
                return i;
            }
            case FuzzyCompDiscrete: {
                const double lb = constStream[nodeStream[++i]];
                const double rb = constStream[nodeStream[++i]];
                dStack.Top() = CSpr(dStack.Top(), lb, rb);
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledFuzzyBoolean(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            const int op = event.nodeStream_[i];
            switch (op) {
            case FuzzyAnd: {
                const T_ x = dStack.TopAndPop();
                dStack.Top() *= x;
                ++i;
                return i;
            }
            case FuzzyOr: {
                const T_ x = dStack.TopAndPop();
                const T_ y = dStack.TopAndPop();
                dStack.Push(x + y - x * y);
                ++i;
                return i;
            }
            case FuzzyNot: {
                dStack.Top() = 1.0 - dStack.Top();
                ++i;
                return i;
            }
            case FuzzyTrue: {
                dStack.Push(T_(1.0));
                ++i;
                return i;
            }
            case FuzzyFalse: {
                dStack.Push(T_(0.0));
                ++i;
                return i;
            }
            default:
                ThrowUnknownCompiledOpcode(op);
            }
        }

        template <class T_, class P_>
        FORCE_INLINE void TraceCompiledFuzzyBranch(const CompiledEventView_<T_, P_>& event, size_t pc, const T_& degree) {
            if constexpr (P_::trace_)
                event.policy_.FuzzyBranch(pc, degree);
        }

        template <bool Prepared_, bool Lsmc_, class T_, class P_>
        FORCE_INLINE size_t EvalCompiledFuzzyBranch(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            auto& scenario = event.scenario_;
            //  Layout: FuzzyIf lastTrue lastFalse nAff aff... nVec vec... [true][false]
            const size_t lastTrue = nodeStream[i + 1];
            const size_t lastFalse = nodeStream[i + 2];
            const int nAff = nodeStream[i + 3];
            const size_t firstAff = i + 4;
            const size_t nVecPos = firstAff + nAff;
            const size_t firstVec = nVecPos + 1;
            const size_t firstTrue = firstVec + nodeStream[nVecPos];
            const auto vectorFirst = nodeStream.begin() + firstVec;
            const auto vectorLast = nodeStream.begin() + firstTrue;

            const T_ t = dStack.TopAndPop();
            TraceCompiledFuzzyBranch(event, i, t);
            if (t > 1.0 - EPSILON) {
                EvalCompiledRange<Prepared_, Lsmc_>(nodeStream, constStream, scenario, state, firstTrue, lastTrue, false, event.policy_);
                i = lastFalse;
            } else if (t < EPSILON) {
                i = lastTrue;
            } else {
                REQUIRE(state.nestedIfLvl_ < state.varStore0_.size(), "compiled FuzzyIf nesting exceeds allocated var stores");
                const size_t lvl = state.nestedIfLvl_++;
                for (int k = 0; k < nAff; ++k) {
                    const size_t idx = nodeStream[firstAff + k];
                    state.varStore0_[lvl][idx] = state.variables_[idx];
                }
                SnapshotFuzzyVectors(state.vectors_, &state.vectorStore0_[lvl], vectorFirst, vectorLast);
                if (state.lsmcFuzzySinks_)
                    state.lsmcFuzzySinks_->SnapshotBranchPayment(lvl);
                EvalCompiledRange<Prepared_, Lsmc_>(nodeStream, constStream, scenario, state, firstTrue, lastTrue, false, event.policy_);
                for (int k = 0; k < nAff; ++k) {
                    const size_t idx = nodeStream[firstAff + k];
                    state.varStore1_[lvl][idx] = state.variables_[idx];
                    state.variables_[idx] = state.varStore0_[lvl][idx];
                }
                CaptureAndRestoreFuzzyVectors(&state.vectors_, state.vectorStore0_[lvl], &state.vectorStore1_[lvl], vectorFirst, vectorLast);
                if (state.lsmcFuzzySinks_)
                    state.lsmcFuzzySinks_->CaptureBranchPayment(lvl);
                EvalCompiledRange<Prepared_, Lsmc_>(nodeStream, constStream, scenario, state, lastTrue, lastFalse, false, event.policy_);
                for (int k = 0; k < nAff; ++k) {
                    const size_t idx = nodeStream[firstAff + k];
                    state.variables_[idx] = t * state.varStore1_[lvl][idx] + (1.0 - t) * state.variables_[idx];
                }
                BlendFuzzyVectors(&state.vectors_, state.vectorStore1_[lvl], t, vectorFirst, vectorLast);
                if (state.lsmcFuzzySinks_)
                    state.lsmcFuzzySinks_->BlendBranchPayment(lvl, t);
                --state.nestedIfLvl_;
                i = lastFalse;
            }
            return i;
        }

        template <bool Prepared_, bool Lsmc_, class T_, class P_>
        FORCE_INLINE size_t EvalCompiledFuzzyControl(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            if (event.nodeStream_[i] == FuzzyIf)
                return EvalCompiledFuzzyBranch<Prepared_, Lsmc_>(event, i, statePtr);
            return EvalCompiledFuzzyBoolean(event, i, statePtr);
        }

        template <class T_> FORCE_INLINE LsmcSinks_& RequireLsmcSinks(EvalState_<T_>* statePtr) {
            REQUIRE2(statePtr->lsmcSinks_, "UnsupportedExecutionMode: the LSMC recording stream requires the LSMC driver's sinks", ScriptError_);
            return *statePtr->lsmcSinks_;
        }

        template <class T_> FORCE_INLINE LsmcFuzzySinks_<T_>& RequireLsmcFuzzySinks(EvalState_<T_>* statePtr) {
            REQUIRE2(statePtr->lsmcFuzzySinks_, "UnsupportedExecutionMode: the LSMC fuzzy replay requires the LSMC driver's sinks", ScriptError_);
            return *statePtr->lsmcFuzzySinks_;
        }

        template <class T_> FORCE_INLINE void RecordLsmcPayment(EvalState_<T_>* statePtr, size_t index, double payment) {
            auto& sinks = RequireLsmcSinks(statePtr);
            if (sinks.pays_ && index == sinks.payoffIdx_)
                (*sinks.pays_)[(*sinks.eventToPays_)[sinks.eventOrdinal_]][sinks.pathSlot_] += payment;
        }

        //  Mirrors the tree-walk recorder: exercise leaves the script state untouched,
        //  only the driver's rows move. Fuzzy conditions land on the double stack, so
        //  the AAD replay extends this with its own decision-degree seam.
        template <class T_> FORCE_INLINE void RecordLsmcExercise(EvalState_<T_>* statePtr, double value, double cond, double spot) {
            REQUIRE2(std::isfinite(value), "InvalidPayoff: non-finite exercise value", ScriptError_);
            auto& sinks = RequireLsmcSinks(statePtr);
            const auto* plan = sinks.plan_;
            const size_t nFeatures = plan ? plan->RegressionFeatureCount() : 1;
            const size_t sample = plan ? plan->EventToSample()[sinks.eventOrdinal_] : 0;
            double firstFeature = spot;
            if (plan) {
                if (plan->RegressionFeatures().empty())
                    firstFeature = Value(plan->RegressionValue(sample, *statePtr->scenario_));
                else
                    firstFeature = Value(plan->RegressionFeatureValue(0, sample, *statePtr->scenario_, statePtr->variables_));
            }
            if (sinks.x_) {
                const size_t slot = (*sinks.eventToExercise_)[sinks.eventOrdinal_];
                (*sinks.x_)[slot * nFeatures][sinks.pathSlot_] = firstFeature;
                for (size_t feature = 1; feature < nFeatures; ++feature)
                    (*sinks.x_)[slot * nFeatures + feature][sinks.pathSlot_] =
                        Value(plan->RegressionFeatureValue(feature, sample, *statePtr->scenario_, statePtr->variables_));
                (*sinks.h_)[slot][sinks.pathSlot_] = value;
                if (sinks.cond_) {
                    auto& row = (*sinks.cond_)[slot];
                    if (!row.empty())
                        row[sinks.pathSlot_] = static_cast<char>(cond);
                }
            } else {
                sinks.pricingFeatures_[0] = firstFeature;
                for (size_t feature = 1; feature < nFeatures; ++feature)
                    sinks.pricingFeatures_[feature] =
                        Value(plan->RegressionFeatureValue(feature, sample, *statePtr->scenario_, statePtr->variables_));
                sinks.pricingH_ = value;
                sinks.pricingCond_ = cond != 0.0;
            }
        }

        //  Fuzzy (AAD) recording tier: the recorded rows stay live on the worker's tape
        template <class T_> FORCE_INLINE void RecordLsmcFuzzyPayment(EvalState_<T_>* statePtr, size_t index, const T_& payment) {
            auto& sinks = RequireLsmcFuzzySinks(statePtr);
            if (index == sinks.payoffIdx_)
                (*sinks.pays_)[sinks.eventOrdinal_] += payment;
        }

        template <class T_> FORCE_INLINE void RecordLsmcFuzzyExercise(EvalState_<T_>* statePtr, const T_& value, const T_& cond) {
            auto& sinks = RequireLsmcFuzzySinks(statePtr);
            const size_t slot = (*sinks.eventToExercise_)[sinks.eventOrdinal_];
            (*sinks.h_)[slot] = value;
            (*sinks.cond_)[slot] = cond;
            const auto* plan = sinks.plan_;
            const size_t sample = plan->EventToSample()[sinks.eventOrdinal_];
            if (plan->RegressionFeatures().empty()) {
                (*sinks.features_)[slot] = plan->RegressionValue(sample, *statePtr->scenario_);
            } else {
                const size_t nFeatures = plan->RegressionFeatureCount();
                for (size_t feature = 0; feature < nFeatures; ++feature)
                    (*sinks.features_)[slot * nFeatures + feature] =
                        plan->RegressionFeatureValue(feature, sample, *statePtr->scenario_, statePtr->variables_);
            }
        }

        //  LSMC recording tier of the prepared tail zone: payments and exercise
        //  triples land in the driver's sinks while the script-state arithmetic
        //  mirrors the plain Pays opcodes statement for statement. Compiled only
        //  into the Lsmc_ dispatch chain (see EvalCompiledPrepared): a reachable
        //  recording tail, called or not, perturbs the inlined LoadObservation
        //  path that every prepared product executes.
        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledLsmcOp(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            auto& state = *statePtr;
            auto& dStack = state.dStack_;
            auto& bStack = state.bStack_;
            auto& nodeStream = event.nodeStream_;
            auto& constStream = event.constStream_;
            const int op = event.nodeStream_[i];
            if (op == LsmcPays) {
                const size_t idx = nodeStream[++i];
                const T_ payment = dStack.TopAndPop();
                RecordLsmcPayment(statePtr, idx, Value(payment));
                state.variables_[idx] += payment / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcPaysConst) {
                const double val = constStream[nodeStream[++i]];
                const size_t idx = nodeStream[++i];
                RecordLsmcPayment(statePtr, idx, val);
                state.variables_[idx] += T_(val) / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcPaysOn) {
                const size_t slot = nodeStream[++i];
                const size_t idx = nodeStream[++i];
                const T_ payment = dStack.TopAndPop() * event.scenario_.discounts_[slot];
                RecordLsmcPayment(statePtr, idx, Value(payment));
                state.variables_[idx] += payment / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcPaysOnConst) {
                const double val = constStream[nodeStream[++i]];
                const size_t slot = nodeStream[++i];
                const size_t idx = nodeStream[++i];
                const T_ payment = T_(val) * event.scenario_.discounts_[slot];
                RecordLsmcPayment(statePtr, idx, Value(payment));
                state.variables_[idx] += payment / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcExercise) {
                const bool hasCond = nodeStream[++i] != 0;
                const T_ value = dStack.TopAndPop();
                double cond = 1.0;
                if (hasCond)
                    cond = bStack.TopAndPop() ? 1.0 : 0.0;
                RecordLsmcExercise(statePtr, Value(value), cond, Value(event.scenario_.spot_));
                return i + 1;
            }
            if (op == LsmcFuzzyPays) {
                const size_t idx = nodeStream[++i];
                const T_ payment = dStack.TopAndPop();
                RecordLsmcFuzzyPayment(statePtr, idx, payment);
                state.variables_[idx] += payment / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcFuzzyPaysConst) {
                const double val = constStream[nodeStream[++i]];
                const size_t idx = nodeStream[++i];
                RecordLsmcFuzzyPayment(statePtr, idx, T_(val));
                state.variables_[idx] += T_(val) / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcFuzzyPaysOn) {
                const size_t slot = nodeStream[++i];
                const size_t idx = nodeStream[++i];
                const T_ payment = dStack.TopAndPop() * event.scenario_.discounts_[slot];
                RecordLsmcFuzzyPayment(statePtr, idx, payment);
                state.variables_[idx] += payment / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcFuzzyPaysOnConst) {
                const double val = constStream[nodeStream[++i]];
                const size_t slot = nodeStream[++i];
                const size_t idx = nodeStream[++i];
                const T_ payment = T_(val) * event.scenario_.discounts_[slot];
                RecordLsmcFuzzyPayment(statePtr, idx, payment);
                state.variables_[idx] += payment / event.scenario_.numeraire_;
                return i + 1;
            }
            if (op == LsmcFuzzyExercise) {
                const bool hasCond = nodeStream[++i] != 0;
                T_ cond(1.0);
                if (hasCond)
                    cond = dStack.TopAndPop();
                const T_ value = dStack.TopAndPop();
                RecordLsmcFuzzyExercise(statePtr, value, cond);
                return i + 1;
            }
            ThrowUnknownCompiledOpcode(op);
        }

        //  Lsmc_ instantiates the recording tier for the LSMC driver's streams; the
        //  default compiles to exactly the LoadObservation/Discard/throw shape that
        //  predates the recording opcodes (hot path of every prepared product)
        template <bool Lsmc_ = false, class T_, class P_>
        FORCE_INLINE size_t EvalCompiledPrepared(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            const int op = event.nodeStream_[i];
            if (op == LoadObservation) {
                statePtr->dStack_.Push(event.policy_.Read(event.nodeStream_[i + 1], *statePtr));
                return i + 2;
            }
            if (op == Discard) {
                statePtr->dStack_.Pop();
                return i + 1;
            }
            if constexpr (Lsmc_)
                return EvalCompiledLsmcOp(event, i, statePtr);
            ThrowUnknownCompiledOpcode(op);
        }

        inline String_ CompiledVectorContext(const Vector_<int>& stream, size_t offset, size_t index) {
            String_ value = "vector index=" + String_(std::to_string(index)) + "; line=" + String_(std::to_string(stream[offset])) +
                            ", column=" + String_(std::to_string(stream[offset + 1]));
            if (stream[offset + 2] > 0)
                value += ", row=" + String_(std::to_string(stream[offset + 2]));
            return value;
        }

        template <class T_, class P_>
        FORCE_INLINE size_t
        EvalCompiledVectorReduction(const Vector_<int>& stream, size_t i, size_t index, EvalState_<T_>* statePtr, const P_& policy) {
            const auto kind = static_cast<NodeVectorReduce_::Kind_>(stream[i++]);
            const auto& values = statePtr->vectors_[index];
            if constexpr (P_::trace_)
                policy.VectorReduction(i - 3, kind, values);
            const String_ context = values.empty() && kind != NodeVectorReduce_::Kind_::Sum ? CompiledVectorContext(stream, i, index) : String_();
            statePtr->dStack_.Push(ReduceVectorValues(values, kind, context));
            return i + 3;
        }

        template <class T_, class P_>
        FORCE_INLINE size_t EvalCompiledVector(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            const auto& stream = event.nodeStream_;
            const int op = stream[i++];
            const size_t index = static_cast<size_t>(stream[i++]);
            auto& values = statePtr->vectors_[index];
            if (op == VectorRead) {
                const size_t entry = static_cast<size_t>(stream[i++]);
                if (entry >= values.size())
                    THROW2("VectorIndexOutOfRange: " + CompiledVectorContext(stream, i, index), ScriptError_);
                statePtr->dStack_.Push(values[entry]);
                return i + 3;
            }
            if (op == VectorAssign) {
                const size_t entry = static_cast<size_t>(stream[i++]);
                WriteVectorEntry(&values, entry, statePtr->dStack_.TopAndPop());
                return i;
            }
            if (op == VectorAppend) {
                values.push_back(statePtr->dStack_.TopAndPop());
                return i;
            }
            if (op == VectorReduce)
                return EvalCompiledVectorReduction(stream, i, index, statePtr, event.policy_);
            ThrowUnknownCompiledOpcode(op);
        }

        template <bool Prepared_, bool Lsmc_, class T_, class P_>
        FORCE_INLINE size_t EvalCompiledInstruction(const CompiledEventView_<T_, P_>& event, size_t i, EvalState_<T_>* statePtr) {
            const int op = event.nodeStream_[i];
            if (op <= Min2Const)
                return EvalCompiledArithmetic(event, i, statePtr);
            if (op <= PaysOnConst)
                return EvalCompiledData(event, i, statePtr);
            if (op <= Or)
                return EvalCompiledControl<Prepared_, Lsmc_>(event, i, statePtr);
            if (op <= ConstVar)
                return EvalCompiledScalar(event, i, statePtr);
            if (op <= FuzzyCompDiscrete)
                return EvalCompiledFuzzyComparison(event, i, statePtr);
            // Keep vector dispatch off the hot path for scalar-only scripts.
            if (op >= VectorRead && op <= VectorReduce)
                return EvalCompiledVector(event, i, statePtr);
            if constexpr (Prepared_) {
                if (op > FuzzyIf)
                    return EvalCompiledPrepared<Lsmc_>(event, i, statePtr);
            }
            return EvalCompiledFuzzyControl<Prepared_, Lsmc_>(event, i, statePtr);
        }

        template <bool Prepared_ = true, bool Lsmc_ = false, class T_, class E_>
        void EvalCompiledEvents(size_t eventCount, const E_& eventAt, EvalState_<T_>* statePtr) {
            for (size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
                const auto event = eventAt(eventIndex);
                const size_t n = event.last_ ? event.last_ : event.nodeStream_.size();
                if (event.reset_) {
                    statePtr->dStack_.Reset();
                    statePtr->bStack_.Reset();
                }
                size_t i = event.first_;
                while (i < n)
                    i = EvalCompiledInstruction<Prepared_, Lsmc_>(event, i, statePtr);
            }
        }

        template <bool Prepared_, bool Lsmc_, class T_, class P_>
        inline void EvalCompiledRange(const Vector_<int>& nodeStream,
                                      const Vector_<double>& constStream,
                                      const AAD::Sample_<T_>& scenario,
                                      EvalState_<T_>& state,
                                      size_t first,
                                      size_t last,
                                      bool reset,
                                      const P_& policy) {
            EvalCompiledEvents<Prepared_, Lsmc_>(
                1, [&](size_t) { return CompiledEventView_<T_, P_>{nodeStream, constStream, scenario, first, last, reset, policy}; }, &state);
        }
    } // namespace Detail

    template <class T_>
    inline void EvalCompiled(const Vector_<int>& nodeStream,
                             const Vector_<double>& constStream,
                             const AAD::Sample_<T_>& scenario,
                             EvalState_<T_>& state,
                             size_t first,
                             size_t last,
                             bool reset) {
        Detail::EvalCompiledRange<true>(nodeStream, constStream, scenario, state, first, last, reset);
    }
} // namespace Dal::Script
