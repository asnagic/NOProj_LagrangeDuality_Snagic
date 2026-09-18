//
//  ViewQP.h
//
//  Primal tab for the quadratic program: editable data on the right, geometry on the left.
//
//  The picture shows the objective as filled contour bands, the feasible polyhedron
//  {x : Ax <= b}, the unconstrained minimiser, and the constrained optimum x*.  Active
//  constraints are drawn heavier than inactive ones, which is the geometric face of
//  complementary slackness: only the constraints the optimum leans on carry a price.
//
#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/NumericEdit.h>
#include <gui/ComboBox.h>
#include <gui/Button.h>
#include <gui/TextEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/VerticalLayout.h>
#include <gui/HorizontalLayout.h>
#include <gui/SplitterLayout.h>
#include <functional>
#include <memory>
#include "PlotCanvas.h"
#include "core/AppModel.h"

//----------------------------------------------------------------------- parameter panel
class QPParamsView : public gui::View
{
    static constexpr size_t MAXM = 3;

    gui::Label _lblHdrObj;
    gui::Label _lblP11, _lblP12, _lblP22, _lblQ1, _lblQ2;
    gui::NumericEdit _p11, _p12, _p22, _q1, _q2;

    gui::Label _lblHdrCon;
    gui::Label _lblM;
    gui::ComboBox _cbM;
    gui::Label _lblRow[MAXM];
    //NumericEdit has no public default constructor, so the rows are heap allocated
    std::unique_ptr<gui::NumericEdit> _a1[MAXM], _a2[MAXM], _bb[MAXM];

    gui::HorizontalLayout _hlButtons;
    gui::Button _btnReset;
    gui::Label _lblResults;
    gui::TextEdit _results;
    gui::GridLayout _gl;

    core::AppModel* _pModel = nullptr;
    std::function<void()>* _pOnChanged = nullptr;

    //stored so switching the constraint count does not lose the rows
    double _rowA1[MAXM] = { 1.0, -1.0, 0.0 };
    double _rowA2[MAXM] = { 1.0,  2.0, -1.0 };
    double _rowB [MAXM] = { 2.0,  4.0, 0.5 };

public:
    QPParamsView()
    : _lblHdrObj("Objective   f0(x) = 1/2 x'Px + q'x")
    , _lblP11("P11:"), _lblP12("P12 = P21:"), _lblP22("P22:")
    , _lblQ1("q1:"), _lblQ2("q2:")
    , _p11(td::real8, gui::LineEdit::Messages::Send)
    , _p12(td::real8, gui::LineEdit::Messages::Send)
    , _p22(td::real8, gui::LineEdit::Messages::Send)
    , _q1 (td::real8, gui::LineEdit::Messages::Send)
    , _q2 (td::real8, gui::LineEdit::Messages::Send)
    , _lblHdrCon("Constraints   a_i'x <= b_i")
    , _lblM("Number of constraints:")
    , _hlButtons(2)
    , _btnReset("Reset to defaults")
    , _lblResults("Results")
    , _gl(14, 4)
    {
        for (size_t i = 0; i < MAXM; ++i)
        {
            td::String s;
            s.format("row %zu:   a%zu1, a%zu2, b%zu:", i + 1, i + 1, i + 1, i + 1);
            _lblRow[i].setTitle(s);
            _a1[i] = std::make_unique<gui::NumericEdit>(td::real8, gui::LineEdit::Messages::Send);
            _a2[i] = std::make_unique<gui::NumericEdit>(td::real8, gui::LineEdit::Messages::Send);
            _bb[i] = std::make_unique<gui::NumericEdit>(td::real8, gui::LineEdit::Messages::Send);
        }

        _cbM.addItem("1");
        _cbM.addItem("2");
        _cbM.addItem("3");
        _cbM.selectIndex(1);

        _results.setAsReadOnly();
        _btnReset.setType(gui::Button::Type::Destructive);

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblHdrObj, 0);
        gc.appendRow(_lblP11) << _p11 << _lblP12 << _p12;
        gc.appendRow(_lblP22) << _p22;
        gc.appendRow(_lblQ1)  << _q1  << _lblQ2  << _q2;
        gc.appendRow(_lblHdrCon, 0);
        gc.appendRow(_lblM)   << _cbM;
        for (size_t i = 0; i < MAXM; ++i)
            gc.appendRow(_lblRow[i]) << *_a1[i] << *_a2[i] << *_bb[i];
        _hlButtons << _btnReset;
        _hlButtons.appendSpacer();
        gc.appendRow(_hlButtons, 0);
        gc.appendRow(_lblResults, 0);
        gc.appendRow(_results, 0);

        auto fire = [this]() { pushToModel(); };
        _p11.onFinishEdit(fire); _p12.onFinishEdit(fire); _p22.onFinishEdit(fire);
        _q1.onFinishEdit(fire);  _q2.onFinishEdit(fire);
        _p11.onActivate(fire);   _p12.onActivate(fire);   _p22.onActivate(fire);
        _q1.onActivate(fire);    _q2.onActivate(fire);
        for (size_t i = 0; i < MAXM; ++i)
        {
            _a1[i]->onFinishEdit(fire); _a2[i]->onFinishEdit(fire); _bb[i]->onFinishEdit(fire);
            _a1[i]->onActivate(fire);   _a2[i]->onActivate(fire);   _bb[i]->onActivate(fire);
        }
        _cbM.onChangedSelection(fire);

        _btnReset.onClick([this]()
        {
            if (!_pModel)
                return;
            _pModel->resetDefaults();
            _rowA1[0]=1;  _rowA2[0]=1;  _rowB[0]=2;
            _rowA1[1]=-1; _rowA2[1]=2;  _rowB[1]=4;
            _rowA1[2]=0;  _rowA2[2]=-1; _rowB[2]=0.5;
            _cbM.selectIndex(1, false);
            pullFromModel();
            _pModel->recompute();
            if (_pOnChanged) (*_pOnChanged)();
        });

        setLayout(&_gl);
    }

    void attach(core::AppModel* pModel, std::function<void()>* pOnChanged)
    {
        _pModel = pModel;
        _pOnChanged = pOnChanged;
        pullFromModel();
    }

    //model -> controls
    void pullFromModel()
    {
        if (!_pModel)
            return;
        const core::QPData& qp = _pModel->qp;
        _p11.setValue(td::Variant(qp.P(0, 0)), false);
        _p12.setValue(td::Variant(qp.P(0, 1)), false);
        _p22.setValue(td::Variant(qp.P(1, 1)), false);
        _q1.setValue(td::Variant(qp.q[0]), false);
        _q2.setValue(td::Variant(qp.q[1]), false);

        for (size_t i = 0; i < qp.m() && i < MAXM; ++i)
        {
            _rowA1[i] = qp.A(i, 0);
            _rowA2[i] = qp.A(i, 1);
            _rowB[i]  = qp.b[i];
        }
        for (size_t i = 0; i < MAXM; ++i)
        {
            _a1[i]->setValue(td::Variant(_rowA1[i]), false);
            _a2[i]->setValue(td::Variant(_rowA2[i]), false);
            _bb[i]->setValue(td::Variant(_rowB[i]), false);
        }
        updateEnabledRows();
    }

    //controls -> model, then recompute and notify
    void pushToModel()
    {
        if (!_pModel)
            return;
        core::QPData& qp = _pModel->qp;

        double v = 0.0;
        _p11.getValue(v); qp.P(0, 0) = v;
        _p12.getValue(v); qp.P(0, 1) = v; qp.P(1, 0) = v;
        _p22.getValue(v); qp.P(1, 1) = v;
        _q1.getValue(v);  qp.q[0] = v;
        _q2.getValue(v);  qp.q[1] = v;

        for (size_t i = 0; i < MAXM; ++i)
        {
            _a1[i]->getValue(_rowA1[i]);
            _a2[i]->getValue(_rowA2[i]);
            _bb[i]->getValue(_rowB[i]);
        }

        const size_t m = size_t(_cbM.getSelectedIndex()) + 1;
        qp.A.resize(m, 2);
        qp.b.assign(m, 0.0);
        for (size_t i = 0; i < m; ++i)
        {
            qp.A(i, 0) = _rowA1[i];
            qp.A(i, 1) = _rowA2[i];
            qp.b[i] = _rowB[i];
        }
        if (_pModel->sensConstraint >= m)
            _pModel->sensConstraint = 0;

        updateEnabledRows();
        _pModel->recompute();
        if (_pOnChanged)
            (*_pOnChanged)();
    }

    void setReport(const td::String& txt)
    {
        _results.clean();
        _results.appendString(txt);
    }

private:
    void updateEnabledRows()
    {
        const size_t m = size_t(_cbM.getSelectedIndex()) + 1;
        for (size_t i = 0; i < MAXM; ++i)
        {
            const bool on = (i < m);
            if (on)
            {
                _a1[i]->enable(); _a2[i]->enable(); _bb[i]->enable();
            }
            else
            {
                _a1[i]->disable(); _a2[i]->disable(); _bb[i]->disable();
            }
        }
    }
};

//----------------------------------------------------------------------- QP tab
class ViewQP : public gui::View
{
    gui::SplitterLayout _split;
    PlotCanvas _plot;
    QPParamsView _params;
    core::AppModel* _pModel = nullptr;

public:
    ViewQP()
    : _split(gui::SplitterLayout::Orientation::Horizontal,
             gui::SplitterLayout::AuxiliaryCell::Second)
    {
        setMargins(0, 0, 0, 0);
        _split.setContent(_plot, _params);
        setLayout(&_split);
    }

    void attach(core::AppModel* pModel, std::function<void()>* pOnChanged)
    {
        _pModel = pModel;
        _params.attach(pModel, pOnChanged);
    }

    void collectCanvases(std::vector<PlotCanvas*>& out, std::vector<td::String>& names)
    {
        out.push_back(&_plot);
        names.push_back("1_primal_qp");
    }

    void refresh()
    {
        if (!_pModel)
            return;
        buildPlot();
        _params.setReport(buildReport());
        _plot.reDraw();
    }

private:
    //-------------------------------------------------------------------- geometry
    void buildPlot()
    {
        const core::QPData& qp = _pModel->qp;
        const core::QPSolution& s = _pModel->qpSol;

        _plot.clear();
        _plot.autoRange();
        _plot.setPlotTitle("Primal problem: objective contours, feasible set and x*");
        _plot.setXLabel("x1");
        _plot.setYLabel("x2");
        _plot.setEqualAspect(true);

        if (!s.ok)
        {
            _plot.setNote("P is not positive definite - the objective is not strictly convex, "
                          "so the closed-form inner minimisation of the Lagrangian does not apply.");
            _plot.setXRange(-1, 1);
            _plot.setYRange(-1, 1);
            return;
        }

        //window centred on the optimum, wide enough to show the constraints
        double cx = s.x[0], cy = s.x[1];
        double half = 3.0;
        for (size_t i = 0; i < qp.m(); ++i)
        {
            half = std::max(half, std::fabs(qp.b[i]) + 1.5);
        }
        half = std::min(half, 12.0);
        const double xlo = cx - half, xhi = cx + half;
        const double ylo = cy - half, yhi = cy + half;
        _plot.setXRange(xlo, xhi);
        _plot.setYRange(ylo, yhi);

        //The field is sampled over a wider box than the axis range, because equal-aspect
        //widens whichever direction is short at draw time and the map must still cover it.
        const double fex = 2.4, fey = 1.6;
        PlotCanvas::Field f;
        f.nx = 130; f.ny = 110;
        f.x0 = cx - half * fex; f.x1 = cx + half * fex;
        f.y0 = cy - half * fey; f.y1 = cy + half * fey;
        f.v.resize(f.nx * f.ny);
        double vmin = 1e300, vmax = -1e300;
        for (size_t iy = 0; iy < f.ny; ++iy)
        {
            for (size_t ix = 0; ix < f.nx; ++ix)
            {
                la::Vec x(2);
                x[0] = f.x0 + (f.x1 - f.x0) * double(ix) / double(f.nx - 1);
                x[1] = f.y0 + (f.y1 - f.y0) * double(iy) / double(f.ny - 1);
                const double v = core::QPSolver::f0(qp, x);
                f.v[iy * f.nx + ix] = v;
                vmin = std::min(vmin, v);
                vmax = std::max(vmax, v);
            }
        }
        //square-root spacing puts more bands near the minimum, where the picture matters
        const int nb = 13;
        for (int k = 0; k <= nb; ++k)
        {
            const double t = double(k) / double(nb);
            f.levels.push_back(vmin + (vmax - vmin) * t * t);
        }
        _plot.setField(f);

        //feasible polygon
        auto poly = core::feasiblePolygon(qp, f.x0, f.x1, f.y0, f.y1);
        if (poly.size() >= 3)
        {
            PlotCanvas::Polygon p;
            p.filled = false;           //keep the objective contours visible inside it
            p.hatch = true;
            p.line = td::ColorID::White;
            p.lineWidth = 2.2f;
            p.name = "feasible set";
            p.inLegend = true;
            for (const auto& q : poly)
                p.pts.emplace_back(q.first, q.second);
            _plot.add(p);
        }

        //constraint boundary lines: active ones solid and heavy, inactive dashed
        for (size_t i = 0; i < qp.m(); ++i)
        {
            const double a0 = qp.A(i, 0), a1 = qp.A(i, 1);
            const double norm = std::sqrt(a0 * a0 + a1 * a1);
            if (norm < 1e-12)
                continue;

            const bool active = (i < s.slack.size()) && (std::fabs(s.slack[i]) < 1e-7);

            std::vector<double> xs, ys;
            if (std::fabs(a1) > std::fabs(a0))
            {
                for (int k = 0; k <= 60; ++k)
                {
                    const double x = xlo + (xhi - xlo) * double(k) / 60.0;
                    xs.push_back(x);
                    ys.push_back((qp.b[i] - a0 * x) / a1);
                }
            }
            else
            {
                for (int k = 0; k <= 60; ++k)
                {
                    const double y = ylo + (yhi - ylo) * double(k) / 60.0;
                    xs.push_back((qp.b[i] - a1 * y) / a0);
                    ys.push_back(y);
                }
            }

            td::String nm;
            nm.format("g%zu %s  (lambda%zu = %.3f)", i + 1,
                      active ? "active" : "inactive", i + 1,
                      i < s.lambda.size() ? s.lambda[i] : 0.0);
            auto ser = PlotCanvas::makeSeries(xs, ys,
                                              active ? td::ColorID::Crimson : td::ColorID::Gray,
                                              nm,
                                              active ? 3.0f : 1.6f,
                                              active ? td::LinePattern::Solid
                                                     : td::LinePattern::Dash);
            _plot.add(ser);
        }

        //unconstrained minimiser of f0
        la::Mat L;
        if (la::cholesky(qp.P, L))
        {
            la::Vec negq(2);
            negq[0] = -qp.q[0];
            negq[1] = -qp.q[1];
            la::Vec xu;
            la::cholSolve(L, negq, xu);

            PlotCanvas::Marker mu;
            mu.pt = gui::Point(xu[0], xu[1]);
            mu.color = td::ColorID::DarkSlateBlue;
            mu.radius = 4.5;
            mu.label = "unconstrained min";
            mu.labelAbove = false;
            mu.labelDy = 14.0;      //x* usually sits close by; keep the two labels apart
            _plot.add(mu);
        }

        PlotCanvas::Marker mx;
        mx.pt = gui::Point(s.x[0], s.x[1]);
        mx.color = td::ColorID::Yellow;
        mx.radius = 6.5;
        mx.label.format("x* = (%.4f, %.4f)", s.x[0], s.x[1]);
        mx.labelDy = -8.0;
        _plot.add(mx);
    }

    //-------------------------------------------------------------------- report
    td::String buildReport() const
    {
        const core::QPData& qp = _pModel->qp;
        const core::QPSolution& s = _pModel->qpSol;
        td::String out, line;

        if (!s.spd)
        {
            out += "P is NOT positive definite.\n\n";
            out += "The dual function is built by minimising the Lagrangian over x. That inner\n";
            out += "problem is unbounded below unless P is positive definite, so g(lambda) = -inf\n";
            out += "and the dual carries no information. Edit P and try again.\n";
            return out;
        }

        line.format("Slater's condition: %s\n",
                    s.slater ? "satisfied (a strictly feasible point exists)"
                             : "NOT satisfied - no strictly feasible point found");
        out += line;
        out += "\n";

        line.format("p*  (primal, from the dual optimum)  = %.10f\n", s.pStar);
        out += line;
        if (s.barrierOk)
        {
            line.format("p*  (primal, log-barrier method)     = %.10f\n", s.pStarBarrier);
            out += line;
            line.format("     agreement                       = %.2e\n",
                        std::fabs(s.pStar - s.pStarBarrier));
            out += line;
        }
        else
        {
            out += "p*  (primal, log-barrier method)     = not available\n";
        }
        line.format("d*  (dual)                           = %.10f\n", s.dStar);
        out += line;
        line.format("duality gap  p* - d*                 = %.3e\n", s.gap);
        out += line;
        out += "\n";
        out += (std::fabs(s.gap) < 1e-7)
             ? "Strong duality holds, as Slater's condition predicts for a convex QP.\n"
             : "A non-zero gap appeared - check that the data really is convex and feasible.\n";
        out += "\n";

        line.format("x*      = (%.6f, %.6f)\n", s.x[0], s.x[1]);
        out += line;
        out += "lambda* = (";
        for (size_t i = 0; i < s.lambda.size(); ++i)
        {
            line.format("%s%.6f", i ? ", " : "", s.lambda[i]);
            out += line;
        }
        out += ")\n\n";

        out += "Complementary slackness   lambda_i * (b_i - a_i'x*)\n";
        for (size_t i = 0; i < qp.m(); ++i)
        {
            line.format("  i=%zu  lambda=%9.6f  slack=%9.6f  product=%.2e  %s\n",
                        i + 1, s.lambda[i], s.slack[i], s.lambda[i] * s.slack[i],
                        std::fabs(s.slack[i]) < 1e-7 ? "[active]" : "[inactive]");
            out += line;
        }
        out += "\n";
        out += "Each multiplier is the shadow price of its constraint: relaxing b_i by one unit\n";
        out += "changes p* by -lambda_i. Inactive constraints price at zero - see the\n";
        out += "Sensitivity tab for the numerical check.\n";
        return out;
    }
};
