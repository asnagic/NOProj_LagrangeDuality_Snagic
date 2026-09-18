//
//  PlotCanvas.h
//
//  A small 2-D plotting canvas built directly on gui::Canvas: axes with "nice" ticks,
//  a grid, line/point series, shaded bands, reference lines, annotated markers, filled
//  scalar fields with contour lines, and a legend.
//
//  Everything is drawn in model coordinates and mapped to device pixels here, so the
//  views only ever hand over mathematical data.
//
#pragma once
#include <gui/Canvas.h>
#include <gui/Shape.h>
#include <gui/DrawableString.h>
#include <gui/Font.h>
#include <gui/Application.h>
#include <td/ColorID.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

class PlotCanvas : public gui::Canvas
{
public:
    enum class SeriesStyle : unsigned char { Line = 0, Dots, LineAndDots, FilledArea };

    struct Series
    {
        std::vector<gui::Point> pts;
        td::ColorID color = td::ColorID::Blue;
        float width = 2.0f;
        td::LinePattern pattern = td::LinePattern::Solid;
        SeriesStyle style = SeriesStyle::Line;
        td::String name;
        bool inLegend = true;
        double baseline = 0.0;      //for FilledArea
        float alpha = 0.25f;        //for FilledArea
    };

    struct Marker
    {
        gui::Point pt;
        td::ColorID color = td::ColorID::Red;
        double radius = 5.0;
        td::String label;
        bool labelAbove = true;
        double labelDy = 0.0;   //extra pixels, to separate labels of nearby markers
    };

    struct RefLine
    {
        bool vertical = false;
        double value = 0.0;
        td::ColorID color = td::ColorID::Gray;
        td::LinePattern pattern = td::LinePattern::Dash;
        float width = 1.0f;
        td::String label;
    };

    struct Polygon
    {
        std::vector<gui::Point> pts;
        td::ColorID fill = td::ColorID::LightSteelBlue;
        td::ColorID line = td::ColorID::SteelBlue;
        float alpha = 0.30f;
        float lineWidth = 1.5f;
        td::String name;
        bool inLegend = false;
        bool filled = true;     //false draws the outline only, leaving the field visible
        bool hatch = false;     //diagonal hatching, the usual way to shade a feasible set
        double hatchStep = 11.0;
    };

    //A scalar field sampled on a regular grid, drawn as filled contour bands.
    struct Field
    {
        bool active = false;
        size_t nx = 0, ny = 0;
        double x0 = 0, x1 = 0, y0 = 0, y1 = 0;
        std::vector<double> v;              //row-major, size nx*ny, v[iy*nx+ix]
        std::vector<double> levels;         //contour levels, ascending
        bool drawLines = true;
        double at(size_t ix, size_t iy) const { return v[iy * nx + ix]; }
    };

private:
    std::vector<Series>  _series;
    std::vector<Marker>  _markers;
    std::vector<RefLine> _refLines;
    std::vector<Polygon> _polys;
    Field                _field;

    td::String _title;
    td::String _xLabel;
    td::String _yLabel;
    td::String _note;               //free text drawn in the corner

    double _xMin = 0, _xMax = 1, _yMin = 0, _yMax = 1;
    bool   _fixedX = false, _fixedY = false;
    bool   _equalAspect = false;
    bool   _showLegend = true;

    //plot area margins in pixels
    static constexpr double ML = 68.0, MR = 18.0, MT = 34.0, MB = 46.0;

    //The canvas follows the system theme on screen, but every other backend - PDF, SVG,
    //print - lands on white paper, so it always uses the light palette there.  Without
    //this, dark-mode SysText is near-white and exports come out blank.
    bool lightPalette() const
    {
        return (_backend != gui::Canvas::Backend::Display) || !gui::Application::isDarkMode();
    }
    td::ColorID fg() const       { return lightPalette() ? td::ColorID::Black : td::ColorID::WhiteSmoke; }
    td::ColorID bgPage() const   { return lightPalette() ? td::ColorID::White : td::ColorID::ObsidianGray; }
    td::ColorID bgPlot() const   { return lightPalette() ? td::ColorID::WhiteSmoke : td::ColorID::Black; }
    td::ColorID gridClr() const  { return lightPalette() ? td::ColorID::Silver : td::ColorID::DimGray; }

    //discrete colour ramp used for scalar fields (cool -> warm)
    static const std::vector<td::ColorID>& ramp()
    {
        static const std::vector<td::ColorID> r = {
            td::ColorID::MidnightBlue, td::ColorID::DarkSlateBlue, td::ColorID::SteelBlue,
            td::ColorID::CadetBlue,    td::ColorID::LightSeaGreen, td::ColorID::MediumSeaGreen,
            td::ColorID::YellowGreen,  td::ColorID::Gold,          td::ColorID::Orange,
            td::ColorID::DarkOrange,   td::ColorID::OrangeRed,     td::ColorID::Firebrick
        };
        return r;
    }

public:
    PlotCanvas() = default;

    //------------------------------------------------------------------ content
    void clear()
    {
        _series.clear();
        _markers.clear();
        _refLines.clear();
        _polys.clear();
        _field.active = false;
        _note = "";
    }

    void setPlotTitle(const td::String& t) { _title = t; }
    void setXLabel(const td::String& t) { _xLabel = t; }
    void setYLabel(const td::String& t) { _yLabel = t; }
    void setNote(const td::String& t)   { _note = t; }
    void showLegend(bool b)             { _showLegend = b; }
    void setEqualAspect(bool b)         { _equalAspect = b; }

    void setXRange(double a, double b) { _xMin = a; _xMax = b; _fixedX = true; }
    void setYRange(double a, double b) { _yMin = a; _yMax = b; _fixedY = true; }
    void autoRange()                   { _fixedX = false; _fixedY = false; }

    void add(const Series& s)  { _series.push_back(s); }
    void add(const Marker& m)  { _markers.push_back(m); }
    void add(const RefLine& r) { _refLines.push_back(r); }
    void add(const Polygon& p) { _polys.push_back(p); }
    void setField(const Field& f) { _field = f; _field.active = true; }

    //Convenience: build a series from parallel x/y arrays.
    static Series makeSeries(const std::vector<double>& xs, const std::vector<double>& ys,
                             td::ColorID color, const td::String& name,
                             float width = 2.0f,
                             td::LinePattern pattern = td::LinePattern::Solid)
    {
        Series s;
        s.color = color;
        s.name = name;
        s.width = width;
        s.pattern = pattern;
        const size_t n = std::min(xs.size(), ys.size());
        s.pts.reserve(n);
        for (size_t i = 0; i < n; ++i)
        {
            if (std::isfinite(xs[i]) && std::isfinite(ys[i]))
                s.pts.emplace_back(xs[i], ys[i]);
        }
        return s;
    }

    //------------------------------------------------------------------ drawing
protected:
    void onDraw(const gui::Rect& /*drawRect*/) override
    {
        gui::Size sz;
        getSize(sz);
        const double W = sz.width;
        const double H = sz.height;
        if (W < 140 || H < 110)
            return;

        computeRanges();

        //The note gets its own band between the title and the plot frame. Floating it
        //inside the plot meant it covered reference-line labels and fought the legend.
        const double pl = ML, pr = W - MR;
        double noteH = 0.0;
        gui::Size ns;
        const bool showNote = (_note.length() > 0) && (pr - pl > 240.0);
        if (showNote)
        {
            gui::DrawableString ds(_note);
            ds.measure(gui::Font::ID::SystemSmaller, pr - pl, ns);
            noteH = ns.height + 4.0;
        }
        const double pt = MT + noteH, pb = H - MB;
        if (pr <= pl + 10 || pb <= pt + 10)
            return;

        if (_equalAspect)
            applyEqualAspect(pr - pl, pb - pt);

        const gui::Rect plotRect(pl, pt, pr, pb);

        //opaque page and plot backgrounds, so the figure is self-contained when exported
        gui::Shape::drawRect(gui::Rect(0, 0, W, H), bgPage());
        gui::Shape::drawRect(plotRect, bgPlot());

        drawField(plotRect);
        drawGridAndAxes(plotRect);
        drawPolygons(plotRect);
        drawRefLines(plotRect);
        drawSeries(plotRect);
        drawMarkers(plotRect);

        //frame on top of everything
        gui::Shape::drawRect(plotRect, fg(), 1.0f);

        //title
        if (_title.length() > 0)
        {
            const gui::Rect tr(pl, 4.0, pr, MT - 6.0);
            gui::DrawableString::draw(_title, tr, gui::Font::ID::SystemBold, fg(),
                                      td::TextAlignment::Center, td::VAlignment::Center);
        }

        const double plotW = pr - pl;
        if (showNote)
        {
            const gui::Rect nr(pl, MT - 2.0, pr, MT - 2.0 + ns.height);
            gui::DrawableString::draw(_note, nr, gui::Font::ID::SystemSmaller, fg(),
                                      td::TextAlignment::Left, td::VAlignment::Top,
                                      td::TextEllipsize::None);
        }

        if (_showLegend && plotW > 300.0)
            drawLegend(plotRect);
    }

private:
    //------------------------------------------------------------------ mapping
    double sx(double x, const gui::Rect& r) const
    {
        return r.left + (x - _xMin) / (_xMax - _xMin) * r.width();
    }
    double sy(double y, const gui::Rect& r) const
    {
        return r.bottom - (y - _yMin) / (_yMax - _yMin) * r.height();
    }
    gui::Point sp(const gui::Point& p, const gui::Rect& r) const
    {
        return gui::Point(sx(p.x, r), sy(p.y, r));
    }

    void computeRanges()
    {
        if (_fixedX && _fixedY)
            return;

        double xlo = std::numeric_limits<double>::infinity();
        double xhi = -std::numeric_limits<double>::infinity();
        double ylo = xlo, yhi = xhi;
        bool any = false;

        auto acc = [&](double x, double y)
        {
            if (!std::isfinite(x) || !std::isfinite(y))
                return;
            xlo = std::min(xlo, x); xhi = std::max(xhi, x);
            ylo = std::min(ylo, y); yhi = std::max(yhi, y);
            any = true;
        };

        for (const auto& s : _series)
            for (const auto& p : s.pts)
                acc(p.x, p.y);
        for (const auto& p : _polys)
            for (const auto& q : p.pts)
                acc(q.x, q.y);
        for (const auto& m : _markers)
            acc(m.pt.x, m.pt.y);
        if (_field.active)
        {
            acc(_field.x0, _field.y0);
            acc(_field.x1, _field.y1);
        }
        for (const auto& rl : _refLines)
        {
            if (rl.vertical) { xlo = std::min(xlo, rl.value); xhi = std::max(xhi, rl.value); }
            else             { ylo = std::min(ylo, rl.value); yhi = std::max(yhi, rl.value); }
        }

        if (!any)
        {
            if (!_fixedX) { _xMin = 0; _xMax = 1; }
            if (!_fixedY) { _yMin = 0; _yMax = 1; }
            return;
        }

        auto pad = [](double& lo, double& hi)
        {
            if (!(std::isfinite(lo) && std::isfinite(hi)))
            {
                lo = 0; hi = 1; return;
            }
            double d = hi - lo;
            if (d < 1e-12)
            {
                const double c = 0.5 * (lo + hi);
                lo = c - 0.5; hi = c + 0.5; return;
            }
            lo -= 0.06 * d;
            hi += 0.06 * d;
        };

        if (!_fixedX) { pad(xlo, xhi); _xMin = xlo; _xMax = xhi; }
        if (!_fixedY) { pad(ylo, yhi); _yMin = ylo; _yMax = yhi; }
    }

    void applyEqualAspect(double wpx, double hpx)
    {
        const double dx = _xMax - _xMin;
        const double dy = _yMax - _yMin;
        const double sxu = wpx / dx;
        const double syu = hpx / dy;
        if (sxu > syu)
        {
            const double newDx = wpx / syu;
            const double c = 0.5 * (_xMin + _xMax);
            _xMin = c - 0.5 * newDx;
            _xMax = c + 0.5 * newDx;
        }
        else
        {
            const double newDy = hpx / sxu;
            const double c = 0.5 * (_yMin + _yMax);
            _yMin = c - 0.5 * newDy;
            _yMax = c + 0.5 * newDy;
        }
    }

    //------------------------------------------------------------------ ticks
    static double niceStep(double range, int target)
    {
        if (range <= 0 || target <= 0)
            return 1.0;
        const double raw = range / double(target);
        const double mag = std::pow(10.0, std::floor(std::log10(raw)));
        const double norm = raw / mag;
        double step;
        if (norm < 1.5)      step = 1.0;
        else if (norm < 3.0) step = 2.0;
        else if (norm < 7.0) step = 5.0;
        else                 step = 10.0;
        return step * mag;
    }

    static td::String fmtTick(double v, double step)
    {
        td::String s;
        if (std::fabs(v) < 1e-12)
            v = 0.0;
        const double a = std::fabs(step);
        if (a >= 100.0)       s.format("%.0f", v);
        else if (a >= 10.0)   s.format("%.0f", v);
        else if (a >= 1.0)    s.format("%.0f", v);
        else if (a >= 0.1)    s.format("%.1f", v);
        else if (a >= 0.01)   s.format("%.2f", v);
        else if (a >= 0.001)  s.format("%.3f", v);
        else                  s.format("%.2e", v);
        return s;
    }

    void drawGridAndAxes(const gui::Rect& r)
    {
        const double stepX = niceStep(_xMax - _xMin, 8);
        const double stepY = niceStep(_yMax - _yMin, 6);

        //vertical grid + x tick labels
        double x0 = std::ceil(_xMin / stepX) * stepX;
        for (double x = x0; x <= _xMax + 1e-9 * stepX; x += stepX)
        {
            const double px = sx(x, r);
            gui::Shape::drawLine(gui::Point(px, r.top), gui::Point(px, r.bottom),
                                 gridClr(), 0.8f, td::LinePattern::Dot, 0.9f);
            const gui::Rect lr(px - 42.0, r.bottom + 4.0, px + 42.0, r.bottom + 20.0);
            gui::DrawableString::draw(fmtTick(x, stepX), lr, gui::Font::ID::SystemSmaller,
                                      fg(), td::TextAlignment::Center,
                                      td::VAlignment::Top);
        }

        //horizontal grid + y tick labels
        double y0 = std::ceil(_yMin / stepY) * stepY;
        for (double y = y0; y <= _yMax + 1e-9 * stepY; y += stepY)
        {
            const double py = sy(y, r);
            gui::Shape::drawLine(gui::Point(r.left, py), gui::Point(r.right, py),
                                 gridClr(), 0.8f, td::LinePattern::Dot, 0.9f);
            const gui::Rect lr(4.0, py - 9.0, r.left - 6.0, py + 9.0);
            gui::DrawableString::draw(fmtTick(y, stepY), lr, gui::Font::ID::SystemSmaller,
                                      fg(), td::TextAlignment::Right,
                                      td::VAlignment::Center);
        }

        //zero axes, if inside the window
        if (_xMin < 0.0 && _xMax > 0.0)
        {
            const double px = sx(0.0, r);
            gui::Shape::drawLine(gui::Point(px, r.top), gui::Point(px, r.bottom),
                                 fg(), 1.2f, td::LinePattern::Solid, 0.75f);
        }
        if (_yMin < 0.0 && _yMax > 0.0)
        {
            const double py = sy(0.0, r);
            gui::Shape::drawLine(gui::Point(r.left, py), gui::Point(r.right, py),
                                 fg(), 1.2f, td::LinePattern::Solid, 0.75f);
        }

        //axis captions
        if (_xLabel.length() > 0)
        {
            const gui::Rect xr(r.left, r.bottom + 22.0, r.right, r.bottom + 42.0);
            gui::DrawableString::draw(_xLabel, xr, gui::Font::ID::SystemSmallerBold,
                                      fg(), td::TextAlignment::Center,
                                      td::VAlignment::Center);
        }
        if (_yLabel.length() > 0)
        {
            //drawn horizontally above the y axis to avoid rotated-text layout issues
            const gui::Rect yr(2.0, r.top - 28.0, r.left + 210.0, r.top - 8.0);
            gui::DrawableString::draw(_yLabel, yr, gui::Font::ID::SystemSmallerBold,
                                      fg(), td::TextAlignment::Left,
                                      td::VAlignment::Center, td::TextEllipsize::None);
        }
    }

    //------------------------------------------------------------------ primitives
    //Clips a segment to the plot rectangle (Liang-Barsky). Returns false if fully outside.
    static bool clipSeg(gui::Point& a, gui::Point& b, const gui::Rect& r)
    {
        double t0 = 0.0, t1 = 1.0;
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double pq[4][2] = { { -dx, a.x - r.left }, { dx, r.right - a.x },
                                  { -dy, a.y - r.top  }, { dy, r.bottom - a.y } };
        for (int i = 0; i < 4; ++i)
        {
            const double pp = pq[i][0], qq = pq[i][1];
            if (std::fabs(pp) < 1e-12)
            {
                if (qq < 0.0)
                    return false;
                continue;
            }
            const double t = qq / pp;
            if (pp < 0.0) { if (t > t1) return false; if (t > t0) t0 = t; }
            else          { if (t < t0) return false; if (t < t1) t1 = t; }
        }
        const gui::Point a2(a.x + t0 * dx, a.y + t0 * dy);
        const gui::Point b2(a.x + t1 * dx, a.y + t1 * dy);
        a = a2; b = b2;
        return true;
    }

    //Scanline hatching of a polygon, rotated 45 degrees so the strokes cannot be confused
    //with the grid or the zero axes.
    void drawHatch(const std::vector<gui::Point>& dev, const Polygon& p, const gui::Rect& r)
    {
        const double c = 0.70710678118654752, sn = -0.70710678118654752;   //rotate by -45
        std::vector<gui::Point> rot;
        rot.reserve(dev.size());
        double ylo = 1e300, yhi = -1e300;
        for (const auto& q : dev)
        {
            const double rx = q.x * c - q.y * sn;
            const double ry = q.x * sn + q.y * c;
            rot.emplace_back(rx, ry);
            ylo = std::min(ylo, ry);
            yhi = std::max(yhi, ry);
        }

        const double step = std::max(4.0, p.hatchStep);
        std::vector<double> xs;
        for (double y = std::ceil(ylo / step) * step; y <= yhi; y += step)
        {
            xs.clear();
            const size_t n = rot.size();
            for (size_t i = 0; i < n; ++i)
            {
                const gui::Point& a = rot[i];
                const gui::Point& b = rot[(i + 1) % n];
                if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y))
                {
                    const double t = (y - a.y) / (b.y - a.y);
                    xs.push_back(a.x + t * (b.x - a.x));
                }
            }
            std::sort(xs.begin(), xs.end());
            for (size_t k = 0; k + 1 < xs.size(); k += 2)
            {
                //rotate the span back into device space
                gui::Point p1(xs[k]     * c + y * sn, -xs[k]     * sn + y * c);
                gui::Point p2(xs[k + 1] * c + y * sn, -xs[k + 1] * sn + y * c);
                if (clipSeg(p1, p2, r))
                    gui::Shape::drawLine(p1, p2, p.line, 1.0f, td::LinePattern::Solid, 0.75f);
            }
        }
    }

    void drawPolygons(const gui::Rect& r)
    {
        for (const auto& p : _polys)
        {
            if (p.pts.size() < 3)
                continue;
            std::vector<gui::Point> dev;
            dev.reserve(p.pts.size());
            for (const auto& q : p.pts)
                dev.push_back(sp(q, r));

            if (p.filled)
            {
                gui::Shape shp;
                shp.createPolygon(dev.data(), dev.size(), p.lineWidth);
                shp.drawFill(p.fill);
            }
            if (p.hatch)
                drawHatch(dev, p, r);

            //outline, clipped edge by edge so it cannot spill into the margins
            for (size_t i = 0; i < dev.size(); ++i)
            {
                gui::Point a = dev[i];
                gui::Point b = dev[(i + 1) % dev.size()];
                if (clipSeg(a, b, r))
                    gui::Shape::drawLine(a, b, p.line, p.lineWidth);
            }
        }
    }

    void drawRefLines(const gui::Rect& r)
    {
        for (const auto& rl : _refLines)
        {
            if (rl.vertical)
            {
                if (rl.value < _xMin || rl.value > _xMax)
                    continue;
                const double px = sx(rl.value, r);
                gui::Shape::drawLine(gui::Point(px, r.top), gui::Point(px, r.bottom),
                                     rl.color, rl.width, rl.pattern);
                if (rl.label.length() > 0)
                {
                    //bottom, not top: the legend lives in the top-right corner
                    const bool toLeft = (px > r.right - 200.0);
                    const gui::Rect lr(toLeft ? px - 194.0 : px + 4.0, r.bottom - 20.0,
                                       toLeft ? px - 4.0   : px + 194.0, r.bottom - 4.0);
                    gui::DrawableString::draw(rl.label, lr, gui::Font::ID::SystemSmaller,
                                              rl.color,
                                              toLeft ? td::TextAlignment::Right
                                                     : td::TextAlignment::Left,
                                              td::VAlignment::Center);
                }
            }
            else
            {
                if (rl.value < _yMin || rl.value > _yMax)
                    continue;
                const double py = sy(rl.value, r);
                gui::Shape::drawLine(gui::Point(r.left, py), gui::Point(r.right, py),
                                     rl.color, rl.width, rl.pattern);
                if (rl.label.length() > 0)
                {
                    const gui::Rect lr(r.left + 6.0, py - 18.0, r.left + 240.0, py - 2.0);
                    gui::DrawableString::draw(rl.label, lr, gui::Font::ID::SystemSmaller,
                                              rl.color, td::TextAlignment::Left,
                                              td::VAlignment::Bottom);
                }
            }
        }
    }

    void drawSeries(const gui::Rect& r)
    {
        for (const auto& s : _series)
        {
            if (s.pts.empty())
                continue;

            std::vector<gui::Point> dev;
            dev.reserve(s.pts.size());
            for (const auto& p : s.pts)
                dev.push_back(sp(p, r));

            if (s.style == SeriesStyle::FilledArea && dev.size() >= 2)
            {
                std::vector<gui::Point> poly = dev;
                poly.emplace_back(dev.back().x, sy(s.baseline, r));
                poly.emplace_back(dev.front().x, sy(s.baseline, r));
                gui::Shape shp;
                shp.createPolygon(poly.data(), poly.size(), 1.0f);
                shp.drawFill(s.color);
            }

            if (s.style != SeriesStyle::Dots && dev.size() >= 2)
            {
                gui::Shape shp;
                shp.createPolyLine(dev.data(), dev.size(), s.width, s.pattern);
                shp.drawWire(s.color, s.width);
            }

            if (s.style == SeriesStyle::Dots || s.style == SeriesStyle::LineAndDots)
            {
                for (const auto& p : dev)
                {
                    gui::Shape c;
                    c.createCircle(gui::Circle(p, 3.0), 1.0f);
                    c.drawFillAndWire(s.color, s.color, 1.0f);
                }
            }
        }
    }

    void drawMarkers(const gui::Rect& r)
    {
        for (const auto& m : _markers)
        {
            const gui::Point p = sp(m.pt, r);
            gui::Shape c;
            c.createCircle(gui::Circle(p, m.radius), 1.5f);
            c.drawFillAndWire(m.color, fg(), 1.5f);

            if (m.label.length() > 0)
            {
                bool above = m.labelAbove;
                double dy = (above ? -22.0 : 11.0) + m.labelDy;
                //flip to the other side if the label would leave the plot area
                if (above && p.y + dy < r.top + 2.0)
                {
                    above = false;
                    dy = 11.0 + m.labelDy;
                }
                else if (!above && p.y + dy + 16.0 > r.bottom - 2.0)
                {
                    above = true;
                    dy = -22.0 - m.labelDy;
                }
                const gui::Rect lr(p.x - 110.0, p.y + dy, p.x + 110.0, p.y + dy + 16.0);
                gui::DrawableString::draw(m.label, lr, gui::Font::ID::SystemSmallerBold,
                                          fg(), td::TextAlignment::Center,
                                          td::VAlignment::Center);
            }
        }
    }

    //------------------------------------------------------------------ scalar field
    void drawField(const gui::Rect& r)
    {
        if (!_field.active || _field.nx < 2 || _field.ny < 2 || _field.levels.size() < 2)
            return;

        const auto& lv = _field.levels;
        const auto& cr = ramp();

        auto bandOf = [&](double v) -> size_t
        {
            if (!std::isfinite(v))
                return 0;
            size_t k = 0;
            while (k + 1 < lv.size() && v > lv[k + 1])
                ++k;
            return k;
        };
        auto colorOf = [&](size_t band) -> td::ColorID
        {
            const size_t nb = lv.size() - 1;
            if (nb == 0)
                return cr.front();
            const size_t idx = (band * (cr.size() - 1)) / std::max<size_t>(1, nb - 1);
            return cr[std::min(idx, cr.size() - 1)];
        };

        const double dx = (_field.x1 - _field.x0) / double(_field.nx - 1);
        const double dy = (_field.y1 - _field.y0) / double(_field.ny - 1);

        //filled cells
        for (size_t iy = 0; iy + 1 < _field.ny; ++iy)
        {
            for (size_t ix = 0; ix + 1 < _field.nx; ++ix)
            {
                const double vAvg = 0.25 * (_field.at(ix, iy) + _field.at(ix + 1, iy) +
                                            _field.at(ix, iy + 1) + _field.at(ix + 1, iy + 1));
                if (!std::isfinite(vAvg))
                    continue;

                const double xa = _field.x0 + double(ix) * dx;
                const double ya = _field.y0 + double(iy) * dy;
                const double xb = xa + dx;
                const double yb = ya + dy;

                const double pxa = sx(xa, r), pxb = sx(xb, r);
                const double pya = sy(yb, r), pyb = sy(ya, r);   //y flips
                if (pxb < r.left || pxa > r.right || pyb < r.top || pya > r.bottom)
                    continue;

                //+1 avoids hairline seams between adjacent cells
                const gui::Rect cell(std::max(pxa, r.left), std::max(pya, r.top),
                                     std::min(pxb + 1.0, r.right), std::min(pyb + 1.0, r.bottom));
                if (cell.width() <= 0 || cell.height() <= 0)
                    continue;
                gui::Shape::drawRect(cell, 0.85f, colorOf(bandOf(vAvg)));
            }
        }

        if (!_field.drawLines)
            return;

        //contour lines by marching squares
        for (size_t k = 1; k + 1 < lv.size(); ++k)
        {
            const double level = lv[k];
            for (size_t iy = 0; iy + 1 < _field.ny; ++iy)
            {
                for (size_t ix = 0; ix + 1 < _field.nx; ++ix)
                {
                    const double v00 = _field.at(ix, iy);
                    const double v10 = _field.at(ix + 1, iy);
                    const double v11 = _field.at(ix + 1, iy + 1);
                    const double v01 = _field.at(ix, iy + 1);
                    if (!(std::isfinite(v00) && std::isfinite(v10) &&
                          std::isfinite(v11) && std::isfinite(v01)))
                        continue;

                    const double xa = _field.x0 + double(ix) * dx;
                    const double ya = _field.y0 + double(iy) * dy;
                    const double xb = xa + dx;
                    const double yb = ya + dy;

                    gui::Point crossings[4];
                    int nc = 0;
                    auto edge = [&](double va, double vb, double xA, double yA,
                                    double xB, double yB)
                    {
                        if ((va < level && vb >= level) || (vb < level && va >= level))
                        {
                            const double t = (level - va) / (vb - va);
                            if (nc < 4)
                                crossings[nc++] = gui::Point(xA + t * (xB - xA),
                                                             yA + t * (yB - yA));
                        }
                    };
                    edge(v00, v10, xa, ya, xb, ya);
                    edge(v10, v11, xb, ya, xb, yb);
                    edge(v11, v01, xb, yb, xa, yb);
                    edge(v01, v00, xa, yb, xa, ya);

                    if (nc >= 2)
                    {
                        gui::Shape::drawLine(sp(crossings[0], r), sp(crossings[1], r),
                                             fg(), 0.7f,
                                             td::LinePattern::Solid, 0.45f);
                        if (nc == 4)
                            gui::Shape::drawLine(sp(crossings[2], r), sp(crossings[3], r),
                                                 fg(), 0.7f,
                                                 td::LinePattern::Solid, 0.45f);
                    }
                }
            }
        }
    }

    //------------------------------------------------------------------ legend
    void drawLegend(const gui::Rect& r)
    {
        struct Entry { td::ColorID c; td::String n; bool dashed; };
        std::vector<Entry> entries;
        for (const auto& s : _series)
            if (s.inLegend && s.name.length() > 0)
                entries.push_back({ s.color, s.name, s.pattern != td::LinePattern::Solid });
        for (const auto& p : _polys)
            if (p.inLegend && p.name.length() > 0)
                entries.push_back({ p.line, p.name, false });
        if (entries.empty())
            return;

        const double lineH = 17.0;
        const double boxW = 208.0;
        const double boxH = lineH * double(entries.size()) + 10.0;
        const double x0 = r.right - boxW - 10.0;
        const double y0 = r.top + 8.0;

        const gui::Rect box(x0, y0, x0 + boxW, y0 + boxH);
        gui::Shape::drawRect(box, bgPage());
        gui::Shape::drawRect(box, fg(), 0.8f);

        double y = y0 + 5.0;
        for (const auto& e : entries)
        {
            gui::Shape::drawLine(gui::Point(x0 + 8.0, y + lineH * 0.5),
                                 gui::Point(x0 + 32.0, y + lineH * 0.5),
                                 e.c, 2.4f,
                                 e.dashed ? td::LinePattern::Dash : td::LinePattern::Solid);
            const gui::Rect tr(x0 + 38.0, y, x0 + boxW - 6.0, y + lineH);
            gui::DrawableString::draw(e.n, tr, gui::Font::ID::SystemSmaller,
                                      fg(), td::TextAlignment::Left,
                                      td::VAlignment::Center);
            y += lineH;
        }
    }
};
