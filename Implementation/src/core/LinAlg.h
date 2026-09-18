//
//  LinAlg.h - minimal dense linear algebra for small problems.
//
//  Everything here works on small, dense systems (n <= a few dozen), which is all
//  the duality examples need. Kept header-only and dependency-free so the numerical
//  core can also be compiled into a console test without linking the GUI.
//
#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <algorithm>

namespace la
{

using Vec = std::vector<double>;

class Mat
{
    size_t _rows = 0;
    size_t _cols = 0;
    Vec _a;
public:
    Mat() = default;
    Mat(size_t rows, size_t cols, double init = 0.0)
    : _rows(rows), _cols(cols), _a(rows * cols, init)
    {}

    size_t rows() const { return _rows; }
    size_t cols() const { return _cols; }

    void resize(size_t rows, size_t cols, double init = 0.0)
    {
        _rows = rows; _cols = cols;
        _a.assign(rows * cols, init);
    }

    double& operator()(size_t i, size_t j)             { return _a[i * _cols + j]; }
    double  operator()(size_t i, size_t j) const       { return _a[i * _cols + j]; }

    const Vec& data() const { return _a; }
};

//y = M*x
inline Vec mul(const Mat& M, const Vec& x)
{
    Vec y(M.rows(), 0.0);
    for (size_t i = 0; i < M.rows(); ++i)
    {
        double s = 0.0;
        for (size_t j = 0; j < M.cols(); ++j)
            s += M(i, j) * x[j];
        y[i] = s;
    }
    return y;
}

//y = M^T * x
inline Vec mulT(const Mat& M, const Vec& x)
{
    Vec y(M.cols(), 0.0);
    for (size_t i = 0; i < M.rows(); ++i)
    {
        const double xi = x[i];
        if (xi == 0.0)
            continue;
        for (size_t j = 0; j < M.cols(); ++j)
            y[j] += M(i, j) * xi;
    }
    return y;
}

inline double dot(const Vec& a, const Vec& b)
{
    double s = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        s += a[i] * b[i];
    return s;
}

inline double norm2(const Vec& a) { return std::sqrt(dot(a, a)); }

inline double normInf(const Vec& a)
{
    double m = 0.0;
    for (double v : a)
        m = std::max(m, std::fabs(v));
    return m;
}

//x = a + s*b
inline Vec axpy(const Vec& a, double s, const Vec& b)
{
    Vec r(a.size());
    for (size_t i = 0; i < a.size(); ++i)
        r[i] = a[i] + s * b[i];
    return r;
}

//Solves A*x = b by Gaussian elimination with partial pivoting.
//A and b are passed by value (destroyed). Returns false on a singular system.
inline bool solveLU(Mat A, Vec b, Vec& x)
{
    const size_t n = A.rows();
    if (n == 0 || A.cols() != n || b.size() != n)
        return false;

    std::vector<size_t> piv(n);
    for (size_t i = 0; i < n; ++i)
        piv[i] = i;

    for (size_t k = 0; k < n; ++k)
    {
        //find pivot
        size_t pr = k;
        double best = std::fabs(A(k, k));
        for (size_t i = k + 1; i < n; ++i)
        {
            const double v = std::fabs(A(i, k));
            if (v > best) { best = v; pr = i; }
        }
        if (best < 1e-14)
            return false;

        if (pr != k)
        {
            for (size_t j = 0; j < n; ++j)
                std::swap(A(k, j), A(pr, j));
            std::swap(b[k], b[pr]);
        }

        const double akk = A(k, k);
        for (size_t i = k + 1; i < n; ++i)
        {
            const double f = A(i, k) / akk;
            if (f == 0.0)
                continue;
            A(i, k) = 0.0;
            for (size_t j = k + 1; j < n; ++j)
                A(i, j) -= f * A(k, j);
            b[i] -= f * b[k];
        }
    }

    x.assign(n, 0.0);
    for (size_t ii = n; ii-- > 0; )
    {
        double s = b[ii];
        for (size_t j = ii + 1; j < n; ++j)
            s -= A(ii, j) * x[j];
        x[ii] = s / A(ii, ii);
    }
    return true;
}

//Cholesky factorisation A = L*L^T for symmetric positive definite A.
//Returns false if A is not numerically positive definite.
inline bool cholesky(const Mat& A, Mat& L)
{
    const size_t n = A.rows();
    if (n == 0 || A.cols() != n)
        return false;

    L.resize(n, n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = 0; j <= i; ++j)
        {
            double s = A(i, j);
            for (size_t k = 0; k < j; ++k)
                s -= L(i, k) * L(j, k);

            if (i == j)
            {
                if (s <= 1e-14)
                    return false;
                L(i, i) = std::sqrt(s);
            }
            else
            {
                L(i, j) = s / L(j, j);
            }
        }
    }
    return true;
}

//Solves L*L^T*x = b given the Cholesky factor L.
inline void cholSolve(const Mat& L, const Vec& b, Vec& x)
{
    const size_t n = L.rows();
    Vec y(n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        double s = b[i];
        for (size_t k = 0; k < i; ++k)
            s -= L(i, k) * y[k];
        y[i] = s / L(i, i);
    }
    x.assign(n, 0.0);
    for (size_t ii = n; ii-- > 0; )
    {
        double s = y[ii];
        for (size_t k = ii + 1; k < n; ++k)
            s -= L(k, ii) * x[k];
        x[ii] = s / L(ii, ii);
    }
}

//True if A is symmetric positive definite (used to check Slater / convexity input).
inline bool isSPD(const Mat& A)
{
    Mat L;
    return cholesky(A, L);
}

} //namespace la
