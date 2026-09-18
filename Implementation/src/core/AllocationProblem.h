//
//  AllocationProblem.h
//
//  Linear-resource allocation with concave utilities - the classical water-filling problem.
//
//      maximize    sum_i w_i * log(1 + x_i / alpha_i)
//      subject to  sum_i x_i <= B,   x >= 0
//
//  Stated as a minimisation, f0(x) = -sum_i w_i log(1 + x_i/alpha_i) and the single
//  inequality is  sum_i x_i - B <= 0.  Keeping x >= 0 in the domain rather than dualising
//  it leaves one multiplier, so the dual function is a curve that can simply be drawn.
//
//      L(x,lambda) = f0(x) + lambda (sum_i x_i - B)
//
//  separates over i, and each term is minimised in closed form:
//
//      x_i(lambda) = max(0, w_i/lambda - alpha_i)        (the water-filling rule)
//
//  lambda* is the water level: the unique lambda with sum_i x_i(lambda) = B, found by
//  bisection because the sum is strictly decreasing in lambda.  lambda* is the shadow
//  price of the budget: one extra unit of B buys exactly lambda* extra utility.
//
#pragma once
#include "LinAlg.h"
#include <cmath>
#include <vector>

namespace core
{

struct AllocData
{
    la::Vec w;          // utility weights, w_i > 0
    la::Vec alpha;      // saturation constants, alpha_i > 0
    double B = 10.0;    // budget

    size_t n() const { return w.size(); }
};

struct AllocSolution
{
    bool ok = false;
    la::Vec x;              // optimal allocation
    double lambda = 0.0;    // optimal multiplier (water level / shadow price)
    double pStar = 0.0;     // f0(x*)   (negative utility)
    double dStar = 0.0;     // g(lambda*)
    double gap = 0.0;
    double utility = 0.0;   // -pStar
    double used = 0.0;      // sum x_i
    double slack = 0.0;     // B - sum x_i
};

class AllocSolver
{
public:
    static double f0(const AllocData& ad, const la::Vec& x)
    {
        double v = 0.0;
        for (size_t i = 0; i < ad.n(); ++i)
            v -= ad.w[i] * std::log(1.0 + x[i] / ad.alpha[i]);
        return v;
    }

    //x_i(lambda) = max(0, w_i/lambda - alpha_i)
    static la::Vec xOfLambda(const AllocData& ad, double lambda)
    {
        la::Vec x(ad.n(), 0.0);
        if (lambda <= 0.0)
            return x;                       //Lagrangian unbounded below for lambda <= 0
        for (size_t i = 0; i < ad.n(); ++i)
        {
            const double v = ad.w[i] / lambda - ad.alpha[i];
            x[i] = (v > 0.0) ? v : 0.0;
        }
        return x;
    }

    static double totalOfLambda(const AllocData& ad, double lambda)
    {
        la::Vec x = xOfLambda(ad, lambda);
        double s = 0.0;
        for (double v : x)
            s += v;
        return s;
    }

    //g(lambda) = f0(x(lambda)) + lambda (sum x(lambda) - B)
    static double dualValue(const AllocData& ad, double lambda)
    {
        if (lambda <= 0.0)
            return -std::numeric_limits<double>::infinity();
        la::Vec x = xOfLambda(ad, lambda);
        double s = 0.0;
        for (double v : x)
            s += v;
        return f0(ad, x) + lambda * (s - ad.B);
    }

    static AllocSolution solve(const AllocData& ad)
    {
        AllocSolution sol;
        if (ad.n() == 0 || ad.B <= 0.0)
            return sol;

        //bracket lambda*: total(lambda) is strictly decreasing, total -> inf as lambda -> 0+
        double lo = 1e-12, hi = 1.0;
        while (totalOfLambda(ad, hi) > ad.B && hi < 1e12)
            hi *= 2.0;

        //total(hi) <= B now; bisect on total(lambda) = B
        for (int it = 0; it < 300; ++it)
        {
            const double mid = 0.5 * (lo + hi);
            if (totalOfLambda(ad, mid) > ad.B)
                lo = mid;
            else
                hi = mid;
        }
        sol.lambda = 0.5 * (lo + hi);
        sol.x = xOfLambda(ad, sol.lambda);

        double s = 0.0;
        for (double v : sol.x)
            s += v;
        sol.used = s;
        sol.slack = ad.B - s;
        sol.pStar = f0(ad, sol.x);
        sol.dStar = dualValue(ad, sol.lambda);
        sol.gap = sol.pStar - sol.dStar;
        sol.utility = -sol.pStar;
        sol.ok = true;
        return sol;
    }

    //Optimal value with budget B + u, used for the sensitivity plot.
    static double perturbedOptimum(const AllocData& ad, double u)
    {
        AllocData a2 = ad;
        a2.B += u;
        if (a2.B <= 1e-9)
            a2.B = 1e-9;
        AllocSolution s = solve(a2);
        return s.pStar;
    }
};

} //namespace core
