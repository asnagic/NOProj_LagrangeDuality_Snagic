//
//  ViewAllocation.h
//
//  Linear-resource allocation: maximize sum_i w_i log(1 + x_i/alpha_i) subject to a single
//  budget sum_i x_i <= B and x >= 0.
//
//  Only the budget is dualised, so there is exactly one multiplier and the dual function is
//  a curve that can simply be drawn.  The inner minimisation separates and is solved in
//  closed form, giving the water-filling rule x_i(lambda) = max(0, w_i/lambda - alpha_i).
//
//  The lower picture is the reason the rule has that name: alpha_i are the heights of the
//  basins, w_i/lambda* is the water level, and the amount allocated to i is the depth of
//  water above its floor.  Basins above the line get nothing, which is the same statement
//  as x_i = 0 for items whose marginal utility at the optimum is below the price.
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
#include <memory>
#include "PlotCanvas.h"
#include "core/AppModel.h"

class AllocParamsView : public gui::View
{
    static constexpr size_t NITEM = 4;

    gui::Label _lblHdr;
    gui::Label _lblB;
    gui::NumericEdit _b;
    gui::Label _lblCols;
    gui::Label _lblItem[NITEM];
    std::unique_ptr<gui::NumericEdit> _w[NITEM], _al[NITEM];
    gui::Label _lblReport;
    gui::TextEdit _report;
    gui::GridLayout _gl;

    core::AppModel* _pModel = nullptr;
    std::function<void()>* _pOnChanged = nullptr;
    bool _building = false;

public:
    AllocParamsView()
    : _lblHdr("max  sum_i w_i log(1 + x_i/alpha_i)   s.t.  sum_i x_i <= B,  x >= 0")
    , _lblB("Budget B:")
    , _b(td::real8, gui::LineEdit::Messages::Send)
    , _lblCols("           weight w_i        alpha_i")
    , _lblReport("Results")
    , _gl(9, 3)
    {
        for (size_t i = 0; i < NITEM; ++i)
        {
            td::String s;
            s.format("item %zu:", i + 1);
            _lblItem[i].setTitle(s);
            _w[i]  = std::make_unique<gui::NumericEdit>(td::real8, gui::LineEdit::Messages::Send);
            _al[i] = std::make_unique<gui::NumericEdit>(td::real8, gui::LineEdit::Messages::Send);
        }
        _report.setAsReadOnly();

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblHdr, 0);
        gc.appendRow(_lblB) << _b;
        gc.appendRow(_lblCols, 0);
        for (size_t i = 0; i < NITEM; ++i)
            gc.appendRow(_lblItem[i]) << *_w[i] << *_al[i];
        gc.appendRow(_lblReport, 0);
        gc.appendRow(_report, 0);

        auto fire = [this]() { pushToModel(); };
        _b.onFinishEdit(fire);
        _b.onActivate(fire);
        for (size_t i = 0; i < NITEM; ++i)
        {
            _w[i]->onFinishEdit(fire);  _w[i]->onActivate(fire);
            _al[i]->onFinishEdit(fire); _al[i]->onActivate(fire);
        }

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
        _b.setValue(td::Variant(_pModel->alloc.B), false);
        for (size_t i = 0; i < NITEM; ++i)
        {
            const double w  = (i < _pModel->alloc.w.size())     ? _pModel->alloc.w[i]     : 1.0;
            const double al = (i < _pModel->alloc.alpha.size()) ? _pModel->alloc.alpha[i] : 1.0;
            _w[i]->setValue(td::Variant(w), false);
            _al[i]->setValue(td::Variant(al), false);
        }
        _building = false;
    }

    void pushToModel()
    {
        if (_building || !_pModel)
            return;

        double v = 0.0;
        _b.getValue(v);
        _pModel->alloc.B = (v > 1e-6) ? v : 1e-6;

        _pModel->alloc.w.assign(NITEM, 1.0);
        _pModel->alloc.alpha.assign(NITEM, 1.0);
        for (size_t i = 0; i < NITEM; ++i)
        {
            double w = 1.0, al = 1.0;
            _w[i]->getValue(w);
            _al[i]->getValue(al);
            //both must stay strictly positive for the utility and the rule to make sense
            _pModel->alloc.w[i]     = (w  > 1e-6) ? w  : 1e-6;
            _pModel->alloc.alpha[i] = (al > 1e-6) ? al : 1e-6;
        }

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

class ViewAllocation : public gui::View
{
    gui::SplitterLayout _split;
    gui::VerticalLayout _vlPlots;
    PlotCanvas _plotDual;
    PlotCanvas _plotWater;
    AllocParamsView _params;
    core::AppModel* _pModel = nullptr;

public:
    ViewAllocation()
    : _split(gui::SplitterLayout::Orientation::Horizontal,
             gui::SplitterLayout::AuxiliaryCell::Second)
    , _vlPlots(2)
    {
        setMargins(0, 0, 0, 0);
        _vlPlots << _plotDual << _plotWater;
        _split.setLayout(0, _vlPlots);
        _split.setView(1, _params);
        setLayout(&_split);
    }

    void attach(core::AppModel* pModel, std::function<void()>* pOnChanged)
    {
        _pModel = pModel;
        _params.attach(pModel, pOnChanged);
    }

    void collectCanvases(std::vector<PlotCanvas*>& out, std::vector<td::String>& names)
    {
        out.push_back(&_plotDual);  names.push_back("4a_allocation_dual");
        out.push_back(&_plotWater); names.push_back("4b_water_filling");
    }

    void refresh()
    {
        if (!_pModel)
            return;
        buildDual();
        buildWater();
        _params.setReport(buildReport());
        _plotDual.reDraw();
        _plotWater.reDraw();
    }

private:
    //------------------------------------------------------------------ dual curve
    void buildDual()
    {
        const core::AllocData& ad = _pModel->alloc;
        const core::AllocSolution& s = _pModel->allocSol;

        _plotDual.clear();
        _plotDual.autoRange();
        _plotDual.setPlotTitle("Dual function  g(lambda) = inf_{x>=0} L(x,lambda)");
        _plotDual.setXLabel("lambda   (price of one unit of budget)");
        _plotDual.setYLabel("g(lambda)");

        if (!s.ok)
        {
            _plotDual.setNote("No solution for the current data.");
            return;
        }

        const double lMax = std::max(0.05, 3.0 * s.lambda);
        std::vector<double> xs, ys;
        const int N = 500;
        for (int k = 1; k <= N; ++k)
        {
            const double l = lMax * double(k) / double(N);
            const double v = core::AllocSolver::dualValue(ad, l);
            if (std::isfinite(v))
            {
                xs.push_back(l);
                ys.push_back(v);
            }
        }
        _plotDual.add(PlotCanvas::makeSeries(xs, ys, td::ColorID::SteelBlue,
                                             "g(lambda)  (concave)", 2.6f));

        PlotCanvas::RefLine rl;
        rl.value = s.pStar;
        rl.color = td::ColorID::Crimson;
        rl.pattern = td::LinePattern::Dash;
        rl.width = 1.8f;
        rl.label.format("p* = %.6f", s.pStar);
        _plotDual.add(rl);

        PlotCanvas::Marker mk;
        mk.pt = gui::Point(s.lambda, s.dStar);
        mk.color = td::ColorID::Gold;
        mk.radius = 6.0;
        mk.label.format("lambda* = %.6f", s.lambda);
        _plotDual.add(mk);

        td::String note;
        note.format("Peak touches the p* line, gap %.2e. lambda* = %.5f is the\n"
                    "marginal utility of one more unit of budget.", s.gap, s.lambda);
        _plotDual.setNote(note);
    }

    //------------------------------------------------------------------ water filling
    void buildWater()
    {
        const core::AllocData& ad = _pModel->alloc;
        const core::AllocSolution& s = _pModel->allocSol;

        _plotWater.clear();
        _plotWater.autoRange();
        _plotWater.setPlotTitle("Water filling: floors alpha_i, level w_i/lambda*, depth x_i");
        _plotWater.setXLabel("item");
        _plotWater.setYLabel("alpha_i  and  alpha_i + x_i");
        _plotWater.showLegend(true);

        if (!s.ok || ad.n() == 0)
            return;

        const size_t n = ad.n();
        double top = 0.0;
        for (size_t i = 0; i < n; ++i)
            top = std::max(top, ad.alpha[i] + s.x[i]);
        top = std::max(top, 1.0) * 1.25;

        _plotWater.setXRange(0.3, double(n) + 0.7);
        _plotWater.setYRange(0.0, top);

        const double hw = 0.34;     //half width of a bar
        for (size_t i = 0; i < n; ++i)
        {
            const double xc = double(i + 1);

            //the floor of the basin
            PlotCanvas::Polygon floorBar;
            floorBar.fill = td::ColorID::SlateGray;
            floorBar.line = td::ColorID::SlateGray;
            floorBar.lineWidth = 1.0f;
            floorBar.pts = { gui::Point(xc - hw, 0.0), gui::Point(xc + hw, 0.0),
                             gui::Point(xc + hw, ad.alpha[i]), gui::Point(xc - hw, ad.alpha[i]) };
            floorBar.name = (i == 0) ? td::String("alpha_i  (floor)") : td::String();
            floorBar.inLegend = (i == 0);
            _plotWater.add(floorBar);

            //the water on top of it
            if (s.x[i] > 1e-12)
            {
                PlotCanvas::Polygon waterBar;
                waterBar.fill = td::ColorID::SteelBlue;
                waterBar.line = td::ColorID::MidnightBlue;
                waterBar.lineWidth = 1.2f;
                waterBar.pts = { gui::Point(xc - hw, ad.alpha[i]),
                                 gui::Point(xc + hw, ad.alpha[i]),
                                 gui::Point(xc + hw, ad.alpha[i] + s.x[i]),
                                 gui::Point(xc - hw, ad.alpha[i] + s.x[i]) };
                waterBar.name = (i == 0) ? td::String("x_i  (allocation)") : td::String();
                waterBar.inLegend = (i == 0);
                _plotWater.add(waterBar);
            }

            //per-item water level w_i/lambda*, drawn as a short segment over the bar
            if (s.lambda > 1e-12)
            {
                const double lvl = ad.w[i] / s.lambda;
                std::vector<double> lx = { xc - hw - 0.06, xc + hw + 0.06 };
                std::vector<double> ly = { lvl, lvl };
                auto ser = PlotCanvas::makeSeries(lx, ly, td::ColorID::Crimson,
                                                  (i == 0) ? td::String("w_i / lambda*  (level)")
                                                           : td::String(),
                                                  2.2f);
                ser.inLegend = (i == 0);
                _plotWater.add(ser);
            }

            PlotCanvas::Marker lab;
            lab.pt = gui::Point(xc, ad.alpha[i] + s.x[i]);
            lab.color = td::ColorID::Gold;
            lab.radius = 3.0;
            lab.label.format("x%zu = %.3f", i + 1, s.x[i]);
            _plotWater.add(lab);
        }

        td::String note;
        note.format("Items whose floor alpha_i already reaches above their level w_i/lambda* get\n"
                    "nothing. Budget used %.4f of %.4f.", s.used, ad.B);
        _plotWater.setNote(note);
    }

    //------------------------------------------------------------------ report
    td::String buildReport() const
    {
        const core::AllocData& ad = _pModel->alloc;
        const core::AllocSolution& s = _pModel->allocSol;
        td::String out, line;

        if (!s.ok)
            return "No solution for the current data.";

        line.format("lambda* = %.8f\n", s.lambda);
        out += line;
        line.format("utility = %.8f\n", s.utility);
        out += line;
        line.format("p* = %.8f\nd* = %.8f\ngap = %.2e\n\n", s.pStar, s.dStar, s.gap);
        out += line;

        out += "allocation\n";
        for (size_t i = 0; i < ad.n(); ++i)
        {
            const double marg = ad.w[i] / (ad.alpha[i] + s.x[i]);   //dUtility/dx_i at x*
            line.format("  x%zu = %8.5f   marginal utility = %8.5f  %s\n",
                        i + 1, s.x[i], marg,
                        (s.x[i] > 1e-9) ? "" : "(shut out)");
            out += line;
        }
        out += "\n";
        out += "Every item that receives something has marginal utility exactly lambda*: at the\n";
        out += "optimum all funded items are equally worth funding. Items that receive nothing\n";
        out += "have marginal utility below lambda* even at x_i = 0 - they are not worth the\n";
        out += "price of the budget.\n\n";

        const double h = 1e-5;
        const double dp = (core::AllocSolver::perturbedOptimum(ad, h) -
                           core::AllocSolver::perturbedOptimum(ad, -h)) / (2.0 * h);
        line.format("dp*/dB numerically = %.8f\n-lambda*           = %.8f\n",
                    dp, -s.lambda);
        out += line;
        line.format("difference         = %.2e\n", std::fabs(dp + s.lambda));
        out += line;
        return out;
    }
};
