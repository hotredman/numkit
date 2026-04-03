// tests/test_comments.cpp — Comment handling in various contexts
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

class CommentTest : public DualEngineTest {};

TEST_P(CommentTest, AssignWithTrailingComment)
{
    eval("c = 1500; % speed of sound");
    EXPECT_DOUBLE_EQ(getVar("c"), 1500.0);
}

TEST_P(CommentTest, AssignWithTrailingCommentUTF8)
{
    eval("c = 1500; % \xd1\x81\xd0\xba\xd0\xbe\xd1\x80\xd0\xbe\xd1\x81\xd1\x82\xd1\x8c "
         "\xd0\xb7\xd0\xb2\xd1\x83\xd0\xba\xd0\xb0, \xd0\xbc/\xd1\x81");
    EXPECT_DOUBLE_EQ(getVar("c"), 1500.0);
}

TEST_P(CommentTest, AssignWithoutSemicolonAndComment)
{
    eval("x = 42 % no semicolon");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
    EXPECT_FALSE(capturedOutput.empty());
}

TEST_P(CommentTest, AssignWithSemicolonSuppressesOutput)
{
    eval("x = 42; % with semicolon");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
    EXPECT_TRUE(capturedOutput.empty());
}

TEST_P(CommentTest, MultipleAssignsWithComments)
{
    eval(R"(
        c     = 1500;       % speed of sound, m/s
        N     = 8;          % number of elements
        f     = 10000;      % frequency, Hz
        d_lambda = 0.5;     % spacing in wavelengths
    )");
    EXPECT_DOUBLE_EQ(getVar("c"), 1500.0);
    EXPECT_DOUBLE_EQ(getVar("N"), 8.0);
    EXPECT_DOUBLE_EQ(getVar("f"), 10000.0);
    EXPECT_DOUBLE_EQ(getVar("d_lambda"), 0.5);
}

TEST_P(CommentTest, ExpressionWithComment)
{
    eval(R"(
        c = 1500;       % speed
        f = 10000;      % frequency
        lambda = c / f; % wavelength
    )");
    EXPECT_DOUBLE_EQ(getVar("lambda"), 0.15);
}

TEST_P(CommentTest, ChainedComputationsWithComments)
{
    eval(R"(
        c     = 1500;           % speed of sound
        f     = 10000;          % frequency
        d_lam = 0.5;            % spacing
        lambda = c / f;         % wavelength
        d = d_lam * lambda;     % element spacing
    )");
    EXPECT_DOUBLE_EQ(getVar("lambda"), 0.15);
    EXPECT_DOUBLE_EQ(getVar("d"), 0.075);
}

TEST_P(CommentTest, SectionComments)
{
    eval(R"(
        %% Section 1: Parameters
        a = 10;
        %% Section 2: Derived
        b = a * 2;
    )");
    EXPECT_DOUBLE_EQ(getVar("a"), 10.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 20.0);
}

TEST_P(CommentTest, HeaderCommentsBeforeCode)
{
    eval(R"(
        % =====================
        % My cool script
        % =====================
        x = 99;
    )");
    EXPECT_DOUBLE_EQ(getVar("x"), 99.0);
}

TEST_P(CommentTest, BlockCommentBetweenCode)
{
    eval("a = 1;\n"
         "%{\n"
         "This block comment\n"
         "spans multiple lines\n"
         "%}\n"
         "b = 2;\n");
    EXPECT_DOUBLE_EQ(getVar("a"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 2.0);
}

TEST_P(CommentTest, CommentInsideIf)
{
    eval(R"(
        x = 5;
        if x > 0
            % positive branch
            y = 1;
        else
            % negative branch
            y = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
}

TEST_P(CommentTest, CommentInsideFor)
{
    eval(R"(
        s = 0;
        for i = 1:5
            % accumulate
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 15.0);
}

TEST_P(CommentTest, CommentAfterForHeader)
{
    eval(R"(
        s = 0;
        for i = 1:3 % iterate
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 6.0);
}

TEST_P(CommentTest, CommentInsideWhile)
{
    eval(R"(
        x = 10;
        while x > 0
            % decrement
            x = x - 3;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("x"), -2.0);
}

TEST_P(CommentTest, CommentInsideSwitch)
{
    eval(R"(
        x = 2;
        switch x
            case 1
                % first
                y = 10;
            case 2
                % second
                y = 20;
            otherwise
                % default
                y = 0;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 20.0);
}

TEST_P(CommentTest, CommentInsideFunction)
{
    eval(R"(
        function y = square(x)
            % Computes the square
            y = x^2;
        end
    )");
    eval("r = square(7);");
    EXPECT_DOUBLE_EQ(getVar("r"), 49.0);
}

TEST_P(CommentTest, CommentBeforeAndInsideFunction)
{
    eval(R"(
        % Helper function for doubling
        function y = dbl(x)
            % double the input
            y = x * 2;
        end
    )");
    eval("r = dbl(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 10.0);
}

TEST_P(CommentTest, CommentInsideTryCatch)
{
    eval(R"(
        try
            % risky code
            x = 1;
        catch e
            % handle error
            x = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("x"), 1.0);
}

TEST_P(CommentTest, OnlyComments)
{
    EXPECT_NO_THROW(eval(R"(
        % just comments
        % nothing here
        %% section
    )"));
}

TEST_P(CommentTest, EmptyLinesAndComments)
{
    eval(R"(

        % comment

        x = 1;

        % another comment

        y = x + 1;

    )");
    EXPECT_DOUBLE_EQ(getVar("x"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

TEST_P(CommentTest, CommentWithOperatorsDoesNotAffectResult)
{
    eval(R"(
        a = 10; % a / b = ??? [not code]
        b = 20; % (a + b) * {c}
    )");
    EXPECT_DOUBLE_EQ(getVar("a"), 10.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 20.0);
}

TEST_P(CommentTest, RealisticScriptSnippet)
{
    eval(R"(
        %% Parameters
        c     = 1500;       % speed of sound, m/s
        N     = 8;          % number of elements
        f     = 10000;      % frequency, Hz
        d_lam = 0.5;        % spacing in wavelengths

        %% Derived quantities
        lambda = c / f;             % wavelength, m
        d      = d_lam * lambda;    % element spacing, m
        k      = 2 * pi / lambda;   % wave number, rad/m

        theta0 = 0;  % main lobe direction (degrees)
    )");

    EXPECT_DOUBLE_EQ(getVar("c"), 1500.0);
    EXPECT_DOUBLE_EQ(getVar("N"), 8.0);
    EXPECT_DOUBLE_EQ(getVar("f"), 10000.0);
    EXPECT_DOUBLE_EQ(getVar("d_lam"), 0.5);

    double lambda = 1500.0 / 10000.0;
    EXPECT_DOUBLE_EQ(getVar("lambda"), lambda);
    EXPECT_DOUBLE_EQ(getVar("d"), 0.5 * lambda);
    EXPECT_NEAR(getVar("k"), 2.0 * M_PI / lambda, 1e-10);
    EXPECT_DOUBLE_EQ(getVar("theta0"), 0.0);
}

INSTANTIATE_DUAL(CommentTest);
