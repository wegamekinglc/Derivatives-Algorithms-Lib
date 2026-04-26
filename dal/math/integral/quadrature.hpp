//
// Created by wegam on 2026/4/26.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/functionals.hpp>

namespace Dal {
    namespace Quadrature {
        template <class T_> inline void Increment(T_* dst, const T_& inc, double w) {
            T_ z(inc);
            z *= w;
            *dst += z;
        }

        template <> inline void Increment(Vector_<>* dst, const Vector_<>& inc, double w) {
            Transform(dst, inc, LinearIncrement(w));
        }

        void NCDFGaussHermiteWeights(Vector_<>* x, Vector_<>* w);
        void SimpsonWeights(int n, double lo, double hi, Vector_<>* x, Vector_<>* w);
    } // namespace Quadrature

    class Quad1DBase_ {
    public:
        virtual ~Quad1DBase_();
    };

    template <class T_>
    class Quad1D_ : public Quad1DBase_ {
    public:
        virtual double GetX() = 0;
        virtual void PutY(const T_& y) = 0;
        [[nodiscard]] virtual bool IsComplete() const = 0;
        virtual T_ Result() const = 0;
        virtual void Restart() = 0;
    };

    template <class T_>
    class Quad1DFixed_ : public Quad1D_<T_> {
        size_t i_;
        T_ sum_, initial_;

    protected:
        Vector_<> x_, w_;
        Quad1DFixed_(int size, const T_& initial) : i_(0), sum_(initial), initial_(initial), x_(size), w_(size) {}

    public:
        double GetX() override {
            ASSERT(!IsComplete(), "quadrature is complete");
            return x_[i_];
        }
        void PutY(const T_& y) override {
            ASSERT(!IsComplete(), "quadrature is complete");
            Quadrature::Increment(&sum_, y, w_[i_++]);
        }
        [[nodiscard]] bool IsComplete() const override { return i_ == x_.size(); }
        T_ Result() const override {
            ASSERT(IsComplete(), "quadrature is incomplete");
            return sum_;
        }
        void Restart() override {
            i_ = 0;
            sum_ = initial_;
        }
        [[nodiscard]] const Vector_<>& Abscissa() const { return x_; }
        [[nodiscard]] const Vector_<>& Weight() const { return w_; }
    };

    template <class T_ = double>
    class NormalExpectation_ : public Quad1DFixed_<T_> {
    public:
        explicit NormalExpectation_(int n, const T_& initial = 0.0) : Quad1DFixed_<T_>(n, initial) {
            Quadrature::NCDFGaussHermiteWeights(&this->x_, &this->w_);
        }
    };

    template <class T_ = double>
    class QuadSimpson_ : public Quad1DFixed_<T_> {
    public:
        QuadSimpson_(int n, double lo, double hi, const T_& initial = 0.0) : Quad1DFixed_<T_>(n | 1, initial) {
            Quadrature::SimpsonWeights(n, lo, hi, &this->x_, &this->w_);
        }
    };
} // namespace Dal
