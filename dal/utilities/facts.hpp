//
// Created by wegam on 2022/2/2.
// abstract tools supporting in-process storage of semi-permanent facts about the world
// e.g., currency defaults
//

#pragma once
#include <dal/utilities/noncopyable.hpp>

namespace Dal {
    class String_;

    struct FactBase_ : noncopyable {
        virtual ~FactBase_() = default;
    };

    template <class K_, class V_> class OneFact_ : public FactBase_ {
    public:
        virtual const V_& operator()(const K_& key) const = 0;

        class Writer_ : noncopyable {
        public:
            virtual void SetDefault(const V_& val) = 0;
            virtual void operator()(const K_& key, const V_& val) = 0;
        };
        virtual Writer_& XWrite() const = 0;
    };
}