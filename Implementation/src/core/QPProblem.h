//
//  QPProblem.h
//
//  Convex quadratic program
//
//      minimize    f0(x) = 1/2 x'Px + q'x          (P symmetric positive definite)
//      subject to  a_i'x <= b_i,  i = 1..m
//
//  The Lagrangian is
//
//      L(x,lambda) = 1/2 x'Px + q'x + lambda'(Ax - b)
//
//  which is strictly convex in x, so the inner minimisation is solved in closed form:
//
//      x(lambda) = -P^{-1}(q + A'lambda)
//      g(lambda) = -1/2 (q + A'lambda)' P^{-1} (q + A'lambda) - b'lambda
//
//  Written with H = A P^{-1} A' and d = b + A P^{-1} q, maximising g over lambda >= 0 is
//
//      minimize  1/2 lambda' H lambda + d'lambda     subject to lambda >= 0
//
//  which is solved here by exact cyclic coordinate descent with clamping at zero.
//
//  The primal is solved independently by a logarithmic barrier method (Newton with a
//  backtracking line search) so that p* and d* can be cross-validated rather than being
//  two readings of the same computation.
//
#pragma once
#include "LinAlg.h"
#include <limits>

namespace core
{

struct QPData
{
    la::Mat P;      // n x n, symmetric positive definite
    la::Vec q;      // n
    la::Mat A;      // m x n
    la::Vec b;      // m

    size_t n() const { return q.size(); }
    size_t m() const { return b.size(); }
};

struct QPSolution
{
    bool ok = false;
    bool spd = false;           // P positive definite
    bool slater = false;        // a strictly feasible point was found
    la::Vec x;                  // primal optimum recovered from the dual
    la::Vec lambda;             // dual optimum
    la::Vec slack;              // b - Ax at the optimum
    double pStar = 0.0;         // f0(x*)
    double dStar = 0.0;         // g(lambda*)
    double gap = 0.0;           // pStar - dStar
    double pStarBarrier = 0.0;  // independent primal value from the barrier method
    la::Vec xBarrier;
    bool barrierOk = false;
    size_t dualIters = 0;
};

class QPSolver
{
public:
    //---------------------------------------------------------------- objective / feasibility
    static double f0(const QPData& qp, const la::Vec& x)
    {
        return 0.5 * la::dot(x, la::mul(qp.P, x)) + la::dot(qp.q, x);
    }

    //b - A x
    static la::Vec slack(const QPData& qp, const la::Vec& x)
    {
        la::Vec s = la::mul(qp.A, x);
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = qp.b[i] - s[i];
        return s;
    }

    //max_i (a_i'x - b_i). Negative means x is strictly inside the feasible set, so the
    //value must not be clamped at zero - the barrier method relies on the sign.
    static double maxViolation(const QPData& qp, const la::Vec& x)
    {
        if (qp.m() == 0)
            return -1.0;
        la::Vec s = slack(qp, x);
        double v = -std::numeric_limits<double>::infinity();
        for (double si : s)
            v = std::max(v, -si);
        return v;
    }

    //---------------------------------------------------------------- dual function
    //x(lambda) = -P^{-1}(q + A'lambda); returns false if P is not positive definite.
    static bool xOfLambda(const QPData& qp, const la::Mat& L, const la::Vec& lambda, la::Vec& x)
    {
        la::Vec rhs = la::mulT(qp.A, lambda);
        for (size_t j = 0; j < rhs.size(); ++j)
            rhs[j] = -(qp.q[j] + rhs[j]);
        la::cholSolve(L, rhs, x);
        return true;
    }

    //g(lambda) = L(x(lambda), lambda)
    static double dualValue(const QPData& qp, const la::Mat& L, const la::Vec& lambda)
    {
        la::Vec x;
        xOfLambda(qp, L, lambda, x);
        la::Vec Ax = la::mul(qp.A, x);
        double v = f0(qp, x);
        for (size_t i = 0; i < lambda.size(); ++i)
            v += lambda[i] * (Ax[i] - qp.b[i]);
        return v;
    }

    //---------------------------------------------------------------- dual maximisation
    //minimize 1/2 l'Hl + d'l over l >= 0 by exact cyclic coordinate descent
    static la::Vec solveDual(const QPData& qp, const la::Mat& L, size_t& itersOut)
    {
        const size_t m = qp.m();
        la::Vec lambda(m, 0.0);
        if (m == 0)
        {
            itersOut = 0;
            return lambda;
        }

        //H = A P^{-1} A',  d = b + A P^{-1} q
        la::Mat Pinv_At(qp.n(), m, 0.0);
        for (size_t i = 0; i < m; ++i)
        {
            la::Vec ai(qp.n());
            for (size_t j = 0; j < qp.n(); ++j)
                ai[j] = qp.A(i, j);
            la::Vec col;
            la::cholSolve(L, ai, col);
            for (size_t j = 0; j < qp.n(); ++j)
                Pinv_At(j, i) = col[j];
        }

        la::Mat H(m, m, 0.0);
        for (size_t i = 0; i < m; ++i)
            for (size_t k = 0; k < m; ++k)
            {
                double s = 0.0;
                for (size_t j = 0; j < qp.n(); ++j)
                    s += qp.A(i, j) * Pinv_At(j, k);
                H(i, k) = s;
            }

        la::Vec Pinv_q;
        la::cholSolve(L, qp.q, Pinv_q);
        la::Vec d(m, 0.0);
        for (size_t i = 0; i < m; ++i)
        {
            double s = 0.0;
            for (size_t j = 0; j < qp.n(); ++j)
                s += qp.A(i, j) * Pinv_q[j];
            d[i] = qp.b[i] + s;
        }

        const size_t maxIt = 20000;
        size_t it = 0;
        for (; it < maxIt; ++it)
        {
            double maxChange = 0.0;
            for (size_t i = 0; i < m; ++i)
            {
                if (H(i, i) < 1e-12)
                    continue;   //degenerate direction, leave the multiplier where it is

                double s = d[i];
                for (size_t k = 0; k < m; ++k)
                    if (k != i)
                        s += H(i, k) * lambda[k];

                double newVal = -s / H(i, i);
                if (newVal < 0.0)
                    newVal = 0.0;
                maxChange = std::max(maxChange, std::fabs(newVal - lambda[i]));
                lambda[i] = newVal;
            }
            if (maxChange < 1e-12)
                break;
        }
        itersOut = it;
        return lambda;
    }

    //---------------------------------------------------------------- phase I
    //Minimises sum_i max(0, a_i'x - b_i + margin)^2 to land strictly inside the feasible set.
    static bool findStrictlyFeasible(const QPData& qp, la::Vec& x0, double margin = 1e-3)
    {
        const size_t n = qp.n();
        const size_t m = qp.m();
        la::Vec x(n, 0.0);
        if (m == 0) { x0 = x; return true; }

        double step = 1.0;
        for (size_t it = 0; it < 5000; ++it)
        {
            la::Vec Ax = la::mul(qp.A, x);
            double val = 0.0;
            la::Vec grad(n, 0.0);
            for (size_t i = 0; i < m; ++i)
            {
                const double r = Ax[i] - qp.b[i] + margin;
                if (r > 0.0)
                {
                    val += r * r;
                    for (size_t j = 0; j < n; ++j)
                        grad[j] += 2.0 * r * qp.A(i, j);
                }
            }
            if (val <= 0.0)
                break;
            if (la::normInf(grad) < 1e-14)
                break;

            //backtracking on the same penalty
            bool moved = false;
            for (int t = 0; t < 60; ++t)
            {
                la::Vec cand = la::axpy(x, -step, grad);
                la::Vec Ac = la::mul(qp.A, cand);
                double v2 = 0.0;
                for (size_t i = 0; i < m; ++i)
                {
                    const double r = Ac[i] - qp.b[i] + margin;
                    if (r > 0.0)
                        v2 += r * r;
                }
                if (v2 < val)
                {
                    x = cand;
                    step *= 1.6;
                    moved = true;
                    break;
                }
                step *= 0.5;
            }
            if (!moved)
                break;
        }

        x0 = x;
        return maxViolation(qp, x) < -1e-9;   //strictly inside
    }

    //---------------------------------------------------------------- log barrier primal
    //minimize t*f0(x) - sum log(b_i - a_i'x), t increased geometrically.
    static bool solvePrimalBarrier(const QPData& qp, la::Vec& xOut, double& fOut)
    {
        const size_t n = qp.n();
        const size_t m = qp.m();

        la::Vec x;
        if (!findStrictlyFeasible(qp, x))
            return false;

        double t = 1.0;
        const double mu = 12.0;
        const double eps = 1e-10;

        for (int outer = 0; outer < 80; ++outer)
        {
            //Newton on the centring problem
            for (int inner = 0; inner < 100; ++inner)
            {
                la::Vec s = slack(qp, x);
                la::Vec grad = la::mul(qp.P, x);
                for (size_t j = 0; j < n; ++j)
                    grad[j] = t * (grad[j] + qp.q[j]);

                la::Mat Hess(n, n, 0.0);
                for (size_t j = 0; j < n; ++j)
                    for (size_t k = 0; k < n; ++k)
                        Hess(j, k) = t * qp.P(j, k);

                for (size_t i = 0; i < m; ++i)
                {
                    const double si = s[i];
                    if (si <= 0.0)
                        return false;
                    const double inv = 1.0 / si;
                    for (size_t j = 0; j < n; ++j)
                    {
                        grad[j] += qp.A(i, j) * inv;
                        for (size_t k = 0; k < n; ++k)
                            Hess(j, k) += qp.A(i, j) * qp.A(i, k) * inv * inv;
                    }
                }

                la::Vec negGrad(n);
                for (size_t j = 0; j < n; ++j)
                    negGrad[j] = -grad[j];

                la::Vec dx;
                if (!la::solveLU(Hess, negGrad, dx))
                    return false;

                const double lambdaSq = -la::dot(grad, dx);   //Newton decrement squared
                if (lambdaSq / 2.0 <= 1e-12)
                    break;

                //backtracking line search keeping strict feasibility
                double step = 1.0;
                const double alpha = 0.25, beta = 0.5;
                const double f0Cur = barrierObjective(qp, x, t);
                for (int ls = 0; ls < 80; ++ls)
                {
                    la::Vec cand = la::axpy(x, step, dx);
                    if (maxViolation(qp, cand) < 0.0)
                    {
                        const double fCand = barrierObjective(qp, cand, t);
                        if (fCand <= f0Cur - alpha * step * lambdaSq)
                        {
                            x = cand;
                            break;
                        }
                    }
                    step *= beta;
                    if (ls == 79)
                        return false;
                }
            }

            if (m == 0 || double(m) / t < eps)
                break;
            t *= mu;
        }

        xOut = x;
        fOut = f0(qp, x);
        return true;
    }

    static double barrierObjective(const QPData& qp, const la::Vec& x, double t)
    {
        double v = t * f0(qp, x);
        la::Vec s = slack(qp, x);
        for (double si : s)
        {
            if (si <= 0.0)
                return std::numeric_limits<double>::infinity();
            v -= std::log(si);
        }
        return v;
    }

    //---------------------------------------------------------------- full solve
    static QPSolution solve(const QPData& qp)
    {
        QPSolution sol;
        sol.spd = la::isSPD(qp.P);
        if (!sol.spd)
            return sol;

        la::Mat L;
        la::cholesky(qp.P, L);

        sol.lambda = solveDual(qp, L, sol.dualIters);
        xOfLambda(qp, L, sol.lambda, sol.x);
        sol.slack = slack(qp, sol.x);
        sol.pStar = f0(qp, sol.x);
        sol.dStar = dualValue(qp, L, sol.lambda);
        sol.gap = sol.pStar - sol.dStar;

        la::Vec x0;
        sol.slater = findStrictlyFeasible(qp, x0);

        sol.barrierOk = solvePrimalBarrier(qp, sol.xBarrier, sol.pStarBarrier);
        sol.ok = true;
        return sol;
    }

    //p*(u): optimal value when constraint 'idx' is perturbed to a_i'x <= b_i + u.
    //Used for the sensitivity plot, where the theory predicts dp*/du = -lambda_i*.
    static bool perturbedOptimum(const QPData& qp, size_t idx, double u, double& pOut)
    {
        QPData q2 = qp;
        if (idx >= q2.m())
            return false;
        q2.b[idx] += u;

        if (!la::isSPD(q2.P))
            return false;
        la::Mat L;
        la::cholesky(q2.P, L);
        size_t it = 0;
        la::Vec lam = solveDual(q2, L, it);
        la::Vec x;
        xOfLambda(q2, L, lam, x);
        //the dual optimum equals the primal one here (strong duality), and is numerically
        //the more reliable of the two because it needs no feasible starting point
        pOut = dualValue(q2, L, lam);
        return true;
    }
};

} //namespace core
