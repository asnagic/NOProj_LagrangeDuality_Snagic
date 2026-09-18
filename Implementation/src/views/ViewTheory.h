//
//  ViewTheory.h
//
//  A running commentary on the three instances: the construction of the dual function, the
//  two duality theorems, Slater's condition, and the cross-validation of every claim
//  against the numbers the solvers actually produced.  Regenerated on every change, so it
//  always describes the data currently on screen.
//
#pragma once
#include <gui/View.h>
#include <gui/TextEdit.h>
#include <gui/VerticalLayout.h>
#include "core/AppModel.h"

class ViewTheory : public gui::View
{
    gui::TextEdit _text;
    gui::VerticalLayout _vl;
    core::AppModel* _pModel = nullptr;

public:
    ViewTheory()
    : _vl(1)
    {
        setMargins(0, 0, 0, 0);
        _text.setAsReadOnly();
        _vl << _text;
        setLayout(&_vl);
    }

    void attach(core::AppModel* pModel) { _pModel = pModel; }

    void refresh()
    {
        if (!_pModel)
            return;
        _text.clean();
        _text.appendString(build());
    }

private:
    static td::String pass(bool ok)
    {
        return ok ? td::String("OK") : td::String("FAILED");
    }

    td::String build() const
    {
        const core::QPSolution& q = _pModel->qpSol;
        const core::AllocSolution& a = _pModel->allocSol;
        const core::NonconvexSolution& n = _pModel->ncSol;
        td::String o, l;

        o += "LAGRANGE DUALITY - construction, theorems, and what the numbers say\n";
        o += "===================================================================\n\n";

        o += "1. THE DUAL FUNCTION\n";
        o += "--------------------\n";
        o += "For  minimize f0(x)  subject to  fi(x) <= 0, i = 1..m,  the Lagrangian is\n\n";
        o += "    L(x, lambda) = f0(x) + sum_i lambda_i fi(x)\n\n";
        o += "and the dual function is its pointwise infimum over x:\n\n";
        o += "    g(lambda) = inf_x L(x, lambda).\n\n";
        o += "g is concave whatever f0 and fi are, because for each fixed x the map\n";
        o += "lambda -> L(x, lambda) is affine, and a pointwise infimum of affine functions is\n";
        o += "concave. This is why the dual problem is always a convex problem, even when the\n";
        o += "primal is not.\n\n";

        o += "2. WEAK DUALITY\n";
        o += "---------------\n";
        o += "For any feasible x and any lambda >= 0,\n\n";
        o += "    g(lambda) = inf_z L(z,lambda) <= L(x,lambda) = f0(x) + sum_i lambda_i fi(x)\n";
        o += "                                  <= f0(x),\n\n";
        o += "since lambda_i >= 0 and fi(x) <= 0. Taking the best of each side, d* <= p*.\n";
        o += "No convexity was used anywhere in that argument.\n\n";

        o += "3. STRONG DUALITY AND SLATER'S CONDITION\n";
        o += "----------------------------------------\n";
        o += "d* = p* is not automatic. Slater's condition is a sufficient condition: if the\n";
        o += "problem is convex and there is a strictly feasible point, then d* = p* and the\n";
        o += "dual optimum is attained. It is sufficient, not necessary - section 6 shows a\n";
        o += "problem where it fails and the gap opens.\n\n";

        o += "-------------------------------------------------------------------\n";
        o += "4. INSTANCE A - CONVEX QUADRATIC PROGRAM\n";
        o += "-------------------------------------------------------------------\n";
        o += "    minimize   1/2 x'Px + q'x        subject to  Ax <= b\n\n";
        o += "L is strictly convex in x when P is positive definite, so the inner minimisation\n";
        o += "is solved by setting the gradient to zero:\n\n";
        o += "    Px + q + A'lambda = 0   =>   x(lambda) = -P^{-1}(q + A'lambda)\n\n";
        o += "Substituting back gives the dual function in closed form:\n\n";
        o += "    g(lambda) = -1/2 (q + A'lambda)' P^{-1} (q + A'lambda) - b'lambda\n\n";
        o += "so the dual problem is itself a quadratic program, over lambda >= 0.\n\n";

        if (!q.spd)
        {
            o += "  P is currently NOT positive definite, so this section has nothing to report.\n\n";
        }
        else
        {
            l.format("  P positive definite            : %s\n", pass(q.spd).c_str());
            o += l;
            l.format("  strictly feasible point exists : %s  (Slater)\n", pass(q.slater).c_str());
            o += l;
            l.format("  p* (from the dual optimum)     : %.10f\n", q.pStar);
            o += l;
            if (q.barrierOk)
            {
                l.format("  p* (independent log barrier)   : %.10f\n", q.pStarBarrier);
                o += l;
                l.format("  the two primal routes agree to : %.2e\n",
                         std::fabs(q.pStar - q.pStarBarrier));
                o += l;
            }
            l.format("  d*                             : %.10f\n", q.dStar);
            o += l;
            l.format("  duality gap                    : %.3e   %s\n", q.gap,
                     (std::fabs(q.gap) < 1e-7) ? "-> strong duality holds"
                                               : "-> unexpected, check the data");
            o += l;
            o += "\n  Complementary slackness: lambda_i (b_i - a_i'x*) = 0 for every i.\n";
            for (size_t i = 0; i < q.lambda.size(); ++i)
            {
                l.format("    i=%zu  lambda=%10.6f  slack=%10.6f  product=%.2e\n",
                         i + 1, q.lambda[i], q.slack[i], q.lambda[i] * q.slack[i]);
                o += l;
            }
            o += "\n  Read that line by line: a constraint is either tight (slack zero, any\n";
            o += "  price allowed) or slack (price forced to zero). Never both loose.\n\n";
        }

        o += "-------------------------------------------------------------------\n";
        o += "5. INSTANCE B - LINEAR RESOURCE ALLOCATION\n";
        o += "-------------------------------------------------------------------\n";
        o += "    maximize  sum_i w_i log(1 + x_i/alpha_i)   s.t.  sum_i x_i <= B,  x >= 0\n\n";
        o += "Only the budget is dualised. L then separates over i and each piece is minimised\n";
        o += "in closed form, giving the water-filling rule\n\n";
        o += "    x_i(lambda) = max(0, w_i/lambda - alpha_i).\n\n";
        o += "lambda* is fixed by sum_i x_i(lambda) = B, which has a unique root because the\n";
        o += "sum is strictly decreasing in lambda.\n\n";

        if (a.ok)
        {
            l.format("  lambda*   : %.8f\n  utility   : %.8f\n", a.lambda, a.utility);
            o += l;
            l.format("  p*        : %.10f\n  d*        : %.10f\n  gap       : %.2e\n",
                     a.pStar, a.dStar, a.gap);
            o += l;
            o += "\n  lambda* is the shadow price of the budget, in utility per unit. It is also\n";
            o += "  the common marginal utility of every funded item - the market-clearing\n";
            o += "  reading of the multiplier. See the Allocation tab for the numerical check\n";
            o += "  that dp*/dB = -lambda*.\n\n";
        }

        o += "-------------------------------------------------------------------\n";
        o += "6. INSTANCE C - WHAT A NON-ZERO GAP LOOKS LIKE\n";
        o += "-------------------------------------------------------------------\n";
        o += "    minimize  x^4 + c2 x^2 + c1 x    subject to  x >= xMin\n\n";
        o += "With c2 < 0 the objective is a double well and the problem is not convex, so\n";
        o += "Slater's condition does not apply even though strictly feasible points exist.\n\n";
        l.format("  p*        : %.8f\n  d*        : %.8f\n  gap       : %.8f\n",
                 n.pStar, n.dStar, n.gap);
        o += l;
        o += "\n  The perturbation function p(u) = min{ f0(x) : xMin - x <= u } explains the\n";
        o += "  number. d* is the value at u = 0 of the largest convex function lying below p.\n";
        o += "  Where p has a non-convex dent, the dual replaces it with the chord across it\n";
        o += "  and reports that instead - the gap is the height of the dent.\n\n";
        o += "  Weak duality is untouched: d* <= p* still holds, and the dual still delivers a\n";
        o += "  certified lower bound. That is exactly how duality earns its keep in\n";
        o += "  non-convex optimisation - as a bound, not as an equality.\n\n";

        o += "-------------------------------------------------------------------\n";
        o += "7. SUMMARY OF THE CHECKS\n";
        o += "-------------------------------------------------------------------\n";
        l.format("  weak duality  d* <= p*   (QP)          : %s\n",
                 pass(q.dStar <= q.pStar + 1e-7).c_str());
        o += l;
        l.format("  weak duality  d* <= p*   (allocation)  : %s\n",
                 pass(a.dStar <= a.pStar + 1e-7).c_str());
        o += l;
        l.format("  weak duality  d* <= p*   (non-convex)  : %s\n",
                 pass(n.dStar <= n.pStar + 1e-6).c_str());
        o += l;
        l.format("  strong duality           (QP)          : %s\n",
                 pass(std::fabs(q.gap) < 1e-7).c_str());
        o += l;
        l.format("  strong duality           (allocation)  : %s\n",
                 pass(std::fabs(a.gap) < 1e-7).c_str());
        o += l;
        l.format("  strong duality           (non-convex)  : %s  <- expected to fail\n",
                 pass(n.strongDuality).c_str());
        o += l;
        return o;
    }
};
