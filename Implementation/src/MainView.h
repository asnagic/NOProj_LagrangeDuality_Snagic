//
//  MainView.h
//
//  Owns the model and the tabs.  Every parameter panel calls the same _fnRefresh, so one
//  edit anywhere re-solves and repaints the whole application.
//
#pragma once
#include <gui/StandardTabView.h>
#include <functional>
#include <vector>
#include "core/AppModel.h"
#include "views/ViewQP.h"
#include "views/ViewDual.h"
#include "views/ViewSensitivity.h"
#include "views/ViewAllocation.h"
#include "views/ViewGap.h"
#include "views/ViewTheory.h"

class MainView : public gui::StandardTabView
{
    core::AppModel _model;

    ViewQP          _viewQP;
    ViewDual        _viewDual;
    ViewSensitivity _viewSens;
    ViewAllocation  _viewAlloc;
    ViewGap         _viewGap;
    ViewTheory      _viewTheory;

    std::function<void()> _fnRefresh;

public:
    MainView()
    {
        _fnRefresh = [this]() { refreshAll(); };

        _viewQP.attach(&_model, &_fnRefresh);
        _viewDual.attach(&_model);
        _viewSens.attach(&_model, &_fnRefresh);
        _viewAlloc.attach(&_model, &_fnRefresh);
        _viewGap.attach(&_model, &_fnRefresh);
        _viewTheory.attach(&_model);

        addTab(_viewQP,     "1. Primal QP");
        addTab(_viewDual,   "2. Dual function");
        addTab(_viewSens,   "3. Sensitivity");
        addTab(_viewAlloc,  "4. Resource allocation");
        addTab(_viewGap,    "5. Duality gap");
        addTab(_viewTheory, "6. Theory and checks");

        refreshAll();
    }

    core::AppModel& model() { return _model; }

    //Every plot in the application, in tab order, with a file-name stem for each.
    void collectCanvases(std::vector<PlotCanvas*>& out, std::vector<td::String>& names)
    {
        _viewQP.collectCanvases(out, names);
        _viewDual.collectCanvases(out, names);
        _viewSens.collectCanvases(out, names);
        _viewAlloc.collectCanvases(out, names);
        _viewGap.collectCanvases(out, names);
    }

    //Tab index that owns each canvas, so the exporter can bring it on screen first.
    void collectCanvasTabs(std::vector<int>& tabs)
    {
        tabs = { 0, 1, 2, 3, 3, 4, 4, 4 };
    }

    void refreshAll()
    {
        _viewQP.refresh();
        _viewDual.refresh();
        _viewSens.refresh();
        _viewAlloc.refresh();
        _viewGap.refresh();
        _viewTheory.refresh();
    }

    void resetAll()
    {
        _model.resetDefaults();
        _model.recompute();
        _viewQP.attach(&_model, &_fnRefresh);
        _viewAlloc.attach(&_model, &_fnRefresh);
        _viewGap.attach(&_model, &_fnRefresh);
        refreshAll();
    }

    //Brings a tab on screen; used by the figure exporter.
    void showTab(int pos) { setCurrentViewPos(pos); }

private:
    //StandardTabView holds fixed, non-closable tabs and does not take ownership,
    //which is what these member views need.
    void addTab(gui::View& v, const char* title)
    {
        addView(&v, title);
    }
};
