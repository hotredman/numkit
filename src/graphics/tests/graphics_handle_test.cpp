// tests/graphics/graphics_handle_test.cpp
//
// Graphics-handle layer (portion 29, bugs/closed/graphics/plot-family-no-
// return-value.md follow-up):
//   • chart handles: class(), set/get, dot-syntax, eq, delete/invalidation
//   • gca/gcf/figure handles and their property surfaces
//   • the unified type-dispatched delete (handle vs file)
//   • plot(x,y,x2,y2) multi-series → 1×2 Line array
// Parameterized over BOTH backends (TreeWalker + VM) per the portion-close
// coverage gate — the handle path exercises external calls, dot-access
// dispatch and object ops, which differ per engine.

#include "dual_engine_fixture.hpp"

#include <numkit/figure/figure_manager.hpp>

using namespace m_test;

namespace {

class GraphicsHandleTest : public DualEngineTest
{};

} // namespace

// ── Chart handles ────────────────────────────────────────────────────

TEST_P(GraphicsHandleTest, PlotReturnsLineHandleWithMatlabClassName)
{
    eval("h = plot(1:5);");
    EXPECT_EQ(evalString("class(h)"),
              "matlab.graphics.chart.primitive.Line");
    EXPECT_TRUE(evalBool("isgraphics(h)"));
    EXPECT_TRUE(evalBool("ishandle(h)"));
    EXPECT_TRUE(evalBool("isa(h, 'handle')"));
}

TEST_P(GraphicsHandleTest, GetSetLineWidthRoundTrip)
{
    eval("h = plot(1:5);");
    EXPECT_NEAR(evalScalar("get(h,'LineWidth')"), 0.5, 1e-12);
    eval("set(h,'LineWidth',2);");
    EXPECT_NEAR(evalScalar("get(h,'LineWidth')"), 2.0, 1e-12);
    // Render-effect: the dataset carries the new width.
    EXPECT_NEAR(ax().datasets.back().lineWidth, 2.0, 1e-12);
}

TEST_P(GraphicsHandleTest, DotSyntaxReadsAndWrites)
{
    eval("h = plot(1:5);");
    eval("h.LineWidth = 3;");
    EXPECT_NEAR(evalScalar("h.LineWidth"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("get(h,'LineWidth')"), 3.0, 1e-12);
    EXPECT_EQ(evalString("h.Type"), "line");
    EXPECT_EQ(evalString("h.Marker"), "none");
    EXPECT_NEAR(evalScalar("h.MarkerSize"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("h.Color(2)"), 0.4470, 1e-3);
}

TEST_P(GraphicsHandleTest, StylePropsRoundTripAndRender)
{
    eval("h = plot(1:5);");
    eval("set(h,'LineStyle','--','Marker','o');");
    EXPECT_EQ(evalString("get(h,'LineStyle')"), "--");
    EXPECT_EQ(evalString("get(h,'Marker')"), "o");
    // Rebuilt style string (kv dialect) carries both components.
    const std::string &style = ax().datasets.back().style;
    EXPECT_NE(style.find("lineStyle=--"), std::string::npos);
    EXPECT_NE(style.find("marker=o"), std::string::npos);
}

TEST_P(GraphicsHandleTest, VisibleOffHidesDataset)
{
    eval("h = plot(1:5);");
    eval("set(h,'Visible','off');");
    EXPECT_FALSE(ax().datasets.back().visible);
    EXPECT_EQ(evalString("get(h,'Visible')"), "off");
    eval("set(h,'Visible','on');");
    EXPECT_TRUE(ax().datasets.back().visible);
}

TEST_P(GraphicsHandleTest, XyDataRoundTrip)
{
    eval("h = plot(1:5);");
    eval("set(h,'YData',[10 20 30 40 50]);");
    EXPECT_NEAR(evalScalar("get(h,'YData')(3)"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("h.XData(4)"), 4.0, 1e-12);
}

TEST_P(GraphicsHandleTest, HandleIdentityEq)
{
    eval("hold on; h1 = plot(1:5); h2 = plot(1:5); h3 = h1;");
    EXPECT_TRUE(evalBool("h1 == h3"));
    EXPECT_TRUE(evalBool("h1 ~= h2"));
    EXPECT_FALSE(evalBool("h1 == h2"));
    EXPECT_FALSE(evalBool("h1 == 5"));
}

TEST_P(GraphicsHandleTest, DeleteChartRemovesDatasetAndInvalidates)
{
    eval("hold on; h1 = plot(1:5); h2 = plot(1:5);");
    ASSERT_EQ(ax().datasets.size(), 2u);
    eval("delete(h1);");
    EXPECT_EQ(ax().datasets.size(), 1u);
    EXPECT_FALSE(evalBool("ishandle(h1)"));
    EXPECT_FALSE(evalBool("isvalid(h1)"));
    EXPECT_FALSE(evalBool("isgraphics(h1)"));
    // The surviving handle still works and points at the shifted slot.
    EXPECT_TRUE(evalBool("ishandle(h2)"));
    EXPECT_NEAR(evalScalar("get(h2,'LineWidth')"), 0.5, 1e-12);
    // Operations on a deleted handle error, MATLAB-style.
    EXPECT_THROW(eval("get(h1,'LineWidth');"), std::runtime_error);
    EXPECT_THROW(eval("set(h1,'LineWidth',1);"), std::runtime_error);
}

TEST_P(GraphicsHandleTest, HoldOffPlotInvalidatesPreviousHandle)
{
    eval("h1 = plot(1:5);");
    eval("h2 = plot(1:5);");   // hold off → fresh axes → h1 dies
    EXPECT_FALSE(evalBool("ishandle(h1)"));
    EXPECT_TRUE(evalBool("ishandle(h2)"));
    EXPECT_NEAR(evalScalar("get(h2,'LineWidth')"), 0.5, 1e-12);
}

TEST_P(GraphicsHandleTest, MultiSeriesPlotReturnsHandleArray)
{
    eval("hs = plot(1:5, 1:5, 1:5, 2:6);");
    EXPECT_EQ(ax().datasets.size(), 2u);
    EXPECT_NEAR(evalScalar("numel(hs)"), 2.0, 1e-12);
    EXPECT_EQ(evalString("class(hs(2))"),
              "matlab.graphics.chart.primitive.Line");
    // Per-handle isolation: setting one leaves the other at default.
    eval("set(hs(2),'LineWidth',4);");
    EXPECT_NEAR(evalScalar("get(hs(1),'LineWidth')"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("get(hs(2),'LineWidth')"), 4.0, 1e-12);
}

TEST_P(GraphicsHandleTest, GetHandleReturnsStruct)
{
    eval("h = plot(1:5); s = get(h);");
    EXPECT_NEAR(evalScalar("s.LineWidth"), 0.5, 1e-12);
    EXPECT_EQ(evalString("s.Type"), "line");
    EXPECT_EQ(evalString("s.Visible"), "on");
}

TEST_P(GraphicsHandleTest, ScatterDefaults)
{
    eval("h = scatter(1:5, 1:5);");
    EXPECT_EQ(evalString("class(h)"),
              "matlab.graphics.chart.scatter.Scatter");
    EXPECT_EQ(evalString("get(h,'Marker')"), "o");
    EXPECT_NEAR(evalScalar("get(h,'SizeData')"), 36.0, 1e-12);
}

// ── gca / gcf / figure ───────────────────────────────────────────────

TEST_P(GraphicsHandleTest, GcaReturnsAxesHandle)
{
    eval("h = plot(1:5); a = gca;");
    EXPECT_EQ(evalString("class(a)"), "matlab.graphics.axis.Axes");
    EXPECT_EQ(evalString("a.Type"), "axes");
    EXPECT_TRUE(evalBool("isgraphics(a)"));
    // Chart → parent axes → containing figure.
    EXPECT_NEAR(evalScalar("h.Parent.Parent.Number"), 1.0, 1e-12);
}

TEST_P(GraphicsHandleTest, GcfReturnsFigureHandle)
{
    eval("f = gcf;");
    EXPECT_EQ(evalString("class(f)"), "matlab.ui.Figure");
    EXPECT_EQ(evalString("f.Type"), "figure");
    EXPECT_NEAR(evalScalar("f.Number"), 1.0, 1e-12);
}

TEST_P(GraphicsHandleTest, FigureReturnsHandleAndNumbersSequentially)
{
    eval("f1 = figure(); f2 = figure();");
    EXPECT_NEAR(evalScalar("f1.Number"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("f2.Number"), 2.0, 1e-12);
    EXPECT_EQ(evalString("class(f1)"), "matlab.ui.Figure");
    // gcf tracks the most recent figure. (Temp var: the dotted rvalue
    // `gcf.Number` itself is a filed VM/TW divergence —
    // bugs/opened/lang/dotted-rvalue-on-call-result.md.)
    eval("figure(f1); g = gcf;");
    EXPECT_NEAR(evalScalar("g.Number"), 1.0, 1e-12);
}

TEST_P(GraphicsHandleTest, AxesPropsRenderEffect)
{
    eval("a = gca;");
    eval("set(a,'XLim',[0 10]);");
    EXPECT_EQ(ax().xlimJson, "[0,10]");
    Value v = eval("get(a,'XLim');");
    ASSERT_EQ(v.numel(), 2u);
    EXPECT_NEAR(v.elemAsDouble(0), 0.0, 1e-12);
    EXPECT_NEAR(v.elemAsDouble(1), 10.0, 1e-12);
    eval("set(a,'Title','My Title');");
    EXPECT_EQ(ax().title, "My Title");
    EXPECT_EQ(evalString("get(a,'Title')"), "My Title");
}

TEST_P(GraphicsHandleTest, CloseFigureHandleInvalidatesChildren)
{
    eval("f1 = figure(); h1 = plot(1:5);");
    eval("close(f1);");
    EXPECT_FALSE(evalBool("ishandle(h1)"));
    EXPECT_FALSE(evalBool("ishandle(f1)"));
    EXPECT_THROW(eval("get(h1,'LineWidth');"), std::runtime_error);
}

TEST_P(GraphicsHandleTest, ClfKeepsFigureHandleKillsCharts)
{
    eval("f = gcf; h = plot(1:5); clf;");
    EXPECT_TRUE(evalBool("ishandle(f)"));
    EXPECT_FALSE(evalBool("ishandle(h)"));
}

// ── Unified delete ───────────────────────────────────────────────────

TEST_P(GraphicsHandleTest, DeleteStringStillRoutesToFileBranch)
{
    // A non-handle argument keeps the file-deletion contract: an
    // unresolvable path errors (proves the handle branch didn't eat it).
    EXPECT_THROW(eval("delete('no_such_file_zz.m');"), std::runtime_error);
}

INSTANTIATE_DUAL(GraphicsHandleTest);
