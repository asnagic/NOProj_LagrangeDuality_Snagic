//
//  ViewSensitivity.h
//
//  Shadow prices, checked numerically.
//
//  Perturb one constraint to a_i'x <= b_i + u and re-solve for a range of u.  The theory
//  says the optimal value moves at rate dp*/du = -lambda_i*, so the analytic line
//  p*(0) - lambda_i* u must be tangent to the computed curve at u = 0.  Drawing both and
//  reporting the difference of slopes turns the shadow-price statement into something the
//  user can watch hold - or fail, when the active set changes and p* develops a kink.
//
#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/ComboBox.h>
#include <gui/NumericEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/VerticalLayout.h>
#include <gui/SplitterLayout.h>
#include <gui/TextEdit.h>
#include <functional>
#include "PlotCanvas.h"
#include "core/AppModel.h"

class SensParamsView : public gui::View
{
    gui::Label _lblHdr;
    gui::Label _lblWhich;
    gui::ComboBox _cbWhich;
    gui::Label _lblRange;
    gui::NumericEdit _range;
    gui::Label _lblReport;
    gui::TextEdit _report;
    gui::GridLayout _gl;

    core::AppModel* _pModel = nullptr;
    std::function<void()>* _pOnChanged = nullptr;
    bool _building = false;

public:
    SensParamsView()
    : _lblHdr("Perturbation   a_i'x <= b_i + u")
    , _lblWhich("Constraint i:")
    , _lblRange("Sweep |u| up to:")
    , _range(td::real8, gui::LineEdit::Messages::Send)
    , _lblReport("Shadow price check")
    , _gl(5, 2)
    {
        _report.setAsReadOnly();

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblHdr, 0);
        gc.appendRow(_lblWhich) << _cbWhich;
        gc.appendRow(_lblRange) << _range;
        gc.appendRow(_lblReport, 0);
        gc.appendRow(_report, 0);

        _cbWhich.onChangedSelection([this]()
        {
            if (_building || !_pModel)
                return;
            const int idx = _cbWhich.getSelectedIndex();
            if (idx >= 0)
                _pModel->sensConstraint = size_t(idx);
            if (_pOnChanged) (*_pOnChanged)();
        });

        auto fire = [this]()
        {
            if (_building || !_pModel)
                return;
            double v = 0.0;
            _range.getValue(v);
            if (v < 1e-6) v = 1e-6;
            if (v > 100.0) v = 100.0;
            _pModel->sensRange = v;
            if (_pOnChanged) (*_pOnChanged)();
        };
        _range.onFinishEdit(fire);
        _range.onActivate(fire);

        setLayout(&_gl);
    }

    void attach(core::AppModel* pModel, std::function<void()>* pOnChanged)
    {
        _pModel = pModel;
        _pOnChanged = pOnChanged;
    }

    //Rebuilds the constraint list, which changes when the user edits the QP.
    void syncControls()
    {
        if (!_pModel)
            return;
        _building = true;
        _cbWhich.clean();
        for (size_t i = 0; i < _pModel->qp.m(); ++i)
        {
            td::String s;
            s.format("constraint %zu", i + 1);
            _cbWhich.addItem(s);
        }
        if (_pModel->sensConstraint >= _pModel->qp.m())
            _pModel->sensConstraint = 0;
        _cbWhich.selectIndex(int(_pModel->sensConstraint), false);
        _range.setValue(td::Variant(_pModel->sensRange), false);
        _building = false;
    }

    void setReport(const td::String& txt)
    {
        _report.clean();
        _report.appendString(txt);
    }
};

class ViewSensitivity : public gui::View
{
    gui::SplitterLayout _split;
    PlotCanvas _plot;
    SensParamsView _params;
    core::AppModel* _pModel = nullptr;

public:
    ViewSensitivity()
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
        names.push_back("3_sensitivity");
    }

    void refresh()
    {
        if (!_pModel)
            return;
        _params.syncControls();
        build();
        _plot.reDraw();
    }

private:
    void build()
    {
        const core::QPSolution& s = _pModel->qpSol;
        const size_t idx = _pModel->sensConstraint;

        _plot.clear();
        _plot.autoRange();
        _plot.setEqualAspect(false);
        _plot.setXLabel("u   (relaxation of b_i)");
        _plot.setYLabel("p*(u)");

        if (!s.ok || !s.spd || idx >= _pModel->qp.m())
        {
            _plot.setPlotTitle("Sensitivity of p* to the constraint level");
            _plot.setNote("No usable solution for the current data.");
            _params.setReport("No usable solution for the current data.");
            _plot.setXRange(-1, 1);
            _plot.setYRange(-1, 1);
            return;
        }

        const double R = _pModel->sensRange;
        const int N = 240;

        std::vector<double> us, ps, ts;
        for (int k = 0; k <= N; ++k)
        {
            const double u = -R + 2.0 * R * double(k) / double(N);
            double p = 0.0;
            if (core::QPSolver::perturbedOptimum(_pModel->qp, idx, u, p))
            {
                us.push_back(u);
                ps.push_back(p);
                ts.push_back(s.pStar - s.lambda[idx] * u);   //first-order prediction
            }
        }

        td::String title;
        title.format("Sensitivity: p*(u) for constraint %zu, and the shadow-price tangent",
                     idx + 1);
        _plot.setPlotTitle(title);

        _plot.add(PlotCanvas::makeSeries(us, ps, td::ColorID::SteelBlue,
                                         "p*(u), recomputed exactly", 2.8f));

        td::String tname;
        tname.format("p* - lambda%zu* u   (predicted)", idx + 1);
        _plot.add(PlotCanvas::makeSeries(us, ts, td::ColorID::Crimson, tname, 2.0f,
                                         td::LinePattern::Dash));

        PlotCanvas::Marker mk;
        mk.pt = gui::Point(0.0, s.pStar);
        mk.color = td::ColorID::Gold;
        mk.radius = 6.0;
        mk.label.format("u = 0,  p* = %.6f", s.pStar);
        _plot.add(mk);

        PlotCanvas::RefLine rl;
        rl.vertical = true;
        rl.value = 0.0;
        rl.color = td::ColorID::Gray;
        rl.pattern = td::LinePattern::Dash;
        _plot.add(rl);

        //central difference against the multiplier
        const double h = std::min(1e-4, 0.01 * R);
        double pp = 0.0, pm = 0.0;
        bool okp = core::QPSolver::perturbedOptimum(_pModel->qp, idx,  h, pp);
        bool okm = core::QPSolver::perturbedOptimum(_pModel->qp, idx, -h, pm);
        const double numeric = (okp && okm) ? (pp - pm) / (2.0 * h)
                                            : std::numeric_limits<double>::quiet_NaN();

        td::String note, rep, line;
        note.format("dp*/du  numerically = %.8f\n-lambda%zu*             = %.8f\n"
                    "difference           = %.2e",
                    numeric, idx + 1, -s.lambda[idx],
                    std::fabs(numeric + s.lambda[idx]));
        _plot.setNote(note);

        line.format("Constraint %zu\n\n", idx + 1);
        rep += line;
        line.format("lambda%zu*            = %.8f\n", idx + 1, s.lambda[idx]);
        rep += line;
        line.format("slack b%zu - a%zu'x*    = %.8f\n", idx + 1, idx + 1, s.slack[idx]);
        rep += line;
        rep += "\n";
        line.format("dp*/du (central diff) = %.8f\n", numeric);
        rep += line;
        line.format("-lambda%zu*            = %.8f\n", idx + 1, -s.lambda[idx]);
        rep += line;
        line.format("difference            = %.2e\n\n", std::fabs(numeric + s.lambda[idx]));
        rep += line;

        if (s.lambda[idx] > 1e-9)
        {
            rep += "The constraint is active, so it carries a positive price: relaxing b by one\n";
            rep += "unit lowers the optimal cost by lambda*. The dashed tangent matches the curve\n";
            rep += "near u = 0, and bends away once the active set changes.\n";
        }
        else
        {
            rep += "The multiplier is zero, so this constraint is not binding. Moving b has no\n";
            rep += "first-order effect and p*(u) is flat near u = 0 - until u is negative enough\n";
            rep += "to push the constraint into the active set, where the curve starts to rise.\n";
        }
        _params.setReport(rep);
    }
};
