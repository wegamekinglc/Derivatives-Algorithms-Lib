//
// Created by Cheng Li on 2018/8/14.
//

#pragma once

namespace Dal::Matrix {
    Vector_<> Vols(const Matrix_<>& cov, Matrix_<>* corr = nullptr);
    void Multiply(const Matrix_<>& left, const Matrix_<>& right, Matrix_<>* result);
    void Multiply(const Matrix_<>& left, const Vector_<>& right, Vector_<>* result);
    void Multiply(const Vector_<>& left, const Matrix_<>& right, Vector_<>* result);
    double WeightedInnerProduct(const Vector_<>& left, const Matrix_<double>& w, const Vector_<>& right);
    void AddJSquaredToUpper(const Matrix_<>& j, Matrix_<>* h);
} // namespace Dal