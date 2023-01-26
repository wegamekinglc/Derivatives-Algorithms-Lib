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
    template <class T> class Evaluator_;
    class Compiler_;
    class ConstCondProcessor_;
    class IFProcessor_;
    class DomainProcessor_;
    template <class T> class FuzzyEvaluator_;

//  List

//  Modifying visitors
#define MVISITORS VarIndexer_, ConstProcessor_, ConstCondProcessor_, IFProcessor_, DomainProcessor_

//  Const visitors
#define CVISITORS Debugger_, Evaluator_<double>, Evaluator_<AAD::Number_>, Compiler_, FuzzyEvaluator_<double>, FuzzyEvaluator_<AAD::Number_>

//  All visitors
#define VISITORS MVISITORS , CVISITORS

    //  Various meta-programming utilities

    //  Is V a const visitor?

    template <class V>
    inline constexpr bool isVisitorConst()
    {
        return Pack<CVISITORS>::includes<V>();
    }

    //  Use : isVisitorConst<V>() returns true if V is const, or false
    //  isVisitorConst() resolves at compile time

    //  Does V have a Visit for a const N? A non-const N?

    template <typename V>
    struct hasNonConstVisit
    {
        template <typename N, void (V::*) (N&) = &V::Visit>
        static bool constexpr forNodeType()
        {
            return true;
        }

        template <typename N>
        static bool constexpr forNodeType(...)
        {
            return false;
        }
    };

    template <typename V>
    struct hasConstVisit
    {
        template <typename N, void (V::*) (const N&) = &V::Visit>
        static bool constexpr forNodeType()
        {
            return true;
        }

        template <typename N>
        static bool constexpr forNodeType(...)
        {
            return false;
        }
    };

    //  Use: hasConstVisit<V>::forNodeType<N>() returns true
    //      if V declares a method void Visit(const N&)
    //      false otherwise
    //  Everything resolves at compile time
    //  hasNonConstVisit is the same: hasNonConstVisit<V>::forNodeType<N>()
    //      returns true if V declares void Visit(N&)

}