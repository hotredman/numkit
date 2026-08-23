// toolboxes/builtin/tests/datevec_test.cpp
//
// Regression guard for datevec() — inverse of datenum.
// Round-trip exactness expected since the algorithm is the inverse
// of the same days_from_civil core used in datenum.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DatevecTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DatevecTest, RoundTripScalar)
{
    eval("v = datevec(datenum(2026, 5, 9));");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 2026.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),    5.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),    9.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"),    0.0);
}

TEST_F(DatevecTest, RoundTripWithTime)
{
    eval("v = datevec(datenum(2026, 5, 9, 12, 30, 45));");
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"), 45.0);
}

TEST_F(DatevecTest, FractionalSecondPreserved)
{
    eval("v = datevec(datenum(2026, 5, 9, 12, 30, 45.5));");
    EXPECT_NEAR(evalScalar("v(6)"), 45.5, 1e-6);
}

TEST_F(DatevecTest, UnixEpoch)
{
    eval("v = datevec(719529);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 1970.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),    1.0);
}

TEST_F(DatevecTest, YearZero)
{
    eval("v = datevec(1);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 1.0);
}

TEST_F(DatevecTest, EdgeZero)
{
    eval("v = datevec(0);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.0);
}

TEST_F(DatevecTest, FractionalDayHalf)
{
    eval("v = datevec(datenum(2026, 5, 9) + 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"),  0.0);
}

TEST_F(DatevecTest, FractionalDayQuarter)
{
    eval("v = datevec(datenum(2026, 5, 9) + 0.25);");
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"), 0.0);
}

TEST_F(DatevecTest, ColumnVectorInput)
{
    eval("M = datevec([datenum(2026,5,9); datenum(2026,5,10); "
         "             datenum(2026,5,11)]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M, 2)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,3)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,3)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(3,3)"), 11.0);
}

TEST_F(DatevecTest, MultiOutputForm)
{
    eval("[Y, Mo, D, H, MI, S] = datevec(datenum(2026, 5, 9, 12, 30, 45.5));");
    EXPECT_DOUBLE_EQ(evalScalar("Y"),  2026.0);
    EXPECT_DOUBLE_EQ(evalScalar("Mo"),    5.0);
    EXPECT_DOUBLE_EQ(evalScalar("D"),     9.0);
    EXPECT_DOUBLE_EQ(evalScalar("H"),    12.0);
    EXPECT_DOUBLE_EQ(evalScalar("MI"),   30.0);
    EXPECT_NEAR(evalScalar("S"), 45.5, 1e-6);
}

TEST_F(DatevecTest, MultiOutputVector)
{
    eval("[Y, Mo, D] = datevec([datenum(2026,5,9); datenum(2027,6,10)]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(Y)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("Y(1)"), 2026.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(2)"), 2027.0);
    EXPECT_DOUBLE_EQ(evalScalar("Mo(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("Mo(2)"), 6.0);
}

// datestr: format a serial date number / date vector as text. vs MATLAB
// R2025b. Implemented 2026-05-30 (was an undefined function). 738885.5 =
// 30-Dec-2022 12:00:00 (a Friday).
TEST_F(DatevecTest, DatestrDefaultAndTokens)
{
    // Default format: date-only when no time, date+time otherwise.
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885), '30-Dec-2022')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885.5), '30-Dec-2022 12:00:00')"), 1.0);
    // Format-string tokens.
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885.5,'yyyy-mm-dd'), '2022-12-30')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885.523,'yyyy/mm/dd HH:MM:SS'), '2022/12/30 12:33:07')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'mmmm dd, yyyy'), 'December 30, 2022')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'ddd'), 'Fri')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'dddd'), 'Friday')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885,'yy'), '22')"), 1.0);
}

TEST_F(DatevecTest, DatestrDatevecInput)
{
    // A 1x6 date vector is interpreted as [Y mo D H MI S].
    EXPECT_DOUBLE_EQ(
        evalScalar("strcmp(datestr([2022 12 30 6 5 9],'yyyy-mm-dd HH:MM:SS'), "
                   "'2022-12-30 06:05:09')"),
        1.0);
}

TEST_F(DatevecTest, DatestrAmPm)
{
    // An AM/PM meridiem token switches HH to a 12-hour, space-padded clock
    // and prints AM/PM by time of day (12 AM = midnight, 12 PM = noon).
    // Verified vs MATLAB R2025b. Exact-minute times avoid sub-second SS rounding.
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(0,    'HH:MM:SS AM'), '12:00:00 AM')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(0.25, 'HH:MM:SS AM'), ' 6:00:00 AM')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(0.5,  'HH:MM:SS AM'), '12:00:00 PM')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(0.7,  'HH:MM:SS AM'), ' 4:48:00 PM')"), 1.0);
    // The 'PM' token behaves identically to 'AM' (a placeholder).
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(0.7,  'HH:MM PM'), ' 4:48 PM')"), 1.0);
    // Full date + meridiem.
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(738885.5,'mm/dd/yyyy HH:MM PM'), '12/30/2022 12:00 PM')"), 1.0);
    // Without a meridiem token, HH stays 24-hour zero-padded.
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(datestr(0.7,'HH:MM'), '16:48')"), 1.0);
}

// datevec(str [, fmt]): parse a date string into [Y M D H MI S]. vs MATLAB
// R2025b. 2026-05-30: previously threw "string parsing not yet supported".
TEST_F(DatevecTest, StringParse)
{
    eval("v = datevec('2022-12-30 12:34:56');");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 2022.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),   12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),   30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"),   12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"),   34.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"),   56.0);
    // ISO date-only and dd-mmm-yyyy auto-detect.
    eval("v2 = datevec('30-Dec-2022');");
    EXPECT_DOUBLE_EQ(evalScalar("v2(2)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("v2(3)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v2(4)"),  0.0);
    // Explicit format string.
    eval("v3 = datevec('30/12/2022','dd/mm/yyyy');");
    EXPECT_DOUBLE_EQ(evalScalar("v3(1)"), 2022.0);
    EXPECT_DOUBLE_EQ(evalScalar("v3(3)"),   30.0);
    // Multi-output form.
    eval("[yy, mm, dd] = datevec('2022-12-30');");
    EXPECT_DOUBLE_EQ(evalScalar("yy"), 2022.0);
    EXPECT_DOUBLE_EQ(evalScalar("mm"),   12.0);
    EXPECT_DOUBLE_EQ(evalScalar("dd"),   30.0);
    // Unparseable string throws.
    EXPECT_THROW(eval("datevec('not a date')"), std::exception);
}

// calendar(year, month): 6x7 month matrix (cols Sun..Sat, zero-padded).
// vs MATLAB R2025b. Implemented 2026-05-30 (was an undefined function).
// Dec 2022 starts on a Thursday (column 5).
TEST_F(DatevecTest, Calendar)
{
    eval("C = calendar(2022, 12);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,1)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,2)")), 7);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,4)"), 0.0);   // Wed of week 1 is empty
    EXPECT_DOUBLE_EQ(evalScalar("C(1,5)"), 1.0);   // Thu = day 1
    EXPECT_DOUBLE_EQ(evalScalar("C(1,7)"), 3.0);   // Sat = day 3
    EXPECT_DOUBLE_EQ(evalScalar("C(5,7)"), 31.0);  // last day
    EXPECT_DOUBLE_EQ(evalScalar("C(6,1)"), 0.0);   // padded row
    EXPECT_DOUBLE_EQ(evalScalar("sum(C(:))"), 496.0); // 1+2+...+31
    // Leap February: 29 days, ends on a Thursday.
    eval("F = calendar(2024, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("F(5,5)"), 29.0);
    EXPECT_DOUBLE_EQ(evalScalar("F(5,6)"), 0.0);
    // Bad month throws.
    EXPECT_THROW(eval("calendar(2022, 13)"), std::exception);
}

// ── etime(t2, t1): elapsed seconds between date vectors ──────────────
// Implemented 2026-05-30 (was an undefined function). Calendar-aware,
// integer date-day part differenced separately from H/MI/S so a
// fractional second survives without cancellation. vs MATLAB R2025b.
TEST_F(DatevecTest, EtimeScalarPairs)
{
    // Fractional second: 0.5 exactly (not ~0.4999973 from cancellation).
    EXPECT_NEAR(evalScalar("etime([2026 5 30 12 0 30.5],[2026 5 30 12 0 30])"),
                0.5, 1e-9);
    // Calendar boundaries -> one day = 86400 s.
    EXPECT_DOUBLE_EQ(evalScalar("etime([2026 6 1 0 0 0],[2026 5 31 0 0 0])"),
                     86400.0);
    EXPECT_DOUBLE_EQ(evalScalar("etime([2027 1 1 0 0 0],[2026 12 31 0 0 0])"),
                     86400.0);
    // Leap-day cross 2024-02-29 -> 2024-03-01.
    EXPECT_DOUBLE_EQ(evalScalar("etime([2024 3 1 0 0 0],[2024 2 29 0 0 0])"),
                     86400.0);
    // Negative: t2 earlier than t1.
    EXPECT_DOUBLE_EQ(evalScalar("etime([2026 5 30 11 0 0],[2026 5 30 12 0 0])"),
                     -3600.0);
}

TEST_F(DatevecTest, EtimeMatrixAndBroadcast)
{
    // Each row a date vector -> N-by-1 column.
    eval("r = etime([2026 5 30 0 0 10; 2026 5 30 0 0 20],"
         "[2026 5 30 0 0 0; 2026 5 30 0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(r,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(r,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscolumn(r)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 20.0);
    // A single row in t1 broadcasts against N rows in t2.
    eval("b = etime([2026 5 30 0 0 1; 2026 5 30 0 0 2; 2026 5 30 0 0 3],"
         "[2026 5 30 0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("b(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(3)"), 3.0);
}

TEST_F(DatevecTest, EtimeWrongColumnsThrows)
{
    // MATLAB indexes column 6; fewer than 6 columns is an error.
    EXPECT_THROW(eval("etime([2026 5 30],[2026 5 29]);"), std::exception);
}

// ── weeknum(D [, WeekStart [, European]]): week-of-year number ────────
// Implemented 2026-05-30 (was an undefined function). US default counts
// the partial first week as week 1; European=1 applies the ISO-style
// >=4-day rule with the chosen WeekStart. vs MATLAB R2025b.
TEST_F(DatevecTest, WeeknumUS)
{
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-01-01'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-01-03'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-01-04'))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-12-31'))"), 53.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2020-02-29'))"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-07-04'))"), 27.0);
}

TEST_F(DatevecTest, WeeknumWeekStartMonday)
{
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-01-04'),2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-01-05'),2)"), 2.0);
}

TEST_F(DatevecTest, WeeknumEuropean)
{
    // Leading partial week donated to prior year.
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-01-01'),1,1)"), 53.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-01-04'),1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2026-12-31'),1,1)"), 52.0);
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2027-01-01'),1,1)"), 52.0);
    // 2025: leading week has 4 days, so European == US (both 53).
    EXPECT_DOUBLE_EQ(evalScalar("weeknum(datenum('2025-12-28'),1,1)"), 53.0);
}

TEST_F(DatevecTest, WeeknumVectorAndError)
{
    eval("w = weeknum([datenum('2026-01-01'); datenum('2026-12-31')]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(2)"), 53.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscolumn(w)"), 1.0);
    // WeekStart outside 1..7 is an error.
    EXPECT_THROW(eval("weeknum(datenum('2026-01-01'),9);"), std::exception);
}

// ── addtodate(D, quantity, units): add units to a serial date ─────────
// Implemented 2026-05-30 (was an undefined function). Time units are
// plain serial arithmetic; calendar units clamp the day and preserve the
// time-of-day. vs MATLAB R2025b.
TEST_F(DatevecTest, AddtodateTimeUnits)
{
    eval("b = datenum(2026,1,31,10,20,30);");
    // +3 day wraps Jan 31 -> Feb 3.
    eval("vd = datevec(addtodate(b,3,'day'));");
    EXPECT_DOUBLE_EQ(evalScalar("vd(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("vd(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("vd(4)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("vd_h = datevec(addtodate(b,3,'hour')); vd_h(4)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("vd_m = datevec(addtodate(b,3,'minute')); vd_m(5)"), 23.0);
    EXPECT_DOUBLE_EQ(evalScalar("vd_s = datevec(addtodate(b,3,'second')); vd_s(6)"), 33.0);
    // Subtracting two ~7.4e5 serials leaves the small delta with ~1e-11
    // absolute error (b's ulp at that magnitude), so use a 1e-9 tolerance.
    EXPECT_NEAR(evalScalar("addtodate(b,3,'millisecond') - b"), 3.0/86400000.0, 1e-9);
}

TEST_F(DatevecTest, AddtodateCalendarClamp)
{
    eval("b = datenum(2026,1,31,10,20,30);");
    // +3 month: Apr has 30 days, day clamps 31 -> 30; time preserved.
    eval("vmo = datevec(addtodate(b,3,'month'));");
    EXPECT_DOUBLE_EQ(evalScalar("vmo(2)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("vmo(3)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("vmo(4)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("vyr = datevec(addtodate(b,3,'year')); vyr(1)"), 2029.0);
    // Jan 31 + 1 month -> Feb 28 (non-leap) / Feb 29 (leap 2024).
    EXPECT_DOUBLE_EQ(evalScalar("v = datevec(addtodate(datenum(2026,1,31),1,'month')); v(3)"), 28.0);
    EXPECT_DOUBLE_EQ(evalScalar("v = datevec(addtodate(datenum(2024,1,31),1,'month')); v(3)"), 29.0);
    // Feb 29 2024 + 1 year -> Feb 28 2025.
    eval("vy = datevec(addtodate(datenum(2024,2,29),1,'year'));");
    EXPECT_DOUBLE_EQ(evalScalar("vy(1)"), 2025.0);
    EXPECT_DOUBLE_EQ(evalScalar("vy(3)"),   28.0);
    // Negative month crosses the year boundary backwards.
    eval("vn = datevec(addtodate(datenum(2026,3,15),-4,'month'));");
    EXPECT_DOUBLE_EQ(evalScalar("vn(1)"), 2025.0);
    EXPECT_DOUBLE_EQ(evalScalar("vn(2)"),   11.0);
    EXPECT_DOUBLE_EQ(evalScalar("vn(3)"),   15.0);
}

TEST_F(DatevecTest, AddtodateErrors)
{
    // Scalar date only; a vector is an error.
    EXPECT_THROW(eval("addtodate([datenum(2026,1,1); datenum(2026,6,1)],1,'day');"),
                 std::exception);
    // Unknown unit is an error.
    EXPECT_THROW(eval("addtodate(datenum(2026,1,1),1,'fortnight');"), std::exception);
}
