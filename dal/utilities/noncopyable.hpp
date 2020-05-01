//
// Created by wegam on 2019/11/4.
//

#pragma once

namespace Dal {
    class noncopyable {
    protected:
        noncopyable() = default;
        ~noncopyable() = default;

    public:
        noncopyable( const noncopyable& ) = delete;
        noncopyable& operator=( const noncopyable& ) = delete;
    };
}
