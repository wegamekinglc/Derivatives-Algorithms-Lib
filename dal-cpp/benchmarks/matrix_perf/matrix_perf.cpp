//
// Created by wegam on 24-3-24.
//

#include <dal/platform/platform.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/timer.hpp>

class Matrix_ {
    int rows_ = 0;
    int cols_ = 0;
    std::vector<double> vector_;

public:
    using value_type = double;
    Matrix_() = default;
    Matrix_(const int rows, const int cols, const std::unique_ptr<Dal::Random_>& random): rows_(rows), cols_(cols), vector_(rows * cols) {
        if(random) {
            Dal::Vector_<double> temp(cols_);
            for(int i = 0; i < rows_; ++i) {
                random->FillNormal(&temp);
                for(int j = 0; j < cols_; ++j)
                    vector_[i * cols + j] = temp[j];
            }
        }
    }

    [[nodiscard]] int Rows() const { return rows_; }
    [[nodiscard]] int Cols() const { return cols_; }

    FORCE_INLINE double* operator[](int row) { return &vector_[row * cols_]; }
    FORCE_INLINE const double* operator[](int row) const { return &vector_[row * cols_]; }
    [[nodiscard]] double Sum() const {
        return Dal::Accumulate(vector_, [](double x, double y) { return x + y;});
    }

    void Clear() {
        Dal::Fill(&vector_, 0.0);
    }
};

void MultiV1(const Matrix_& a, const Matrix_& b, Matrix_& c) {
    const int rows = a.Rows();
    const int cols = b.Cols();
    const int n = a.Cols();

    for(int i = 0; i < rows; ++i) {
        const auto ai = a[i];
        auto ci = c[i];

        for(int j = 0; j < cols; ++j) {
            double res = 0.0;
            for(int k = 0; k < n; ++k)
                res += ai[k] * b[k][j];
            ci[j] = res;
        }
    }
}

void MultiV2(const Matrix_& a, const Matrix_& b, Matrix_& c) {
    const int rows = a.Rows();
    const int cols = b.Cols();
    const int n = a.Cols();

    for(int i = 0; i < rows; ++i) {
        const auto ai = a[i];
        auto ci = c[i];

        for(int j = 0; j < cols; ++j) {
            const double* bkj = &b[0][j];
            double res = 0.0;
            for(int k = 0; k < n; ++k) {
                res += ai[k] * *bkj;
                bkj += rows;
            }
            ci[j] = res;
        }
    }
}

void MultiV3(const Matrix_& a, const Matrix_& b, Matrix_& c) {
    const int rows = a.Rows();
    const int cols = b.Cols();
    const int n = a.Cols();

    for(int i = 0; i < rows; ++i) {
        const auto ai = a[i];
        auto ci = c[i];

        for(int k = 0; k < n; ++k) {
            const auto bk = b[k];
            const auto aik = ai[k];

            #pragma loop(no_vector)
            for(int j = 0; j < cols; ++j)
                ci[j] += aik * bk[j];
        }
    }
}


void MultiV4(const Matrix_& a, const Matrix_& b, Matrix_& c) {
    const int rows = a.Rows();
    const int cols = b.Cols();
    const int n = a.Cols();

    for(int i = 0; i < rows; ++i) {
        const auto ai = a[i];
        auto ci = c[i];

        for(int k = 0; k < n; ++k) {
            const auto bk = b[k];
            const auto aik = ai[k];

            for(int j = 0; j < cols; ++j)
                ci[j] += aik * bk[j];
        }
    }
}

void MultiV5(const Matrix_& a, const Matrix_& b, Matrix_& c) {
    const int rows = a.Rows();
    const int cols = b.Cols();
    const int n = a.Cols();

    #pragma omp parallel for
    for(int i = 0; i < rows; ++i) {
        const auto ai = a[i];
        auto ci = c[i];

        for(int k = 0; k < n; ++k) {
            const auto bk = b[k];
            const auto aik = ai[k];

            for(int j = 0; j < cols; ++j)
                ci[j] += aik * bk[j];
        }
    }
}

int main() {
    const int n = 1000;
    std::unique_ptr<Dal::Random_> random(Dal::NewSobol(n, 1000));
    Matrix_ a(n, n, random);
    Matrix_ b(n, n, random);
    Matrix_ c(n, n, nullptr);

    Dal::Vector_<int> widths = {20, 20, 20};

    std::cout << std::setw(widths[0]) << std::right << "Model"
              << std::setw(widths[1]) << std::right << "Duration"
              << std::setw(widths[2]) << std::right << "Total Sum." << std::endl;

    Dal::Timer_ timer;

    c.Clear();
    timer.Reset();
    MultiV1(a, b, c);
    std::cout << std::setw(widths[0]) << std::right << "Multi. V1"
              << std::fixed
              << std::setprecision(4)
              << std::setw(widths[1]) << std::right << int(timer.Elapsed<Dal::milliseconds>())
              << std::setw(widths[2]) << std::right << c.Sum() << std::endl;

    c.Clear();
    timer.Reset();
    MultiV2(a, b, c);
    std::cout << std::setw(widths[0]) << std::right << "Multi. V2"
              << std::fixed
              << std::setprecision(4)
              << std::setw(widths[1]) << std::right << int(timer.Elapsed<Dal::milliseconds>())
              << std::setw(widths[2]) << std::right << c.Sum() << std::endl;

    c.Clear();
    timer.Reset();
    MultiV3(a, b, c);
    std::cout << std::setw(widths[0]) << std::right << "Multi. V3"
              << std::fixed
              << std::setprecision(4)
              << std::setw(widths[1]) << std::right << int(timer.Elapsed<Dal::milliseconds>())
              << std::setw(widths[2]) << std::right << c.Sum() << std::endl;

    c.Clear();
    timer.Reset();
    MultiV4(a, b, c);
    std::cout << std::setw(widths[0]) << std::right << "Multi. V4"
              << std::fixed
              << std::setprecision(4)
              << std::setw(widths[1]) << std::right << int(timer.Elapsed<Dal::milliseconds>())
              << std::setw(widths[2]) << std::right << c.Sum() << std::endl;

    c.Clear();
    timer.Reset();
    MultiV5(a, b, c);
    std::cout << std::setw(widths[0]) << std::right << "Multi. V5"
              << std::fixed
              << std::setprecision(4)
              << std::setw(widths[1]) << std::right << int(timer.Elapsed<Dal::milliseconds>())
              << std::setw(widths[2]) << std::right << c.Sum() << std::endl;

    return 0;
}