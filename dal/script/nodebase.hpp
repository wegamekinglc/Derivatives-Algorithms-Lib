//
// Created by wegam on 2023/1/27.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/script/visitorlist.hpp>

namespace Dal::Script {
    struct Node;
    using ExprTree = std::unique_ptr<Node>;
    using Expression = ExprTree;
    using Statement = ExprTree;
    using Event_ = Vector_<Statement>;

    //  Visitable Base

        template <typename V, bool CONST> struct BaseImpl;

    template <typename V> struct BaseImpl<V, false> {
        virtual void accept(V& visitor) = 0;
    };

    template <typename V> struct BaseImpl<V, true> {
        virtual void accept(V& visitor) const = 0;
    };

    template <typename... Vs> struct VisitableBase;

    template <typename V> struct VisitableBase<V> : BaseImpl<V, isVisitorConst<V>()> {
        using BaseImpl<V, isVisitorConst<V>()>::accept;
    };

    template <typename V, typename... Vs> struct VisitableBase<V, Vs...> : VisitableBase<V>, VisitableBase<Vs...> {
        using VisitableBase<V>::accept;
        using VisitableBase<Vs...>::accept;
    };

    //  Visitable

    template <typename V, typename Base, typename Concrete, bool CONST, typename... Vs> struct DerImpl;

    template <typename V, typename Base, typename Concrete> struct DerImpl<V, Base, Concrete, false> : Base {
        void accept(V& visitor) override { visitor.Visit(static_cast<Concrete&>(*this)); }
    };

    template <typename V, typename Base, typename Concrete> struct DerImpl<V, Base, Concrete, true> : Base {
        void accept(V& visitor) const override { visitor.Visit(static_cast<const Concrete&>(*this)); }
    };

    template <typename Base, typename Concrete, typename... Vs> struct Visitable;

    template <typename Base, typename Concrete, typename V>
    struct Visitable<Base, Concrete, V> : DerImpl<V, Base, Concrete, isVisitorConst<V>()> {
        using DerImpl<V, Base, Concrete, isVisitorConst<V>()>::accept;
    };

    template <typename V, typename Base, typename Concrete, typename... Vs>
    struct DerImpl<V, Base, Concrete, false, Vs...> : Visitable<Base, Concrete, Vs...> {
        using Visitable<Base, Concrete, Vs...>::accept;

        void accept(V& visitor) override { visitor.Visit(static_cast<Concrete&>(*this)); }
    };

    template <typename V, typename Base, typename Concrete, typename... Vs>
    struct DerImpl<V, Base, Concrete, true, Vs...> : Visitable<Base, Concrete, Vs...> {
        using Visitable<Base, Concrete, Vs...>::accept;

        void accept(V& visitor) const override { visitor.Visit(static_cast<const Concrete&>(*this)); }
    };

    template <typename Base, typename Concrete, typename V, typename... Vs>
    struct Visitable<Base, Concrete, V, Vs...> : DerImpl<V, Base, Concrete, isVisitorConst<V>(), Vs...> {
        using DerImpl<V, Base, Concrete, isVisitorConst<V>(), Vs...>::accept;
    };

    struct Node : public VisitableBase<VISITORS> {
        using VisitableBase<VISITORS>::accept;
        Vector_<ExprTree> arguments;
        virtual ~Node() {}
    };
}
