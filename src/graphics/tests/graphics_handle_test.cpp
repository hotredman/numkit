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
    // Portion 31: label slots read back as Text handles (MATLAB).
    EXPECT_EQ(evalString("get(a,'Title').String"), "My Title");
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

// bugs/closed/graphics/subplot-replaces-default-axes.md — the first
// subplot must ADOPT (not ghost) the pre-existing default axes.
TEST_P(GraphicsHandleTest, SubplotAdoptsDefaultAxes)
{
    eval("plot(1:5); subplot(2,1,1); plot(1:5); subplot(2,1,2); plot(1:5);");
    EXPECT_EQ(evalScalar("numel(gca.Parent.Children)"), 2.0);
}

// ── Text handles (portion 31) ────────────────────────────────────────

TEST_P(GraphicsHandleTest, TitleReturnsTextHandle)
{
    eval("h = plot(1:5); hT = title('My Title');");
    EXPECT_EQ(evalString("class(hT)"), "matlab.graphics.primitive.Text");
    EXPECT_EQ(evalString("hT.String"), "My Title");
    EXPECT_NEAR(evalScalar("hT.FontSize"), 11.0, 1e-12);
    EXPECT_EQ(evalString("get(gca,'Title').String"), "My Title");
    // String write renders: the axes field is the source of truth.
    eval("hT.String = 'Changed';");
    EXPECT_EQ(ax().title, "Changed");
    EXPECT_EQ(evalString("get(gca,'Title').String"), "Changed");
    // set(gca,'Title',char) keeps working and the read stays a handle.
    eval("set(gca,'Title','By set');");
    EXPECT_EQ(ax().title, "By set");
    EXPECT_EQ(evalString("class(get(gca,'Title'))"),
              "matlab.graphics.primitive.Text");
}

TEST_P(GraphicsHandleTest, XLabelIsTextHandle)
{
    eval("hX = xlabel('X axis');");
    EXPECT_EQ(evalString("class(hX)"), "matlab.graphics.primitive.Text");
    EXPECT_EQ(evalString("get(gca,'XLabel').String"), "X axis");
    EXPECT_NEAR(evalScalar("get(gca,'XLabel').FontSize"), 11.0, 1e-12);
}

TEST_P(GraphicsHandleTest, SgtitleSurvivesAxesResetDiesWithClf)
{
    eval("sg = sgtitle('Super'); h1 = plot(1:5);");
    EXPECT_EQ(evalString("class(sg)"), "matlab.graphics.primitive.Text");
    EXPECT_EQ(evalString("sg.String"), "Super");
    eval("h2 = plot(1:5);");   // hold off → axes content reset, sgtitle lives
    EXPECT_TRUE(evalBool("ishandle(sg)"));
    EXPECT_TRUE(evalBool("ishandle(h2)"));
    EXPECT_FALSE(evalBool("ishandle(h1)"));
    eval("clf;");
    EXPECT_FALSE(evalBool("ishandle(sg)"));
    EXPECT_EQ(fm().current().superTitle, "");
}

TEST_P(GraphicsHandleTest, TitleHandleDiesWithClf)
{
    eval("hT = title('T'); clf;");
    EXPECT_FALSE(evalBool("ishandle(hT)"));
}

// ── Legend handle (portion 31) ───────────────────────────────────────

TEST_P(GraphicsHandleTest, LegendReturnsHandleWithProps)
{
    eval("h = plot(1:5); hL = legend('data1');");
    EXPECT_EQ(evalString("class(hL)"), "matlab.graphics.illustration.Legend");
    EXPECT_EQ(evalString("get(hL,'Location')"), "northeast");
    EXPECT_EQ(evalString("hL.Location"), "northeast");
    eval("hL.Location = 'south';");
    EXPECT_EQ(ax().legendLocation, "south");
    EXPECT_EQ(evalString("get(hL,'Location')"), "south");
    EXPECT_EQ(evalString("hL.String{1}"), "data1");
}

TEST_P(GraphicsHandleTest, LegendVisibleOffPreservesLabels)
{
    eval("h = plot(1:5); hL = legend('data1','data2');");
    ASSERT_EQ(ax().legendLabels.size(), 2u);
    eval("set(hL,'Visible','off');");
    EXPECT_TRUE(ax().legendHidden);
    EXPECT_EQ(ax().legendLabels.size(), 2u);   // preserved
    EXPECT_EQ(evalString("get(hL,'Visible')"), "off");
    eval("hL.Visible = 'on';");
    EXPECT_FALSE(ax().legendHidden);
    // The command form behaves identically.
    eval("legend off;");
    EXPECT_TRUE(ax().legendHidden);
    EXPECT_EQ(ax().legendLabels.size(), 2u);
    eval("legend show;");
    EXPECT_FALSE(ax().legendHidden);
}

// ── groot / parent chain / Children arrays (portion 31) ─────────────

TEST_P(GraphicsHandleTest, GrootAndFigureParent)
{
    eval("g = groot; f = gcf;");
    EXPECT_EQ(evalString("class(g)"), "matlab.ui.Root");
    EXPECT_EQ(evalString("g.Type"), "root");
    EXPECT_TRUE(evalBool("groot == g"));
    EXPECT_EQ(evalString("class(f.Parent)"), "matlab.ui.Root");
    EXPECT_TRUE(evalBool("f.Parent == g"));
}

TEST_P(GraphicsHandleTest, FigureChildrenIsFullAxesArray)
{
    eval("subplot(2,1,1); plot(1:5); subplot(2,1,2); plot(1:5);");
    Value ch = eval("gca.Parent.Children;");
    EXPECT_EQ(ch.numel(), 2u);
    EXPECT_EQ(evalString("class(gca.Parent.Children(2))"),
              "matlab.graphics.axis.Axes");
    // Children(2) IS the current axes handle (create-or-reuse identity).
    EXPECT_TRUE(evalBool("gca == gca.Parent.Children(2)"));
}
