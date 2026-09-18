//
//  AppModel.h
//
//  Holds the three problem instances, their solutions, and the small geometric helpers the
//  views need.  Every view reads from this one object, so a single recompute() keeps the
//  whole application consistent.
//
#pragma once
#include "QPProblem.h"
#include "AllocationProblem.h"
#include "NonconvexProblem.h"
#include <vector>
#include <cmath>

namespace core
{

//Sutherland-Hodgman clipping of a polygon by the half-plane a'x <= b.
inline std::vector<std::pair<double, double>>
clipHalfPlane(const std::vector<std::pair<double, double>>& poly,
              double a0, double a1, double b)
{
    std::vector<std::pair<double, double>> out;
    const size_t n = poly.size();
    if (n == 0)
        return out;

    auto inside = [&](const std::pair<double, double>& p)
    {
        return a0 * p.first + a1 * p.second <= b + 1e-12;
    };
    auto intersect = [&](const std::pair<double, double>& p,
                         const std::pair<double, double>& q)
    {
        const double fp = a0 * p.first + a1 * p.second - b;
        const double fq = a0 * q.first + a1 * q.second - b;
        const double t = fp / (fp - fq);
        return std::make_pair(p.first + t * (q.first - p.first),
                              p.second + t * (q.second - p.second));
    };

    for (size_t i = 0; i < n; ++i)
    {
        const auto& cur = poly[i];
        const auto& prv = poly[(i + n - 1) % n];
        const bool ci = inside(cur), pi = inside(prv);
        if (ci)
        {
            if (!pi)
                out.push_back(intersect(prv, cur));
            out.push_back(cur);
        }
        else if (pi)
        {
            out.push_back(intersect(prv, cur));
        }
    }
    return out;
}

//Feasible polygon of {x : Ax <= b} intersected with the drawing box, for n = 2.
inline std::vector<std::pair<double, double>>
feasiblePolygon(const QPData& qp, double xlo, double xhi, double ylo, double yhi)
{
    std::vector<std::pair<double, double>> poly = {
        { xlo, ylo }, { xhi, ylo }, { xhi, yhi }, { xlo, yhi }
    };
    if (qp.n() != 2)
        return {};
    for (size_t i = 0; i < qp.m(); ++i)
    {
        poly = clipHalfPlane(poly, qp.A(i, 0), qp.A(i, 1), qp.b[i]);
        if (poly.empty())
            break;
    }
    return poly;
}

class AppModel
{
public:
    QPData          qp;
    QPSolution      qpSol;

    AllocData       alloc;
    AllocSolution   allocSol;

    NonconvexData   nc;
    NonconvexSolution ncSol;

    size_t          sensConstraint = 0;     //QP constraint shown on the sensitivity tab
    double          sensRange = 1.5;        //half-width of the perturbation sweep

    AppModel()
    {
        resetDefaults();
        recompute();
    }

    void resetDefaults()
    {
        //  minimize  x1^2 + x1 x2 + x2^2 - 3 x1 - 4 x2
        //  s.t.      x1 + x2 <= 2        (active at the optimum, lambda1 > 0)
        //           -x1 + 2 x2 <= 4      (slack at the optimum, lambda2 = 0)
        //
        //  Chosen so that complementary slackness has something to show: one constraint
        //  binds and carries a positive price, the other is free and prices at zero.
        qp.P.resize(2, 2);
        qp.P(0, 0) = 2.0; qp.P(0, 1) = 1.0;
        qp.P(1, 0) = 1.0; qp.P(1, 1) = 2.0;
        qp.q = { -3.0, -4.0 };
        qp.A.resize(2, 2);
        qp.A(0, 0) =  1.0; qp.A(0, 1) = 1.0;
        qp.A(1, 0) = -1.0; qp.A(1, 1) = 2.0;
        qp.b = { 2.0, 4.0 };

        alloc.w     = { 3.0, 2.0, 1.0, 0.5 };
        alloc.alpha = { 1.0, 0.5, 2.0, 0.2 };
        alloc.B     = 6.0;

        nc = NonconvexData();
        sensConstraint = 0;
    }

    void recompute()
    {
        qpSol    = QPSolver::solve(qp);
        allocSol = AllocSolver::solve(alloc);
        ncSol    = NonconvexSolver::solve(nc);
    }

    //Keeps P symmetric after the user edits the off-diagonal entry.
    void symmetrizeP()
    {
        const double off = 0.5 * (qp.P(0, 1) + qp.P(1, 0));
        qp.P(0, 1) = off;
        qp.P(1, 0) = off;
    }
};

} //namespace core
