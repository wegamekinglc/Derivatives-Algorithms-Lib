//
// Created by wegam on 2022/5/2.
//

#pragma once

#include <dal/storage/archive.hpp>
#include <dal/math/interp/interp.hpp>

namespace Dal {
    class BASE_EXPORT Interp2_: public Storable_ {
    public:
        explicit Interp2_(const String_& name);
        virtual double operator()(double x, double y) const = 0;
        virtual bool IsInBounds(double x, double y) const { return true; }
    };

    class BASE_EXPORT Interp2Linear_ : public Interp2_ {
        Vector_<> x_;
        Vector_<> y_;
        Matrix_<> f_;
        Vector_<Handle_<Interp1_>> interps_;
        size_t n_;
        size_t m_;

    public:
        Interp2Linear_(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f);
        void Write(Archive::Store_& dst) const override;
        double operator()(double x, double y) const override;
        const Vector_<>& x() const { return x_; }
        const Vector_<>& y() const { return y_; }
    };

    namespace Interp {
        Interp2_* NewLinear2(const String_& name, const Vector_<>& x, const Vector_<>& y, const Matrix_<>& f);
    }
}
