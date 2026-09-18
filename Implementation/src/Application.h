//
//  Application.h
//
#pragma once
#include <gui/Application.h>
#include <cstring>
#include "MainWindow.h"

class Application : public gui::Application
{
    td::String _exportDir;      //set by -export=<folder>

protected:
    gui::Window* createInitialWindow() override
    {
        return new MainWindow(_exportDir.length() ? _exportDir.c_str() : nullptr);
    }

public:
    Application(int argc, const char** argv)
    : gui::Application(argc, argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strncmp(argv[i], "-export=", 8) == 0)
                _exportDir = argv[i] + 8;
        }
    }
};
