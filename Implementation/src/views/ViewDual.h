//
//  ViewDual.h
//
//  The dual function of the quadratic program, drawn over the space of multipliers.
//
//    m = 1  a curve g(lambda) with the horizontal line p*.  The curve never rises above
//           that line - that is weak duality, and the vertical distance at lambda* is the
//           duality gap.
//    m = 2  a filled contour map of g(lambda1, lambda2) over the non-negative quadrant.
//           g is concave, so the map is a single hill whose summit is lambda*.
//    m = 3  the same map, taken as a slice through lambda* with the third multiplier held
//           at its optimal value.
//
#pragma once
#include <gui/View.h>
#include <gui/VerticalLayout.h>
#include "PlotCanvas.h"
#include "core/AppModel.h"

class ViewDual : public gui::View
{
    PlotCanvas _plot;
    gui::VerticalLayout _vl;
    core::AppModel* _pModel = nullptr;

public:
    ViewDual()
    : _vl(1)
    {
        setMargins(0, 0, 0, 0);
        _vl << _plot;
        setLayout(&_vl);
    }

    void attach(core::AppModel* pModel) { _pModel = pModel; }

    void collectCanvases(std::vector<PlotCanvas*>& out, std::vector<td::String>& names)
    {
        out.push_back(&_plot);
        names.push_back("2_dual_function");
    }

    void refresh()
    {
        if (!_pModel)
            return;
        build();
        _plot.reDraw();
    }

private:
    void build()
    {
        const core::QPData& qp = _pModel->qp;
        const core::QPSolution& s = _pModel->qpSol;

        _plot.clear();
        _plot.autoRange();
        _plot.setEqualAspect(false);

        if (!s.ok || !s.spd)
        {
            _plot.setPlotTitle("Dual function g(lambda)");
            _plot.setNote("P is not positive definite, so inf_x L(x,lambda) = -infinity for every\n"
                          "lambda and the dual function carries no information.");
            _plot.setXRange(0, 1);
            _plot.setYRange(0, 1);
            return;
        }

        la::Mat L;
        la::cholesky(qp.P, L);
        const size_t m = qp.m();

        double lamMax = 1.0;
        for (double v : s.lambda)
            lamMax = std::max(lamMax, v);
        lamMax = std::max(2.0, 2.6 * lamMax);

        if (m == 1)
            buildCurve(qp, L, s, lamMax);
        else
            buildField(qp, L, s, lamMax);
    }

    //------------------------------------------------------------------ m = 1
    void buildCurve(const core::QPData& qp, const la::Mat& L,
                    const core::QPSolution& s, double lamMax)
    {
        std::vector<double> xs, ys;
        const int N = 400;
        for (int k = 0; k <= N; ++k)
        {
            const double l = lamMax * double(k) / double(N);
            la::Vec lam(1, l);
            xs.push_back(l);
            ys.push_back(core::QPSolver::dualValue(qp, L, lam));
        }

        _plot.setPlotTitle("Dual function  g(lambda) = inf_x L(x,lambda)");
        _plot.setXLabel("lambda");
        _plot.setYLabel("g(lambda)");
        _plot.add(PlotCanvas::makeSeries(xs, ys, td::ColorID::SteelBlue,
                                         "g(lambda)  (concave)", 2.6f));

        PlotCanvas::RefLine rl;
        rl.vertical = false;
        rl.value = s.pStar;
        rl.color = td::ColorID::Crimson;
        rl.pattern = td::LinePattern::Dash;
        rl.width = 1.8f;
        rl.label.format("p* = %.6f   (g never rises above this line: weak duality)", s.pStar);
        _plot.add(rl);

        PlotCanvas::Marker mk;
        mk.pt = gui::Point(s.lambda[0], s.dStar);
        mk.color = td::ColorID::Gold;
        mk.radius = 6.0;
        mk.label.format("lambda* = %.5f,  d* = %.6f", s.lambda[0], s.dStar);
        _plot.add(mk);

        td::String note;
        note.format("d* = max g(lambda) = %.6f,  gap = %.2e:\n"
                    "the curve touches the p* line, so strong duality holds.",
                    s.dStar, s.gap);
        _plot.setNote(note);
    }

    //------------------------------------------------------------------ m >= 2
    void buildField(const core::QPData& qp, const la::Mat& L,
                    const core::QPSolution& s, double lamMax)
    {
        const size_t m = qp.m();

        //A strip of negative lambda is sampled as well. The dual problem forbids it, but
        //seeing where the unconstrained hill would peak is exactly what explains a
        //multiplier pinned at zero.
        const double lamLo = -0.12 * lamMax;
        PlotCanvas::Field f;
        f.nx = 120; f.ny = 120;
        f.x0 = lamLo; f.x1 = lamMax;
        f.y0 = lamLo; f.y1 = lamMax;
        f.v.resize(f.nx * f.ny);

        double vmin = 1e300, vmax = -1e300;
        for (size_t iy = 0; iy < f.ny; ++iy)
        {
            for (size_t ix = 0; ix < f.nx; ++ix)
            {
                la::Vec lam(m, 0.0);
                lam[0] = f.x0 + (f.x1 - f.x0) * double(ix) / double(f.nx - 1);
                lam[1] = f.y0 + (f.y1 - f.y0) * double(iy) / double(f.ny - 1);
                for (size_t i = 2; i < m; ++i)
                    lam[i] = s.lambda[i];      //slice through the dual optimum
                const double v = core::QPSolver::dualValue(qp, L, lam);
                f.v[iy * f.nx + ix] = v;
                vmin = std::min(vmin, v);
                vmax = std::max(vmax, v);
            }
        }

        //bands packed towards the top, where the summit and the level sets are
        const int nb = 14;
        for (int k = 0; k <= nb; ++k)
        {
            const double t = double(k) / double(nb);
            const double tt = 1.0 - (1.0 - t) * (1.0 - t);
            f.levels.push_back(vmin + (vmax - vmin) * tt);
        }
        _plot.setField(f);
        _plot.setXRange(f.x0, f.x1);
        _plot.setYRange(f.y0, f.y1);

        td::String title;
        if (m == 2)
            title = "Dual function  g(lambda1, lambda2)  on the non-negative quadrant";
        else
            title.format("Dual function  g(lambda1, lambda2)  -  slice with lambda3 = %.4f",
                         s.lambda[2]);
        _plot.setPlotTitle(title);
        _plot.setXLabel("lambda1");
        _plot.setYLabel("lambda2");

        PlotCanvas::Marker mk;
        mk.pt = gui::Point(s.lambda[0], s.lambda[1]);
        mk.color = td::ColorID::Yellow;
        mk.radius = 7.0;
        mk.label.format("lambda* = (%.4f, %.4f)", s.lambda[0], s.lambda[1]);
        _plot.add(mk);

        //mark the axes where a multiplier is pinned at zero: an inactive constraint
        td::String boundary;
        for (size_t i = 0; i < 2 && i < m; ++i)
        {
            if (s.lambda[i] < 1e-9)
            {
                PlotCanvas::RefLine rl;
                rl.vertical = (i == 0);
                rl.value = 0.0;
                (void)0;
                rl.color = td::ColorID::Crimson;
                rl.pattern = td::LinePattern::Solid;
                rl.width = 2.4f;
                _plot.add(rl);

                td::String bl;
                bl.format("\nlambda%zu* = 0 (red axis): the summit sits on the boundary,"
                          " so constraint %zu is inactive.", i + 1, i + 1);
                boundary += bl;
            }
        }

        td::String note;
        note.format("g is concave: the map is one hill, summit d* = %.6f, gap %.2e.\n"
                    "Every point on it is a lower bound on p* - weak duality.",
                    s.dStar, s.gap);
        note += boundary;
        _plot.setNote(note);
        _plot.showLegend(false);
    }
};
