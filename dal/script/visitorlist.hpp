//
// Created by wegam on 2023/1/26.
//

#pragma once

#include <dal/math/aad/aad.hpp>
#include <dal/script/packincludes.hpp>

namespace Dal::Script {
    //  Declaration of all visitors
    class Debugger_;
    class VarIndexer_;
    class ConstProcessor_;
    template <class T_> class Evaluator_;
    template <class T_> class PastEvaluator_;
    class Compiler_;
    class ConstCondProcessor_;
    class IFProcessor_;
    class DomainProcessor_;
    template <class T> class FuzzyEvaluator_;

//  List

//  Modifying visitors
#define MODIFY_VISITORS VarIndexer_, ConstProcessor_, ConstCondProcessor_, IFProcessor_, DomainProcessor_

//  Const visitors
#define CONST_VISITORS                                                                                                 \
    Debugger_, Evaluator_<double>, Evaluator_<AAD::Number_>, PastEvaluator_<double>, Compiler_, FuzzyEvaluator_<double>,                       \
        FuzzyEvaluator_<AAD::Number_>

//  All visitors
#define VISITORS MODIFY_VISITORS, CONST_VISITORS

    //  Various meta-programming utilities

    //  Is V_ a const visitor?

    template <class V_> inline constexpr bool IsVisitorConst() { return Pack_<CONST_VISITORS>::Includes<V_>(); }

    //  Use : IsVisitorConst<V_>() returns true if V_ is const, or false
    //  IsVisitorConst() resolves at compile time

    //  Does V_ have a Visit for a const N_? A non-const N_?

    template <typename V_> struct HasNonConstVisit_ {
        template <typename N_, void (V_::*)(N_&) = &V_::Visit> static bool constexpr ForNodeType() { return true; }

        template <typename N_> static bool constexpr ForNodeType(...) { return false; }
    };

    template <typename V_> struct HasConstVisit_ {
        template <typename N_, void (V_::*)(const N_&) = &V_::Visit> static bool constexpr ForNodeType() {
            return true;
        }

        template <typename N_> static bool constexpr ForNodeType(...) { return false; }
    };

    //  Use: HasConstVisit_<V_>::ForNodeType<N_>() returns true
    //      if V_ declares a method void Visit(const N_&)
    //      false otherwise
    //  Everything resolves at compile time
    //  HasNonConstVisit_ is the same: HasNonConstVisit_<V_>::ForNodeType<N_>()
    //      returns true if V_ declares void Visit(N_&)

} // namespace Dal::Script