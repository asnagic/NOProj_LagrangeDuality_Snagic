//
//  MainWindow.h
//
//  Hosts the tabs, and implements the figure exporter.
//
//  Export mode (-export=<folder>) walks the tabs one timer tick at a time so that each
//  canvas is laid out on screen before it is written, then writes every plot to PDF and
//  the theory page to a text file, and closes.  It is how the figures for the written
//  report are produced, and it doubles as a way to exercise the whole drawing path
//  without a human at the keyboard.
//
#pragma once
#include <gui/Window.h>
#include <gui/ActionItem.h>
#include <gui/Timer.h>
#include <td/String.h>
#include <vector>
#include <cstdio>
#include "MainView.h"

class MainWindow : public gui::Window
{
protected:
    MainView _mainView;

    //export state
    td::String _exportDir;
    bool _exporting = false;
    int _tick = -1;
    std::vector<PlotCanvas*> _canvases;
    std::vector<td::String> _names;
    std::vector<int> _tabOf;
    gui::Timer _timer;

public:
    //Figures are exported at the canvas's on-screen size, so export mode opens a larger
    //window than normal use to get report-quality pages out of the same drawing code.
    static gui::Size windowSize(const char* exportDir)
    {
        return (exportDir && *exportDir) ? gui::Size(1900, 1180) : gui::Size(1340, 840);
    }

    MainWindow(const char* exportDir = nullptr)
    : gui::Window(windowSize(exportDir))
    , _timer(this, 0.45f, false)
    {
        setTitle("Lagrange Duality - dual function, strong duality and sensitivity analysis");
        setCentralView(&_mainView);

        if (exportDir && *exportDir)
        {
            _exportDir = exportDir;
            _exporting = true;
            _mainView.collectCanvases(_canvases, _names);
            _mainView.collectCanvasTabs(_tabOf);
            _timer.start();
        }
    }

    bool shouldClose() override { return true; }

protected:
    bool onTimer(gui::Timer* /*pTimer*/) override
    {
        onExportTick();
        return true;
    }

private:
    void onExportTick()
    {
        ++_tick;

        //one canvas per tick: select its tab, and write the one selected last tick
        if (_tick > 0)
        {
            const size_t prev = size_t(_tick - 1);
            if (prev < _canvases.size())
            {
                td::String path;
                path.format("%s/%s.pdf", _exportDir.c_str(), _names[prev].c_str());
                const bool ok = _canvases[prev]->exportToPDF(path, true);
                std::printf("%s %s\n", ok ? "wrote" : "FAILED", path.c_str());
                std::fflush(stdout);
            }
        }

        if (size_t(_tick) >= _canvases.size())
        {
            _timer.stop();
            std::printf("export finished: %zu figures\n", _canvases.size());
            std::fflush(stdout);
            close();
            return;
        }

        const int tab = (size_t(_tick) < _tabOf.size()) ? _tabOf[size_t(_tick)] : 0;
        _mainView.showTab(tab);
    }
};
