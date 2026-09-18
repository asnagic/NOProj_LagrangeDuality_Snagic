//
//  NonconvexProblem.h
//
//  A one-dimensional problem whose regularity conditions fail, used to show what happens
//  when strong duality does not hold.
//
//      minimize    f0(x) = x^4 + c2 x^2 + c1 x        (a double well for c2 < 0)
//      subject to  f1(x) = xMin - x <= 0              (that is, x >= xMin)
//
//  f0 is not convex, so nothing forces p* = d*.  Two pictures make the gap visible:
//
//   1. The dual function g(lambda) = inf_x ( f0(x) + lambda (xMin - x) ) is still concave
//      (an infimum of affine functions of lambda), and d* = max_{lambda>=0} g(lambda)
//      is still a lower bound on p* - weak duality never needs convexity.
//
//   2. The perturbation function p(u) = min{ f0(x) : f1(x) <= u }.  The dual optimum is
//      the value at u = 0 of the largest convex minorant of p.  When p has a non-convex
//      dent at the origin, that minorant lies strictly below p(0) and the gap is the
//      vertical distance between them.
//
//  Everything is evaluated on a fine grid with a local golden-section refinement, which is
//  robust for a coercive quartic and keeps the picture honest - no solver is being asked
//  to find a global minimum it cannot certify.
//
#pragma once
#include "LinAlg.h"
#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>

namespace core
{

struct NonconvexData
{
    double c2 = -6.0;       // coefficient of x^2
    double c1 = 1.0;        // coefficient of x
    double xMin = 0.0;      // constraint x >= xMin
    double xLo = -4.0;      // search window
    double xHi = 4.0;
};

struct NonconvexSolution
{
    bool ok = false;
    double xStar = 0.0;
    double pStar = 0.0;
    double lambdaStar = 0.0;
    double dStar = 0.0;
    double gap = 0.0;       // pStar - dStar, strictly positive when strong duality fails
    bool strongDuality = false;
};

class NonconvexSolver
{
public:
    static double f0(const NonconvexData& nd, double x)
    {
        return x * x * x * x + nd.c2 * x * x + nd.c1 * x;
    }

    //Global minimum of f0 on [lo, hi] by grid scan plus golden-section refinement.
    static double minOnInterval(const NonconvexData& nd, double lo, double hi, double& argMin)
    {
        if (hi < lo)
        {
            argMin = lo;
            return std::numeric_limits<double>::infinity();
        }

        const int N = 4000;
        double best = std::numeric_limits<double>::infinity();
        double bestX = lo;
        int bestI = 0;
        for (int i = 0; i <= N; ++i)
        {
            const double x = lo + (hi - lo) * double(i) / double(N);
            const double v = f0(nd, x);
            if (v < best) { best = v; bestX = x; bestI = i; }
        }

        //refine inside the neighbouring cells of the best grid point
        const double h = (hi - lo) / double(N);
        double a = std::max(lo, bestX - h);
        double b = std::min(hi, bestX + h);
        const double invPhi = 0.6180339887498949;
        double c = b - invPhi * (b - a);
        double d = a + invPhi * (b - a);
        for (int it = 0; it < 120; ++it)
        {
            if (f0(nd, c) < f0(nd, d)) b = d; else a = c;
            c = b - invPhi * (b - a);
            d = a + invPhi * (b - a);
        }
        const double xr = 0.5 * (a + b);
        const double vr = f0(nd, xr);
        if (vr < best) { best = vr; bestX = xr; }
        (void)bestI;

        argMin = bestX;
        return best;
    }

    //g(lambda) = inf_x ( f0(x) + lambda(xMin - x) ) over the search window
    static double dualValue(const NonconvexData& nd, double lambda)
    {
        //shift the linear coefficient: f0(x) - lambda x + lambda*xMin
        NonconvexData shifted = nd;
        shifted.c1 = nd.c1 - lambda;
        double arg = 0.0;
        const double v = minOnInterval(shifted, nd.xLo, nd.xHi, arg);
        return v + lambda * nd.xMin;
    }

    //p(u) = min{ f0(x) : xMin - x <= u } = min over x >= xMin - u
    static double perturbedOptimum(const NonconvexData& nd, double u)
    {
        const double lo = std::max(nd.xLo, nd.xMin - u);
        double arg = 0.0;
        if (lo > nd.xHi)
            return std::numeric_limits<double>::infinity();
        return minOnInterval(nd, lo, nd.xHi, arg);
    }

    static NonconvexSolution solve(const NonconvexData& nd, double lambdaMax = 12.0)
    {
        NonconvexSolution sol;

        //primal: minimise over the feasible half-line
        const double lo = std::max(nd.xLo, nd.xMin);
        sol.pStar = minOnInterval(nd, lo, nd.xHi, sol.xStar);

        //dual: maximise the concave g over [0, lambdaMax] by grid + golden section
        const int N = 600;
        double best = -std::numeric_limits<double>::infinity();
        double bestL = 0.0;
        for (int i = 0; i <= N; ++i)
        {
            const double l = lambdaMax * double(i) / double(N);
            const double v = dualValue(nd, l);
            if (v > best) { best = v; bestL = l; }
        }
        const double h = lambdaMax / double(N);
        double a = std::max(0.0, bestL - h);
        double b = std::min(lambdaMax, bestL + h);
        const double invPhi = 0.6180339887498949;
        double c = b - invPhi * (b - a);
        double d = a + invPhi * (b - a);
        for (int it = 0; it < 80; ++it)
        {
            if (dualValue(nd, c) > dualValue(nd, d)) b = d; else a = c;
            c = b - invPhi * (b - a);
            d = a + invPhi * (b - a);
        }
        const double lr = 0.5 * (a + b);
        const double vr = dualValue(nd, lr);
        if (vr > best) { best = vr; bestL = lr; }

        sol.lambdaStar = bestL;
        sol.dStar = best;
        sol.gap = sol.pStar - sol.dStar;
        sol.strongDuality = (std::fabs(sol.gap) < 1e-4);
        sol.ok = true;
        return sol;
    }

    //Lower convex envelope of the sampled perturbation function, by a monotone-chain
    //lower hull over the (u, p(u)) samples.  Points with an infinite value are skipped.
    static void convexEnvelope(const la::Vec& u, const la::Vec& p, la::Vec& hullU, la::Vec& hullP)
    {
        hullU.clear();
        hullP.clear();
        const size_t n = u.size();
        for (size_t i = 0; i < n; ++i)
        {
            if (!std::isfinite(p[i]))
                continue;
            while (hullU.size() >= 2)
            {
                const size_t k = hullU.size();
                const double x1 = hullU[k - 2], y1 = hullP[k - 2];
                const double x2 = hullU[k - 1], y2 = hullP[k - 1];
                //drop the middle point if it is on or above the chord
                if ((y2 - y1) * (u[i] - x1) >= (p[i] - y1) * (x2 - x1))
                {
                    hullU.pop_back();
                    hullP.pop_back();
                }
                else
                    break;
            }
            hullU.push_back(u[i]);
            hullP.push_back(p[i]);
        }
    }

    //Value of the piecewise-linear envelope at a given u (linear interpolation).
    static double envelopeAt(const la::Vec& hullU, const la::Vec& hullP, double u)
    {
        if (hullU.size() < 2)
            return std::numeric_limits<double>::quiet_NaN();
        if (u <= hullU.front()) return hullP.front();
        if (u >= hullU.back())  return hullP.back();
        for (size_t i = 1; i < hullU.size(); ++i)
        {
            if (u <= hullU[i])
            {
                const double t = (u - hullU[i - 1]) / (hullU[i] - hullU[i - 1]);
                return hullP[i - 1] + t * (hullP[i] - hullP[i - 1]);
            }
        }
        return hullP.back();
    }
};

} //namespace core
