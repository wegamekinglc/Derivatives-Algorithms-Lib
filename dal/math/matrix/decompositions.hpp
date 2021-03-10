//
// Created by wegam on 2021/2/19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/noncopyable.hpp>

namespace Dal {
    class SquareMatrixDecomposition_ : noncopyable {
        virtual void XMultiplyLeft_af(const Vector_<>& x, Vector_<>* b) const = 0;
        virtual void XMultiplyRight_af(const Vector_<>& x, Vector_<>* b) const = 0;
        virtual void XSolveLeft_af(const Vector_<>& b, Vector_<>* x) const = 0;
        virtual void XSolveRight_af(const Vector_<>& b, Vector_<>* x) const = 0;

    public:
        virtual ~SquareMatrixDecomposition_() = default;
        virtual int Size() const = 0;
        void MultiplyLeft(const Vector_<>& x, Vector_<>* b) const;
        void MultiplyRight(const Vector_<>& x, Vector_<>* b) const;
        void SolveLeft(const Vector_<>& b, Vector_<>* x) const;
        void SolveRight(const Vector_<>& b, Vector_<>* x) const;
    };

    // special case of symmetric matrix

    class SymmetricMatrixDecomposition_ : public SquareMatrixDecomposition_ {
        virtual void XMultiply_af(const Vector_<>& x, Vector_<>* b) const = 0;

        void XMultiplyLeft_af(const Vector_<>& x, Vector_<>* b) const override {
            return XMultiply_af(x, b);
        }

        void XMultiplyRight_af(const Vector_<>& x, Vector_<>* b) const override {
            return XMultiply_af(x, b);
        }

        virtual void XSolve_af(const Vector_<>& b, Vector_<>* x) const = 0;

        void XSolveLeft_af(const Vector_<>& b, Vector_<>* x) const override {
            return XSolve_af(b, x);
        }

        void XSolveRight_af(const Vector_<>& b, Vector_<>* x) const override {
            return XSolve_af(b, x);
        }

    public:
        virtual int Rank() const { return Size(); }
        virtual Vector_<>::const_iterator MakeCorrelated(Vector_<>::const_iterator iid_begin,
                                                         Vector_<>* correlated) const = 0;

        void Multiply(const Vector_<>& x, Vector_<>* b) const;
        void Solve(const Vector_<>& b, Vector_<>* x) const;
    };

    class ExponentiatesMatrix_ {
    public:
        virtual void ExpAT(double t, SquareMatrix_<>* dst) const = 0;
    };
}
