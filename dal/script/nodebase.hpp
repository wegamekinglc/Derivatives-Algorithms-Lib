//
// Created by wegam on 2023/1/27.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/script/visitorlist.hpp>

namespace Dal::Script {
    struct Node_;
    using ExprTree_ = std::unique_ptr<Node_>;
    using Expression_ = ExprTree_;
    using Statement_ = ExprTree_;
    using Event_ = Vector_<Statement_>;

    //  Visitable_ Base

    template <typename V_, bool CONST> struct BaseImpl_;

    template <typename V_> struct BaseImpl_<V_, false> {
        virtual void Accept(V_& visitor) = 0;
    };

    template <typename V_> struct BaseImpl_<V_, true> {
        virtual void Accept(V_& visitor) const = 0;
    };

    template <typename... Vs_> struct VisitableBase_;

    template <typename V_> struct VisitableBase_<V_> : public BaseImpl_<V_, IsVisitorConst<V_>()> {
        using BaseImpl_<V_, IsVisitorConst<V_>()>::Accept;
    };

    template <typename V_, typename... Vs_> struct VisitableBase_<V_, Vs_...> : public VisitableBase_<V_>, VisitableBase_<Vs_...> {
        using VisitableBase_<V_>::Accept;
        using VisitableBase_<Vs_...>::Accept;
    };

    //  Visitable_

    template <typename V_, typename Base_, typename Concrete_, bool CONST, typename... Vs_> struct DerImpl_;

    template <typename V_, typename Base_, typename Concrete_> struct DerImpl_<V_, Base_, Concrete_, false> : public Base_ {
        void Accept(V_& visitor) override { visitor.Visit(static_cast<Concrete_&>(*this)); }
    };

    template <typename V_, typename Base_, typename Concrete_> struct DerImpl_<V_, Base_, Concrete_, true> : public Base_ {
        void Accept(V_& visitor) const override { visitor.Visit(static_cast<const Concrete_&>(*this)); }
    };

    template <typename Base_, typename Concrete_, typename... Vs_> struct Visitable_;

    template <typename Base_, typename Concrete_, typename V_>
    struct Visitable_<Base_, Concrete_, V_> : public DerImpl_<V_, Base_, Concrete_, IsVisitorConst<V_>()> {
        using DerImpl_<V_, Base_, Concrete_, IsVisitorConst<V_>()>::Accept;
    };

    template <typename V_, typename Base_, typename Concrete_, typename... Vs_>
    struct DerImpl_<V_, Base_, Concrete_, false, Vs_...> : public Visitable_<Base_, Concrete_, Vs_...> {
        using Visitable_<Base_, Concrete_, Vs_...>::Accept;

        void Accept(V_& visitor) override { visitor.Visit(static_cast<Concrete_&>(*this)); }
    };

    template <typename V_, typename Base_, typename Concrete_, typename... Vs_>
    struct DerImpl_<V_, Base_, Concrete_, true, Vs_...> : public Visitable_<Base_, Concrete_, Vs_...> {
        using Visitable_<Base_, Concrete_, Vs_...>::Accept;

        void Accept(V_& visitor) const override { visitor.Visit(static_cast<const Concrete_&>(*this)); }
    };

    template <typename Base_, typename Concrete_, typename V_, typename... Vs_>
    struct Visitable_<Base_, Concrete_, V_, Vs_...> : public DerImpl_<V_, Base_, Concrete_, IsVisitorConst<V_>(), Vs_...> {
        using DerImpl_<V_, Base_, Concrete_, IsVisitorConst<V_>(), Vs_...>::Accept;
    };

    struct Node_ : public VisitableBase_<VISITORS> {
        using VisitableBase_<VISITORS>::Accept;
        Vector_<ExprTree_> arguments_;
        virtual ~Node_() {}
    };
}
