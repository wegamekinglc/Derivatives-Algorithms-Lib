//
// Created by wegamekinglc on 22-12-17.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal {

    class HasPreConditioner_ {
    public:
        virtual void PreConditionerSolveLeft(const Vector_<>& b, Vector_<>* x) const = 0;
        virtual void PreConditionerSolveRight(const Vector_<>& b, Vector_<>* x) const = 0;
    };

    namespace Sparse {
        class Square_;

        void CGSolve(const Sparse::Square_& A,
                     const Vector_<>& b, double tol_rel, double tol_abs, int max_iterations, Vector_<>* x);

        void BCGSolve(const Sparse::Square_& A,
                      const Vector_<>& b, double tol_rel, double tol_abs, int max_iterations, Vector_<>* x);
    }
}