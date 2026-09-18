//
//  ViewGap.h
//
//  What happens when the regularity conditions are violated.
//
//      minimize   f0(x) = x^4 + c2 x^2 + c1 x        (non-convex for c2 < 0)
//      subject to x >= xMin
//
//  Three pictures, left to right and top to bottom:
//
//   * f0 itself, with the feasible half-line and the two local minima.  The global minimum
//     of the well sits outside the feasible region, which is what creates the gap.
//
//   * the dual function g(lambda) = inf_x ( f0(x) + lambda(xMin - x) ).  It is still
//     concave - an infimum of affine functions of lambda always is, convexity of f0 has
//     nothing to do with it - and it still lies below p*.  Weak duality survives; strong
//     duality does not.
//
//   * the perturbation function p(u) = min{ f0(x) : xMin - x <= u } together with its
//     largest convex minorant.  d* is the value of that minorant at u = 0, so the vertical
//     distance between the two curves at the origin is exactly the duality gap.
//
#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/NumericEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/VerticalLayout.h>
#include <gui/SplitterLayout.h>
#include <gui/TextEdit.h>
#include <functional>
#include "PlotCanvas.h"
#include "core/AppModel.h"

class GapParamsView : public gui::View
{
    gui::Label _lblHdr;
    gui::Label _lblC2, _lblC1, _lblXMin;
    gui::NumericEdit _c2, _c1, _xMin;
    gui::Label _lblReport;
    gui::TextEdit _report;
    gui::GridLayout _gl;

    core::AppModel* _pModel = nullptr;
    std::function<void()>* _pOnChanged = nullptr;
    bool _building = false;

public:
    GapParamsView()
    : _lblHdr("min  x^4 + c2 x^2 + c1 x    s.t.  x >= xMin")
    , _lblC2("c2  (negative gives two wells):")
    , _lblC1("c1:")
    , _lblXMin("xMin:")
    , _c2(td::real8, gui::LineEdit::Messages::Send)
    , _c1(td::real8, gui::LineEdit::Messages::Send)
    , _xMin(td::real8, gui::LineEdit::Messages::Send)
    , _lblReport("Results")
    , _gl(6, 2)
    {
        _report.setAsReadOnly();

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblHdr, 0);
        gc.appendRow(_lblC2)   << _c2;
        gc.appendRow(_lblC1)   << _c1;
        gc.appendRow(_lblXMin) << _xMin;
        gc.appendRow(_lblReport, 0);
        gc.appendRow(_report, 0);

        auto fire = [this]() { pushToModel(); };
        _c2.onFinishEdit(fire);   _c2.onActivate(fire);
        _c1.onFinishEdit(fire);   _c1.onActivate(fire);
        _xMin.onFinishEdit(fire); _xMin.onActivate(fire);

        setLayout(&_gl);
    }

    void attach(core::AppModel* pModel, std::function<void()>* pOnChanged)
    {
        _pModel = pModel;
        _pOnChanged = pOnChanged;
        pullFromModel();
    }

    void pullFromModel()
    {
        if (!_pModel)
            return;
        _building = true;
        _c2.setValue(td::Variant(_pModel->nc.c2), false);
        _c1.setValue(td::Variant(_pModel->nc.c1), false);
        _xMin.setValue(td::Variant(_pModel->nc.xMin), false);
        _building = false;
    }

    void pushToModel()
    {
        if (_building || !_pModel)
            return;
        double v = 0.0;
        _c2.getValue(v);   _pModel->nc.c2 = v;
        _c1.getValue(v);   _pModel->nc.c1 = v;
        _xMin.getValue(v); _pModel->nc.xMin = v;

        //keep the search window wide enough to contain both wells
        const double reach = 1.0 + std::sqrt(std::fabs(_pModel->nc.c2)) +
                             0.5 * std::fabs(_pModel->nc.c1);
        _pModel->nc.xLo = std::min(-reach - 1.0, _pModel->nc.xMin - 1.0);
        _pModel->nc.xHi = reach + 1.0;

        _pModel->recompute();
        if (_pOnChanged)
            (*_pOnChanged)();
    }

    void setReport(const td::String& txt)
    {
        _report.clean();
        _report.appendString(txt);
    }
};

class ViewGap : public gui::View
{
    //Three plots and the parameters share a 2x2 grid. A splitter with a stacked side
    //column squeezed the third plot down to a couple of hundred pixels, which is too
    //small to carry a title, a legend and a note.
    gui::GridLayout _gl;
    PlotCanvas _plotF0;
    PlotCanvas _plotP;
    PlotCanvas _plotG;
    GapParamsView _params;
    core::AppModel* _pModel = nullptr;

public:
    ViewGap()
    : _gl(2, 2)
    {
        setMargins(0, 0, 0, 0);
        gui::GridComposer gc(_gl);
        gc.appendRow(_plotF0) << _plotP;
        gc.appendRow(_plotG)  << _params;
        setLayout(&_gl);
    }

    void attach(core::AppModel* pModel, std::function<void()>* pOnChanged)
    {
        _pModel = pModel;
        _params.attach(pModel, pOnChanged);
    }

    void collectCanvases(std::vector<PlotCanvas*>& out, std::vector<td::String>& names)
    {
        out.push_back(&_plotF0); names.push_back("5a_nonconvex_objective");
        out.push_back(&_plotP);  names.push_back("5b_perturbation_and_envelope");
        out.push_back(&_plotG);  names.push_back("5c_nonconvex_dual");
    }

    void refresh()
    {
        if (!_pModel)
            return;
        buildF0();
        buildPerturbation();
        buildDual();
        _params.setReport(buildReport());
        _plotF0.reDraw();
        _plotP.reDraw();
        _plotG.reDraw();
    }

private:
    //------------------------------------------------------------------ f0
    void buildF0()
    {
        const core::NonconvexData& nd = _pModel->nc;
        const core::NonconvexSolution& s = _pModel->ncSol;

        _plotF0.clear();
        _plotF0.autoRange();
        _plotF0.setPlotTitle("Objective f0(x) and the feasible half-line x >= xMin");
        _plotF0.setXLabel("x");
        _plotF0.setYLabel("f0(x)");

        std::vector<double> xs, ys;
        const int N = 500;
        for (int k = 0; k <= N; ++k)
        {
            const double x = nd.xLo + (nd.xHi - nd.xLo) * double(k) / double(N);
            xs.push_back(x);
            ys.push_back(core::NonconvexSolver::f0(nd, x));
        }
        _plotF0.add(PlotCanvas::makeSeries(xs, ys, td::ColorID::SteelBlue, "f0(x)", 2.6f));

        //the feasible part, drawn on top
        std::vector<double> fx, fy;
        for (size_t i = 0; i < xs.size(); ++i)
        {
            if (xs[i] >= nd.xMin)
            {
                fx.push_back(xs[i]);
                fy.push_back(ys[i]);
            }
        }
        _plotF0.add(PlotCanvas::makeSeries(fx, fy, td::ColorID::MediumSeaGreen,
                                           "feasible part", 3.4f));

        //A quartic's tails reach a few hundred while the wells sit near zero; without a
        //focused window the interesting structure collapses onto the axis.
        double fmin = 1e300;
        for (double v : ys)
            fmin = std::min(fmin, v);
        const double fspan = std::max(1.0, std::fabs(fmin));
        _plotF0.setYRange(fmin - 0.25 * fspan, fmin + 2.4 * fspan);

        PlotCanvas::RefLine rl;
        rl.vertical = true;
        rl.value = nd.xMin;
        rl.color = td::ColorID::Crimson;
        rl.pattern = td::LinePattern::Dash;
        rl.width = 1.8f;
        rl.label.format("xMin = %.3f", nd.xMin);
        _plotF0.add(rl);

        PlotCanvas::Marker mk;
        mk.pt = gui::Point(s.xStar, s.pStar);
        mk.color = td::ColorID::Gold;
        mk.radius = 6.0;
        mk.label.format("x* = %.4f,  p* = %.4f", s.xStar, s.pStar);
        _plotF0.add(mk);

        //global minimiser of f0 ignoring the constraint
        double argU = 0.0;
        const double vU = core::NonconvexSolver::minOnInterval(nd, nd.xLo, nd.xHi, argU);
        if (argU < nd.xMin - 1e-6)
        {
            PlotCanvas::Marker mu;
            mu.pt = gui::Point(argU, vU);
            mu.color = td::ColorID::DarkOrange;
            mu.radius = 5.0;
            mu.label.format("global min %.4f is infeasible", vU);
            mu.labelAbove = false;
            _plotF0.add(mu);
        }
    }

    //------------------------------------------------------------------ p(u) and its envelope
    void buildPerturbation()
    {
        const core::NonconvexData& nd = _pModel->nc;
        const core::NonconvexSolution& s = _pModel->ncSol;

        _plotP.clear();
        _plotP.autoRange();
        _plotP.setPlotTitle("Perturbation function p(u) and its convex envelope");
        _plotP.setXLabel("u   (relaxation of the constraint)");
        _plotP.setYLabel("p(u)");

        const double R = std::max(1.0, nd.xHi - nd.xMin);
        la::Vec us, ps;
        const int N = 320;
        for (int k = 0; k <= N; ++k)
        {
            const double u = -R + 2.0 * R * double(k) / double(N);
            const double v = core::NonconvexSolver::perturbedOptimum(nd, u);
            us.push_back(u);
            ps.push_back(v);
        }

        la::Vec hu, hp;
        core::NonconvexSolver::convexEnvelope(us, ps, hu, hp);

        _plotP.add(PlotCanvas::makeSeries(us, ps, td::ColorID::SteelBlue, "p(u)", 2.8f));
        _plotP.add(PlotCanvas::makeSeries(hu, hp, td::ColorID::DarkOrange,
                                          "convex envelope of p", 2.2f,
                                          td::LinePattern::Dash));

        const double env0 = core::NonconvexSolver::envelopeAt(hu, hp, 0.0);
        const double p0 = core::NonconvexSolver::perturbedOptimum(nd, 0.0);

        //zoom onto the band that holds p(0), the envelope and the drop
        double pmin = 1e300;
        for (double v : ps)
            if (std::isfinite(v))
                pmin = std::min(pmin, v);
        const double lo = std::min(pmin, std::isfinite(env0) ? env0 : pmin);
        const double span = std::max(1.0, std::fabs(p0 - lo));
        _plotP.setYRange(lo - 0.45 * span, p0 + 1.6 * span);

        //the gap itself, drawn as the segment it is
        if (std::isfinite(env0) && std::isfinite(p0))
        {
            std::vector<double> gx = { 0.0, 0.0 };
            std::vector<double> gy = { env0, p0 };
            auto gser = PlotCanvas::makeSeries(gx, gy, td::ColorID::Crimson,
                                               "duality gap at u = 0", 3.2f);
            gser.style = PlotCanvas::SeriesStyle::LineAndDots;
            _plotP.add(gser);
        }

        td::String note;
        note.format("p(0) = %.4f, envelope(0) = %.4f: d* is the envelope value.",
                    p0, env0);
        _plotP.setNote(note);
        (void)s;
    }

    //------------------------------------------------------------------ g(lambda)
    void buildDual()
    {
        const core::NonconvexData& nd = _pModel->nc;
        const core::NonconvexSolution& s = _pModel->ncSol;

        _plotG.clear();
        _plotG.autoRange();
        _plotG.setPlotTitle("Dual function g(lambda) - concave even though f0 is not convex");
        _plotG.setXLabel("lambda");
        _plotG.setYLabel("g(lambda)");

        const double lMax = std::max(2.0, 3.0 * s.lambdaStar);
        std::vector<double> xs, ys;
        const int N = 220;
        for (int k = 0; k <= N; ++k)
        {
            const double l = lMax * double(k) / double(N);
            xs.push_back(l);
            ys.push_back(core::NonconvexSolver::dualValue(nd, l));
        }
        _plotG.add(PlotCanvas::makeSeries(xs, ys, td::ColorID::SteelBlue, "g(lambda)", 2.6f));

        PlotCanvas::RefLine rp;
        rp.value = s.pStar;
        rp.color = td::ColorID::Crimson;
        rp.pattern = td::LinePattern::Dash;
        rp.width = 1.8f;
        rp.label.format("p* = %.6f", s.pStar);
        _plotG.add(rp);

        PlotCanvas::RefLine rd;
        rd.value = s.dStar;
        rd.color = td::ColorID::DarkOrange;
        rd.pattern = td::LinePattern::Dot;
        rd.width = 1.6f;
        rd.label.format("d* = %.6f", s.dStar);
        _plotG.add(rd);

        PlotCanvas::Marker mk;
        mk.pt = gui::Point(s.lambdaStar, s.dStar);
        mk.color = td::ColorID::Gold;
        mk.radius = 6.0;
        mk.label.format("lambda* = %.4f", s.lambdaStar);
        _plotG.add(mk);

        td::String note;
        note.format("Weak duality survives without convexity: gap %.4f.", s.gap);
        _plotG.setNote(note);
    }

    //------------------------------------------------------------------ report
    td::String buildReport() const
    {
        const core::NonconvexSolution& s = _pModel->ncSol;
        td::String out, line;

        line.format("x*        = %.8f\np*        = %.8f\n", s.xStar, s.pStar);
        out += line;
        line.format("lambda*   = %.8f\nd*        = %.8f\n\n", s.lambdaStar, s.dStar);
        out += line;
        line.format("duality gap p* - d* = %.8f\n\n", s.gap);
        out += line;

        if (s.strongDuality)
        {
            out += "For this data the gap has closed. That can happen even without convexity -\n";
            out += "strong duality is sufficient-condition territory, not an if-and-only-if. Try\n";
            out += "making c2 more negative, or moving xMin between the two wells, to reopen it.\n";
        }
        else
        {
            out += "Strong duality FAILS here. Weak duality still holds: d* <= p* always, because\n";
            out += "g(lambda) = inf_x L(x,lambda) <= L(x_feasible, lambda) <= f0(x_feasible).\n";
            out += "Convexity was never needed for that half of the theory.\n\n";
            out += "What breaks is the other half. Slater's condition asks for a convex problem\n";
            out += "with a strictly feasible point; the feasible point exists, but f0 is not\n";
            out += "convex, so the conclusion does not follow.\n\n";
            out += "The p(u) picture says precisely what the dual can see: d* is the value at\n";
            out += "u = 0 of the largest convex function below p. Where p dips non-convexly, the\n";
            out += "dual is blind to the dip and reports the chord instead.\n";
        }
        return out;
    }
};
