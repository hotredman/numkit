// tests/figure_test.cpp
//
// Tests for FigureManager and all plotting functionality:
//   - FigureManager API (newFigure, closeFigure, closeAll, subplot)
//   - Plot types (plot, stem, stairs, scatter, bar, polarplot)
//   - Plot config (grid, hold, axis modes, title/xlabel/ylabel/legend)
//   - Log scales (semilogx, semilogy, loglog)
//   - Polar config (rlim, thetalim, thetadir, thetazero)
//   - Figure management (figure, close, clf)
//   - Name-value pairs (LineWidth, MarkerSize)
//   - Subplot

#include <numkit/core/engine.hpp>
#include <numkit/core/figure_manager.hpp>
#include <numkit/builtin/library.hpp>
#include <cmath>
#include <gtest/gtest.h>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

// ============================================================
// FigureManager — direct API tests (no engine)
// ============================================================

class FigureManagerTest : public ::testing::Test
{
protected:
    FigureManager fm;
};

TEST_F(FigureManagerTest, NewFigureStartsAtOne)
{
    EXPECT_EQ(fm.newFigure(), 1);
}

TEST_F(FigureManagerTest, SequentialIds)
{
    EXPECT_EQ(fm.newFigure(), 1);
    EXPECT_EQ(fm.newFigure(), 2);
    EXPECT_EQ(fm.newFigure(), 3);
}

TEST_F(FigureManagerTest, FillsGapsWithMinFreeId)
{
    fm.newFigure(); // 1
    fm.newFigure(); // 2
    fm.newFigure(); // 3
    fm.closeFigure(2);
    EXPECT_EQ(fm.newFigure(), 2); // reuses gap
}

TEST_F(FigureManagerTest, FillsFirstGap)
{
    fm.newFigure(); // 1
    fm.newFigure(); // 2
    fm.newFigure(); // 3
    fm.closeFigure(1);
    fm.closeFigure(2);
    EXPECT_EQ(fm.newFigure(), 1); // picks lowest free
}

TEST_F(FigureManagerTest, CloseFigureUpdatesCurrent)
{
    fm.newFigure(); // 1
    fm.newFigure(); // 2
    fm.newFigure(); // 3
    EXPECT_EQ(fm.currentFigureId(), 3);
    fm.closeFigure(3);
    EXPECT_EQ(fm.currentFigureId(), 2); // falls back to highest
}

TEST_F(FigureManagerTest, CloseFigureNonCurrentKeepsCurrent)
{
    fm.newFigure(); // 1
    fm.newFigure(); // 2
    fm.newFigure(); // 3
    fm.closeFigure(1);
    EXPECT_EQ(fm.currentFigureId(), 3); // unchanged
    EXPECT_EQ(fm.figures().size(), 2u);
}

TEST_F(FigureManagerTest, CloseAllResetsToOne)
{
    fm.newFigure();
    fm.newFigure();
    fm.closeAll();
    EXPECT_EQ(fm.figures().size(), 0u);
    EXPECT_EQ(fm.currentFigureId(), 1);
}

TEST_F(FigureManagerTest, CloseAllThenNewStartsAtOne)
{
    fm.newFigure(); fm.newFigure();
    fm.closeAll();
    EXPECT_EQ(fm.newFigure(), 1);
}

TEST_F(FigureManagerTest, CloseNonExistentIsNoOp)
{
    fm.newFigure();
    fm.closeFigure(999);
    EXPECT_EQ(fm.figures().size(), 1u);
}

TEST_F(FigureManagerTest, SetFigureSwitchesCurrent)
{
    fm.newFigure(); // 1
    fm.newFigure(); // 2
    fm.setFigure(1);
    EXPECT_EQ(fm.currentFigureId(), 1);
}

TEST_F(FigureManagerTest, SubplotCreatesGrid)
{
    fm.newFigure();
    fm.setSubplot(2, 3, 1);
    auto &fig = fm.current();
    EXPECT_EQ(fig.subplotRows, 2);
    EXPECT_EQ(fig.subplotCols, 3);
}

TEST_F(FigureManagerTest, SubplotCreatesMultipleAxes)
{
    fm.newFigure();
    fm.setSubplot(2, 2, 1);
    fm.setSubplot(2, 2, 2);
    fm.setSubplot(2, 2, 3);
    fm.setSubplot(2, 2, 4);
    // Default axes + 4 subplot positions (first may reuse default)
    EXPECT_GE(fm.current().axes.size(), 4u);
}

TEST_F(FigureManagerTest, SubplotSwitchesBack)
{
    fm.newFigure();
    fm.setSubplot(2, 1, 1);
    fm.currentAxes().title = "Top";
    fm.setSubplot(2, 1, 2);
    fm.currentAxes().title = "Bottom";
    fm.setSubplot(2, 1, 1);
    EXPECT_EQ(fm.currentAxes().title, "Top");
}

TEST_F(FigureManagerTest, DefaultAxesState)
{
    fm.newFigure();
    auto &ax = fm.currentAxes();
    EXPECT_EQ(ax.title, "");
    EXPECT_EQ(ax.xlabel, "");
    EXPECT_EQ(ax.ylabel, "");
    EXPECT_EQ(ax.gridMode, "");
    EXPECT_FALSE(ax.holdOn);
    EXPECT_FALSE(ax.polar);
    EXPECT_EQ(ax.xscale, "linear");
    EXPECT_EQ(ax.yscale, "linear");
    EXPECT_EQ(ax.thetaDir, "counterclockwise");
    EXPECT_EQ(ax.thetaZeroLocation, "right");
}

// ============================================================
// Fixture for tests requiring engine + BuiltinLibrary
// ============================================================

class FigureEngineTest : public ::testing::Test
{
public:
    Engine engine;
    std::string capturedOutput;

    void SetUp() override
    {
        capturedOutput.clear();
        engine.setOutputFunc([this](const std::string &s) { capturedOutput += s; });
        // MATLAB-compat: flatten mirror-library functions (graphics.*, signal.*, …)
        // into the workspace so tests can call `plot`, `bar`, `figure`, … flat.
        engine.eval("import compat.*;");
    }

    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
    FigureManager &fm() { return engine.figureManager(); }
    AxesState &ax() { return fm().currentAxes(); }
};

// ============================================================
// Figure / close / clf
// ============================================================

class FigureCloseTest : public FigureEngineTest {};

TEST_F(FigureCloseTest, FigureWithId)
{
    eval("figure(1);");
    eval("figure(2);");
    EXPECT_EQ(fm().currentFigureId(), 2);
    EXPECT_EQ(fm().figures().size(), 2u);
}

TEST_F(FigureCloseTest, FigureNoArg)
{
    eval("figure;");
    EXPECT_EQ(fm().figures().size(), 1u);
}

// ── figure() emits marker (so UI can show panel) ──

TEST_F(FigureCloseTest, FigureEmitsMarker)
{
    capturedOutput.clear();
    eval("figure(1);");
    EXPECT_NE(capturedOutput.find("__FIGURE_DATA__"), std::string::npos)
        << "figure(1) should emit marker, got: " << capturedOutput;
}

TEST_F(FigureCloseTest, FigureNoArgEmitsMarker)
{
    capturedOutput.clear();
    eval("figure;");
    EXPECT_NE(capturedOutput.find("__FIGURE_DATA__"), std::string::npos)
        << "figure should emit marker, got: " << capturedOutput;
}

TEST_F(FigureCloseTest, FigureEmitsCorrectId)
{
    capturedOutput.clear();
    eval("figure(3);");
    EXPECT_NE(capturedOutput.find("\"id\":3"), std::string::npos)
        << "figure(3) marker should contain id:3, got: " << capturedOutput;
}

TEST_F(FigureCloseTest, FigureEmptyHasNoDatasets)
{
    capturedOutput.clear();
    eval("figure(1);");
    EXPECT_NE(capturedOutput.find("\"datasets\":[]"), std::string::npos)
        << "empty figure should have no datasets, got: " << capturedOutput;
}

// ── close() markers go through outputFunc ──

TEST_F(FigureCloseTest, CloseEmitsMarker)
{
    eval("figure(1);");
    capturedOutput.clear();
    eval("close(1)");
    EXPECT_NE(capturedOutput.find("__FIGURE_CLOSE__:1"), std::string::npos)
        << "close(1) should emit close marker, got: " << capturedOutput;
}

TEST_F(FigureCloseTest, CloseAllEmitsMarker)
{
    eval("figure(1); figure(2);");
    capturedOutput.clear();
    eval("close('all')");
    EXPECT_NE(capturedOutput.find("__FIGURE_CLOSE_ALL__"), std::string::npos)
        << "close all should emit marker, got: " << capturedOutput;
}

TEST_F(FigureCloseTest, CloseCurrentEmitsMarker)
{
    eval("figure(1); figure(2);");
    capturedOutput.clear();
    eval("close");
    EXPECT_NE(capturedOutput.find("__FIGURE_CLOSE__:2"), std::string::npos)
        << "close (current=2) should emit marker, got: " << capturedOutput;
}

TEST_F(FigureCloseTest, FigureSwitchBack)
{
    eval("figure(1); figure(2); figure(1);");
    EXPECT_EQ(fm().currentFigureId(), 1);
}

TEST_F(FigureCloseTest, CloseSpecific)
{
    eval("figure(1); figure(2); figure(3);");
    eval("close(2)");
    EXPECT_EQ(fm().figures().size(), 2u);
    EXPECT_EQ(fm().figures().count(2), 0u);
}

TEST_F(FigureCloseTest, CloseFunctionSyntax)
{
    eval("figure(1); figure(2);");
    eval("close('all')");
    EXPECT_EQ(fm().figures().size(), 0u);
}

TEST_F(FigureCloseTest, CloseAllCommandStyle)
{
    eval("figure(1); figure(2);");
    eval("close all");
    EXPECT_EQ(fm().figures().size(), 0u);
}

TEST_F(FigureCloseTest, ClfResetsAxes)
{
    eval("figure(1);");
    eval("plot([1 2], [1 2]);");
    eval("title('test'); grid on;");
    eval("clf");
    EXPECT_EQ(ax().datasets.size(), 0u);
    EXPECT_EQ(ax().title, "");
    EXPECT_EQ(ax().gridMode, "");
}

TEST_F(FigureCloseTest, ClearAllClosesFigures)
{
    eval("figure(1); figure(2);");
    eval("clear all");
    EXPECT_EQ(fm().figures().size(), 0u);
}

// ============================================================
// Grid — on/off/minor/toggle semantics
// ============================================================

class GridTest : public FigureEngineTest {};

TEST_F(GridTest, OnSetsMode)
{
    eval("figure(1); plot([1 2],[1 2]); grid on");
    EXPECT_EQ(ax().gridMode, "on");
}

TEST_F(GridTest, OffClearsMode)
{
    eval("figure(1); plot([1 2],[1 2]); grid on; grid off");
    EXPECT_EQ(ax().gridMode, "");
}

TEST_F(GridTest, ToggleOffToOn)
{
    eval("figure(1); plot([1 2],[1 2]); grid");
    EXPECT_EQ(ax().gridMode, "on");
}

TEST_F(GridTest, ToggleOnToOff)
{
    eval("figure(1); plot([1 2],[1 2]); grid on; grid");
    EXPECT_EQ(ax().gridMode, "");
}

TEST_F(GridTest, MinorFromOff)
{
    eval("figure(1); plot([1 2],[1 2]); grid minor");
    EXPECT_EQ(ax().gridMode, "minor");
}

TEST_F(GridTest, MinorFromOn)
{
    eval("figure(1); plot([1 2],[1 2]); grid on; grid minor");
    EXPECT_EQ(ax().gridMode, "minor");
}

TEST_F(GridTest, MinorToggleBackToOn)
{
    eval("figure(1); plot([1 2],[1 2]); grid minor; grid minor");
    EXPECT_EQ(ax().gridMode, "on");
}

TEST_F(GridTest, FunctionSyntax)
{
    eval("figure(1); plot([1 2],[1 2]); grid('on')");
    EXPECT_EQ(ax().gridMode, "on");
}

TEST_F(GridTest, FunctionSyntaxMinor)
{
    eval("figure(1); plot([1 2],[1 2]); grid('minor')");
    EXPECT_EQ(ax().gridMode, "minor");
}

// ============================================================
// Hold — on/off/toggle, data preservation
// ============================================================

class HoldTest : public FigureEngineTest {};

TEST_F(HoldTest, OnSetsFlag)
{
    eval("figure(1); plot([1 2],[1 2]); hold on");
    EXPECT_TRUE(ax().holdOn);
}

TEST_F(HoldTest, OffClearsFlag)
{
    eval("figure(1); plot([1 2],[1 2]); hold on; hold off");
    EXPECT_FALSE(ax().holdOn);
}

TEST_F(HoldTest, Toggle)
{
    eval("figure(1); plot([1 2],[1 2]);");
    eval("hold"); // off → on
    EXPECT_TRUE(ax().holdOn);
    eval("hold"); // on → off
    EXPECT_FALSE(ax().holdOn);
}

TEST_F(HoldTest, OnPreservesDatasets)
{
    eval("figure(1); plot([1 2],[1 2]); hold on; plot([3 4],[3 4]);");
    EXPECT_EQ(ax().datasets.size(), 2u);
}

TEST_F(HoldTest, OffReplacesDatasets)
{
    eval("figure(1); plot([1 2],[1 2]); plot([3 4],[3 4]);");
    EXPECT_EQ(ax().datasets.size(), 1u);
}

// ============================================================
// Plot types — stem, stairs, scatter, bar
// ============================================================

class PlotTypeTest : public FigureEngineTest {};

TEST_F(PlotTypeTest, PlotCreatesLine)
{
    eval("figure(1); plot([1 2 3],[1 4 9]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "line");
}

TEST_F(PlotTypeTest, StemCreatesStem)
{
    eval("figure(1); stem([1 2 3],[1 4 9]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "stem");
}

TEST_F(PlotTypeTest, StairsCreatesStairs)
{
    eval("figure(1); stairs([1 2 3],[1 4 9]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "stairs");
}

TEST_F(PlotTypeTest, ScatterCreatesScatter)
{
    eval("figure(1); scatter([1 2 3],[1 4 9]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "scatter");
}

TEST_F(PlotTypeTest, BarCreatesBar)
{
    eval("figure(1); bar([1 2 3 4]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "bar");
}

TEST_F(PlotTypeTest, HistCreatesBar)
{
    eval("figure(1); hist(randn(1, 100));");
    ASSERT_GE(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "bar");
}

// ============================================================
// Log scales
// ============================================================

class LogScaleTest : public FigureEngineTest {};

TEST_F(LogScaleTest, SemilogxSetsXscale)
{
    eval("figure(1); semilogx([1 10 100],[1 2 3]);");
    EXPECT_EQ(ax().xscale, "log");
    EXPECT_EQ(ax().yscale, "linear");
}

TEST_F(LogScaleTest, SemilogySetsYscale)
{
    eval("figure(1); semilogy([1 2 3],[1 10 100]);");
    EXPECT_EQ(ax().xscale, "linear");
    EXPECT_EQ(ax().yscale, "log");
}

TEST_F(LogScaleTest, LoglogSetsBoth)
{
    eval("figure(1); loglog([1 10 100],[1 10 100]);");
    EXPECT_EQ(ax().xscale, "log");
    EXPECT_EQ(ax().yscale, "log");
}

// ============================================================
// Axis modes
// ============================================================

class AxisModeTest : public FigureEngineTest {};

TEST_F(AxisModeTest, AxisEqual)
{
    eval("figure(1); plot([1 2],[1 2]); axis('equal')");
    EXPECT_EQ(ax().axisMode, "equal");
}

TEST_F(AxisModeTest, AxisTight)
{
    eval("figure(1); plot([1 2],[1 2]); axis('tight')");
    EXPECT_EQ(ax().axisMode, "tight");
}

TEST_F(AxisModeTest, AxisIJCommandStyle)
{
    eval("figure(1); plot([1 2],[1 2]); axis ij");
    EXPECT_EQ(ax().axisMode, "ij");
}

TEST_F(AxisModeTest, AxisXYCommandStyle)
{
    eval("figure(1); plot([1 2],[1 2]); axis xy");
    EXPECT_EQ(ax().axisMode, "xy");
}

// ============================================================
// Labels, legend, limits
// ============================================================

class PlotLabelsTest : public FigureEngineTest {};

TEST_F(PlotLabelsTest, TitleSetsTitle)
{
    eval("figure(1); plot([1 2],[1 2]); title('My Plot')");
    EXPECT_EQ(ax().title, "My Plot");
}

TEST_F(PlotLabelsTest, XlabelSetsLabel)
{
    eval("figure(1); plot([1 2],[1 2]); xlabel('X axis')");
    EXPECT_EQ(ax().xlabel, "X axis");
}

TEST_F(PlotLabelsTest, YlabelSetsLabel)
{
    eval("figure(1); plot([1 2],[1 2]); ylabel('Y axis')");
    EXPECT_EQ(ax().ylabel, "Y axis");
}

TEST_F(PlotLabelsTest, LegendSetsLabels)
{
    eval("figure(1); plot([1 2],[1 2]); hold on; plot([1 2],[2 4]);");
    eval("legend('A', 'B')");
    ASSERT_EQ(ax().legendLabels.size(), 2u);
    EXPECT_EQ(ax().legendLabels[0], "A");
    EXPECT_EQ(ax().legendLabels[1], "B");
}

TEST_F(PlotLabelsTest, XlimSetsLimits)
{
    eval("figure(1); plot([1 2],[1 2]); xlim([0 10])");
    EXPECT_FALSE(ax().xlimJson.empty());
}

TEST_F(PlotLabelsTest, YlimSetsLimits)
{
    eval("figure(1); plot([1 2],[1 2]); ylim([-5 5])");
    EXPECT_FALSE(ax().ylimJson.empty());
}

// ============================================================
// Plot style strings and name-value pairs
// ============================================================

class PlotStyleTest : public FigureEngineTest {};

TEST_F(PlotStyleTest, StyleStringStored)
{
    eval("figure(1); plot([1 2 3],[1 4 9],'r--o');");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].style, "r--o");
}

TEST_F(PlotStyleTest, LineWidthDoesNotCrash)
{
    eval("figure(1); plot([1 2 3],[1 4 9],'b-','LineWidth',3);");
    EXPECT_EQ(ax().datasets.size(), 1u);
}

TEST_F(PlotStyleTest, MarkerSizeDoesNotCrash)
{
    eval("figure(1); plot([1 2 3],[1 4 9],'ro','MarkerSize',8);");
    EXPECT_EQ(ax().datasets.size(), 1u);
}

// ============================================================
// Polar plot configuration
// ============================================================

class PolarTest : public FigureEngineTest {};

TEST_F(PolarTest, PolarplotSetsPolarFlag)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    EXPECT_TRUE(ax().polar);
}

TEST_F(PolarTest, ThetadirClockwise)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    eval("thetadir('clockwise')");
    EXPECT_EQ(ax().thetaDir, "clockwise");
}

TEST_F(PolarTest, ThetadirCounterclockwise)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    eval("thetadir('counterclockwise')");
    EXPECT_EQ(ax().thetaDir, "counterclockwise");
}

TEST_F(PolarTest, ThetazeroTop)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    eval("thetazero('top')");
    EXPECT_EQ(ax().thetaZeroLocation, "top");
}

TEST_F(PolarTest, ThetazeroCommandStyle)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    eval("thetazero top");
    EXPECT_EQ(ax().thetaZeroLocation, "top");
}

TEST_F(PolarTest, RlimSetsLimits)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    eval("rlim([0 2])");
    EXPECT_FALSE(ax().rlimJson.empty());
}

// ============================================================
// Plot type/axes replacement without hold (MATLAB behavior)
// ============================================================

class PlotReplaceTest : public FigureEngineTest {};

TEST_F(PlotReplaceTest, PlotReplacesPolarplot)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    EXPECT_TRUE(ax().polar);
    EXPECT_EQ(ax().datasets[0].type, "line");

    eval("plot([1 2 3], [4 5 6]);");
    EXPECT_FALSE(ax().polar) << "plot() should switch axes back to cartesian";
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "line");
}

TEST_F(PlotReplaceTest, PolarplotReplacesPlot)
{
    eval("figure(1); plot([1 2 3], [4 5 6]);");
    EXPECT_FALSE(ax().polar);

    eval("polarplot(linspace(0,6.28,63), ones(1,63));");
    EXPECT_TRUE(ax().polar) << "polarplot() should switch axes to polar";
    ASSERT_EQ(ax().datasets.size(), 1u);
}

TEST_F(PlotReplaceTest, BarReplacesPlot)
{
    eval("figure(1); plot([1 2 3], [4 5 6]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "line");

    eval("bar([10 20 30]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "bar");
}

TEST_F(PlotReplaceTest, ScatterReplacesBar)
{
    eval("figure(1); bar([1 2 3]);");
    EXPECT_EQ(ax().datasets[0].type, "bar");

    eval("scatter([1 2 3], [4 5 6]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "scatter");
}

TEST_F(PlotReplaceTest, StemReplacesScatter)
{
    eval("figure(1); scatter([1 2 3], [4 5 6]);");
    EXPECT_EQ(ax().datasets[0].type, "scatter");

    eval("stem([1 2 3], [7 8 9]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "stem");
}

TEST_F(PlotReplaceTest, StairsReplacesPlot)
{
    eval("figure(1); plot([1 2 3], [4 5 6]);");
    EXPECT_EQ(ax().datasets[0].type, "line");

    eval("stairs([1 2 3], [7 8 9]);");
    ASSERT_EQ(ax().datasets.size(), 1u);
    EXPECT_EQ(ax().datasets[0].type, "stairs");
}

TEST_F(PlotReplaceTest, PlotReplacesBarClearsConfig)
{
    eval("figure(1); bar([1 2 3]); title('old'); xlabel('x'); grid on;");
    EXPECT_EQ(ax().title, "old");
    EXPECT_EQ(ax().gridMode, "on");

    eval("plot([1 2], [3 4]);");
    EXPECT_EQ(ax().title, "") << "title should be cleared without hold";
    EXPECT_EQ(ax().xlabel, "") << "xlabel should be cleared without hold";
    EXPECT_EQ(ax().gridMode, "") << "grid should be cleared without hold";
}

TEST_F(PlotReplaceTest, PlotReplacesClearsLimits)
{
    eval("figure(1); plot([1 2],[3 4]); xlim([0 10]); ylim([-1 1]);");
    EXPECT_FALSE(ax().xlimJson.empty());
    EXPECT_FALSE(ax().ylimJson.empty());

    eval("bar([5 6 7]);");
    EXPECT_TRUE(ax().xlimJson.empty()) << "xlim should be cleared without hold";
    EXPECT_TRUE(ax().ylimJson.empty()) << "ylim should be cleared without hold";
}

TEST_F(PlotReplaceTest, PlotReplacesClearsLogScale)
{
    eval("figure(1); semilogy([1 2 3], [10 100 1000]);");
    EXPECT_EQ(ax().yscale, "log");

    eval("plot([1 2 3], [4 5 6]);");
    EXPECT_EQ(ax().yscale, "linear") << "yscale should reset to linear without hold";
    EXPECT_EQ(ax().xscale, "linear");
}

TEST_F(PlotReplaceTest, HoldOnPreservesTypeAndConfig)
{
    eval("figure(1); polarplot(linspace(0,6.28,63), ones(1,63));");
    eval("title('polar'); grid on; hold on;");
    EXPECT_TRUE(ax().polar);

    eval("polarplot(linspace(0,6.28,63), 2*ones(1,63));");
    EXPECT_TRUE(ax().polar) << "hold on should preserve polar";
    EXPECT_EQ(ax().title, "polar") << "hold on should preserve title";
    EXPECT_EQ(ax().gridMode, "on") << "hold on should preserve grid";
    EXPECT_EQ(ax().datasets.size(), 2u) << "hold on should accumulate datasets";
}

// ============================================================
// Subplot
// ============================================================

class SubplotEngineTest : public FigureEngineTest {};

TEST_F(SubplotEngineTest, CreatesGrid)
{
    eval("figure(1); subplot(2,1,1);");
    auto &fig = fm().current();
    EXPECT_EQ(fig.subplotRows, 2);
    EXPECT_EQ(fig.subplotCols, 1);
}

TEST_F(SubplotEngineTest, SwitchesAxes)
{
    eval("figure(1);");
    eval("subplot(2,1,1); title('Top');");
    eval("subplot(2,1,2); title('Bottom');");
    eval("subplot(2,1,1);");
    EXPECT_EQ(ax().title, "Top");
}

TEST_F(SubplotEngineTest, IndependentGridModes)
{
    eval("figure(1);");
    eval("subplot(1,2,1); grid on;");
    eval("subplot(1,2,2);");
    EXPECT_EQ(ax().gridMode, ""); // second subplot has no grid
}

TEST_F(SubplotEngineTest, IndependentHold)
{
    eval("figure(1);");
    eval("subplot(1,2,1); hold on;");
    eval("subplot(1,2,2);");
    EXPECT_FALSE(ax().holdOn); // second subplot has hold off
}

TEST_F(SubplotEngineTest, PlotIntoSubplots)
{
    eval("figure(1);");
    eval("subplot(2,1,1); plot([1 2],[1 2]);");
    eval("subplot(2,1,2); stem([1 2 3],[1 4 9]);");

    auto &fig = fm().current();
    bool foundLine = false, foundStem = false;
    for (auto &a : fig.axes) {
        if (a.subplotIndex == 1 && !a.datasets.empty() && a.datasets[0].type == "line")
            foundLine = true;
        if (a.subplotIndex == 2 && !a.datasets.empty() && a.datasets[0].type == "stem")
            foundStem = true;
    }
    EXPECT_TRUE(foundLine);
    EXPECT_TRUE(foundStem);
}

// ============================================================
// Integration: realistic workflows
// ============================================================

class FigureIntegrationTest : public FigureEngineTest {};

TEST_F(FigureIntegrationTest, FullPlotWorkflow)
{
    eval(R"(
        figure(1);
        x = linspace(0, 2*pi, 100);
        plot(x, sin(x), 'b-');
        title('Trig Functions');
        xlabel('x'); ylabel('y');
        grid on; hold on;
        plot(x, cos(x), 'r--');
        legend('sin', 'cos');
    )");
    EXPECT_EQ(ax().title, "Trig Functions");
    EXPECT_EQ(ax().xlabel, "x");
    EXPECT_EQ(ax().ylabel, "y");
    EXPECT_EQ(ax().gridMode, "on");
    EXPECT_TRUE(ax().holdOn);
    EXPECT_EQ(ax().datasets.size(), 2u);
    ASSERT_EQ(ax().legendLabels.size(), 2u);
    EXPECT_EQ(ax().legendLabels[0], "sin");
    EXPECT_EQ(ax().legendLabels[1], "cos");
}

TEST_F(FigureIntegrationTest, SubplotWorkflow)
{
    eval(R"(
        figure(1);
        subplot(2,1,1);
        plot([1 2 3],[1 4 9]); title('Quadratic'); grid on;
        subplot(2,1,2);
        stem([1 2 3 4],[1 8 27 64]); title('Cubic');
    )");
    auto &fig = fm().current();
    EXPECT_EQ(fig.subplotRows, 2);
    EXPECT_EQ(fig.subplotCols, 1);

    bool topOk = false, bottomOk = false;
    for (auto &a : fig.axes) {
        if (a.subplotIndex == 1 && a.title == "Quadratic" && a.gridMode == "on")
            topOk = true;
        if (a.subplotIndex == 2 && a.title == "Cubic" &&
            !a.datasets.empty() && a.datasets[0].type == "stem")
            bottomOk = true;
    }
    EXPECT_TRUE(topOk);
    EXPECT_TRUE(bottomOk);
}

TEST_F(FigureIntegrationTest, MultiFigure)
{
    eval(R"(
        figure(1); plot([1 2],[1 2]); title('Linear');
        figure(2); bar([1 2 3 4]); title('Bars');
        figure(1);
    )");
    EXPECT_EQ(fm().figures().size(), 2u);
    EXPECT_EQ(fm().currentFigureId(), 1);
    EXPECT_EQ(ax().title, "Linear");
}

TEST_F(FigureIntegrationTest, CloseAndReuseId)
{
    eval("figure(1); figure(2); figure(3);");
    eval("close(2);");
    eval("figure;"); // should get ID 2 (min free)
    EXPECT_EQ(fm().currentFigureId(), 2);
    EXPECT_EQ(fm().figures().size(), 3u);
}

TEST_F(FigureIntegrationTest, ClearAllThenPlot)
{
    eval(R"(
        figure(1); plot([1 2],[1 2]);
        clear all;
        figure(1); plot([3 4],[3 4]);
    )");
    EXPECT_EQ(fm().figures().size(), 1u);
    EXPECT_EQ(ax().datasets.size(), 1u);
}

// ============================================================
// imagesc — large-matrix tile pipeline (downsample contract)
// ============================================================

class ImagescTileTest : public FigureEngineTest {};

TEST_F(ImagescTileTest, SmallMatrixNoDownsample)
{
    // 100×100 = 10K cells, well below the 2M cap → full-resolution path,
    // downsampled flag stays false, zRaw still populated for tile fetches.
    eval("imagesc(eye(100));");
    ASSERT_EQ(ax().datasets.size(), 1u);
    const auto &ds = ax().datasets[0];
    EXPECT_EQ(ds.type, "imagesc");
    EXPECT_FALSE(ds.downsampled);
    EXPECT_EQ(ds.zRaw.size(), 100u * 100u);
    // eye(100): col-major, [0,0] = 1, [0,1] = 0, [1,1] = 1 (diagonal).
    EXPECT_FLOAT_EQ(ds.zRaw[0], 1.0f);                 // (1,1) on diagonal
    EXPECT_FLOAT_EQ(ds.zRaw[1], 0.0f);                 // (2,1) off-diagonal
    EXPECT_FLOAT_EQ(ds.zRaw[100 + 1], 1.0f);           // (2,2) on diagonal
}

TEST_F(ImagescTileTest, OversizedMatrixDownsamples)
{
    // 3000×3000 = 9M cells, above the 2M cap → mean-pool path triggers,
    // downsampled flag set, originalRows/Cols preserved.
    eval("imagesc(ones(3000));");
    ASSERT_EQ(ax().datasets.size(), 1u);
    const auto &ds = ax().datasets[0];
    EXPECT_TRUE(ds.downsampled);
    EXPECT_EQ(ds.originalRows, 3000u);
    EXPECT_EQ(ds.originalCols, 3000u);
    // zRaw still holds full resolution — Phase 2 tile-fetch reads from it.
    EXPECT_EQ(ds.zRaw.size(), 3000u * 3000u);
}

TEST_F(ImagescTileTest, MeanPoolPreservesUniformValues)
{
    // ones(3000) → every block average is 1.0, regardless of pool factor.
    // Sanity check that mean-pool isn't accidentally summing/clamping.
    eval("imagesc(ones(3000));");
    const auto &ds = ax().datasets[0];
    ASSERT_TRUE(ds.downsampled);
    // zJson should be a 2D JSON array of all 1s. Spot-check by counting
    // commas at top level vs row count — N rows should produce N-1 row separators.
    // Easier: the JSON should contain neither "0" nor "2" as standalone values.
    EXPECT_NE(ds.zJson.find('1'), std::string::npos);
    EXPECT_EQ(ds.zJson.find("[2"), std::string::npos);
    EXPECT_EQ(ds.zJson.find(",2"), std::string::npos);
}

TEST_F(ImagescTileTest, JsonExposesDownsampleFields)
{
    // The IDE adapter reads downsampled / originalRows / originalCols off the
    // dataset JSON. Verify they appear in the emitted figure marker.
    eval("imagesc(ones(3000));");
    EXPECT_NE(capturedOutput.find("\"downsampled\":true"), std::string::npos);
    EXPECT_NE(capturedOutput.find("\"originalRows\":3000"), std::string::npos);
    EXPECT_NE(capturedOutput.find("\"originalCols\":3000"), std::string::npos);
}

TEST_F(ImagescTileTest, SmallMatrixOmitsDownsampleFields)
{
    // No downsample flag for matrices ≤2M cells — keep the JSON minimal so
    // existing parity / golden tests stay byte-identical.
    eval("imagesc(eye(100));");
    EXPECT_EQ(capturedOutput.find("\"downsampled\""), std::string::npos);
    EXPECT_EQ(capturedOutput.find("\"originalRows\""), std::string::npos);
}

// ============================================================
// FigureManager::getFigureTile — sub-rect read with LOD pooling
// ============================================================

class FigureTileTest : public FigureEngineTest {};

TEST_F(FigureTileTest, FullExtentLod1)
{
    // 100×100 eye matrix, fetch the full extent at lod=1 — should match
    // zRaw exactly (column-major source → row-major output).
    eval("imagesc(eye(100));");
    int figId = fm().currentFigureId();
    std::vector<float> tile;
    size_t oRows = 0, oCols = 0;
    bool ok = fm().getFigureTile(figId, 0, 0, 0, 0, 100, 100, 1, tile, oRows, oCols);
    ASSERT_TRUE(ok);
    EXPECT_EQ(oRows, 100u);
    EXPECT_EQ(oCols, 100u);
    EXPECT_EQ(tile.size(), 100u * 100u);
    // Row 0, col 0 of eye(100) is 1; row 0, col 1 is 0.
    EXPECT_FLOAT_EQ(tile[0], 1.0f);
    EXPECT_FLOAT_EQ(tile[1], 0.0f);
    // Row 1, col 1 is 1.
    EXPECT_FLOAT_EQ(tile[1 * 100 + 1], 1.0f);
}

TEST_F(FigureTileTest, SubRectExtractsCorrectRegion)
{
    // ones(3000), fetch a 200×200 region from offset (1000, 1000) — every
    // value is 1, output mean is 1.
    eval("imagesc(ones(3000));");
    int figId = fm().currentFigureId();
    std::vector<float> tile;
    size_t oRows = 0, oCols = 0;
    bool ok = fm().getFigureTile(figId, 0, 0, 1000, 1000, 200, 200, 1, tile, oRows, oCols);
    ASSERT_TRUE(ok);
    EXPECT_EQ(oRows, 200u);
    EXPECT_EQ(oCols, 200u);
    for (float v : tile) EXPECT_FLOAT_EQ(v, 1.0f);
}

TEST_F(FigureTileTest, LodPoolingHalvesDimensions)
{
    // ones(1000), fetch full extent at lod=4 — expect 250×250 output, all 1s.
    eval("imagesc(ones(1000));");
    int figId = fm().currentFigureId();
    std::vector<float> tile;
    size_t oRows = 0, oCols = 0;
    bool ok = fm().getFigureTile(figId, 0, 0, 0, 0, 1000, 1000, 4, tile, oRows, oCols);
    ASSERT_TRUE(ok);
    EXPECT_EQ(oRows, 250u);
    EXPECT_EQ(oCols, 250u);
    for (float v : tile) EXPECT_FLOAT_EQ(v, 1.0f);
}

TEST_F(FigureTileTest, OutOfRangeReturnsFalse)
{
    eval("imagesc(eye(100));");
    int figId = fm().currentFigureId();
    std::vector<float> tile;
    size_t oRows = 0, oCols = 0;
    EXPECT_FALSE(fm().getFigureTile(figId, 0, 0, 200, 0, 10, 10, 1, tile, oRows, oCols));
    EXPECT_FALSE(fm().getFigureTile(figId, 0, 0, 0, 0, 10, 10, 1, tile, oRows, oCols)
                 == false  /* on-range succeeds */);
}

TEST_F(FigureTileTest, NonImagescReturnsFalse)
{
    // plot(...) creates a line dataset, not imagesc — getFigureTile should refuse.
    eval("plot([1 2 3], [1 4 9]);");
    int figId = fm().currentFigureId();
    std::vector<float> tile;
    size_t oRows = 0, oCols = 0;
    EXPECT_FALSE(fm().getFigureTile(figId, 0, 0, 0, 0, 1, 1, 1, tile, oRows, oCols));
}

TEST_F(FigureTileTest, OversizedMatrixServesFullResolutionTile)
{
    // After Phase 1 cap, a 3000×3000 imagesc has a downsampled JSON preview
    // but zRaw still holds the full 9M cells. A tile request reads zRaw,
    // not the preview — so we get exact data even when the preview is mean-pooled.
    eval("imagesc(ones(3000));");
    int figId = fm().currentFigureId();
    std::vector<float> tile;
    size_t oRows = 0, oCols = 0;
    bool ok = fm().getFigureTile(figId, 0, 0, 1500, 1500, 100, 100, 1, tile, oRows, oCols);
    ASSERT_TRUE(ok);
    EXPECT_EQ(oRows, 100u);
    EXPECT_EQ(oCols, 100u);
    for (float v : tile) EXPECT_FLOAT_EQ(v, 1.0f);
}