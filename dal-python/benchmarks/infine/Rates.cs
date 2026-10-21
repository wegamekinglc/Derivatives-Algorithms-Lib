using System.Text.Json;
using InFine.QuantFinance.Market;

// Suppressions below cover value-type parameters, which cannot be null.
namespace Dal.Infine.Benchmarks;

internal static class Rates
{
    private static readonly IDayCounter Dc = DayCounter.Actual365Fixed;
    private static readonly SwapConventions Annual = new(WeekendsOnlyCalendar.Instance, 0,
        BusinessDayConvention.Unadjusted, Frequency.Annual, Dc, Frequency.Annual, Dc);

    internal static Func<double[]> Prepare(JsonElement inputs, JsonElement fixture)
    {
        var workload = fixture.GetProperty("workload"); // nosemgrep: codacy.csharp.security.null-dereference
        string operation = workload.GetProperty("operation").GetString()!;
        var today = Program.Date(inputs.GetProperty("today")); // nosemgrep: codacy.csharp.security.null-dereference
        if (operation == "calibration")
        {
            return () => Calibration(today, fixture);
        }
        var dates = Program.Dates(inputs.GetProperty("dates")); // nosemgrep: codacy.csharp.security.null-dereference
        var dfs = Program.Numbers(inputs.GetProperty("dfs")); // nosemgrep: codacy.csharp.security.null-dereference
        IYieldCurve Curve(double shift = 0, int node = -1) => new InterpolatedDiscountCurve(today, Dc, dates,
            dates.Select((d, i) => dfs[i] * Math.Exp(-shift * (node < 0 || node == i ? 1 : 0) * Dc.YearFraction(today, d))).ToArray(),
            CurveInterpolation.LogLinearDiscount);
        var baseCurve = Curve();
        if (operation == "discount")
        {
            var queries = Program.Dates(fixture.GetProperty("queries")); // nosemgrep: codacy.csharp.security.null-dereference
            return () => queries.Select(d => baseCurve.DiscountFactor(d)).ToArray();
        }
        var terms = fixture.GetProperty("trades").EnumerateArray().ToArray(); // nosemgrep: codacy.csharp.security.null-dereference
        var instruments = terms.Select(Trade).ToArray();
        if (operation is "pv" or "prepared_pv")
        {
            return () => Prices(instruments, baseCurve);
        }
        if (operation == "cold_pv")
        {
            return () => terms.Select(t => Trade(t).Npv(baseCurve)).ToArray();
        }
        return PrepareRisk(inputs, operation, instruments, Curve, dates.Length); // nosemgrep: codacy.csharp.security.null-dereference
    }

    private static Func<double[]> PrepareRisk(JsonElement inputs, string operation, VanillaSwap[] instruments,
        Func<double, int, IYieldCurve> curve, int nodes)
    {
        if (instruments == null)
        {
            throw new ArgumentNullException(nameof(instruments));
        }
        if (operation is "dv01" or "market_update_pv")
        {
            double bump = inputs.GetProperty("bump").GetDouble(); // nosemgrep: codacy.csharp.security.null-dereference
            var up = curve(bump, -1);
            var down = curve(-bump, -1);
            return operation == "dv01" ? () => instruments.Select(s => (s.Npv(up) - s.Npv(down)) / 2).ToArray()
                : () => Prices(instruments, up).Concat(Prices(instruments, down)).ToArray();
        }
        if (operation == "node_dv01")
        {
            double step = inputs.GetProperty("node_bump").GetDouble(); // nosemgrep: codacy.csharp.security.null-dereference
            double scale = inputs.GetProperty("bump").GetDouble() / (2 * step); // nosemgrep: codacy.csharp.security.null-dereference
            var up = Enumerable.Range(0, nodes).Select(i => curve(step, i)).ToArray();
            var down = Enumerable.Range(0, nodes).Select(i => curve(-step, i)).ToArray();
            return () => instruments.SelectMany(s => Enumerable.Range(0, nodes)
                .Select(i => (s.Npv(up[i]) - s.Npv(down[i])) * scale)).ToArray();
        }
        throw new InvalidOperationException("Unmapped rate operation: " + operation);
    }

    private static VanillaSwap Trade(JsonElement term) => VanillaSwap.FromDates(
        Program.Date(term.GetProperty("start")), Program.Date(term.GetProperty("end")), Annual, // nosemgrep: codacy.csharp.security.null-dereference
        fixedRate: term.GetProperty("rate").GetDouble(), notional: term.GetProperty("notional").GetDouble(), // nosemgrep: codacy.csharp.security.null-dereference
        direction: term.GetProperty("sign").GetInt32() > 0 ? SwapDirection.Payer : SwapDirection.Receiver); // nosemgrep: codacy.csharp.security.null-dereference

    internal static double[] Prices(VanillaSwap[] swaps, IYieldCurve curve)
    {
        if (swaps == null)
        {
            throw new ArgumentNullException(nameof(swaps));
        }
        if (curve == null)
        {
            throw new ArgumentNullException(nameof(curve));
        }
        return swaps.Select(s => s.Npv(curve)).ToArray();
    }

    private sealed class ForecastInstrument : ICurveInstrument
    {
        private readonly VanillaSwap swap;
        private readonly IYieldCurve discount;

        internal ForecastInstrument(VanillaSwap swap, IYieldCurve discount)
        {
            this.swap = swap;
            this.discount = discount;
        }

        public DateOnly MaturityDate => swap.MaturityDate;
        public double ImpliedRate(IYieldCurve forecast) => swap.ParRate(discount, forecast);
    }

    private static double[] Calibration(DateOnly today, JsonElement fixture)
    {
        var workload = fixture.GetProperty("workload"); // nosemgrep: codacy.csharp.security.null-dereference
        int size = workload.GetProperty("size").GetInt32();
        var quotes = fixture.GetProperty("quotes"); // nosemgrep: codacy.csharp.security.null-dereference
        IYieldCurve Solve(string key, IYieldCurve? discount)
        {
            var builder = new PiecewiseYieldCurveBuilder(today, Dc)
                .WithInterpolation(CurveInterpolation.LogLinearDiscount).WithSolverTolerance(1e-12);
            for (int i = 1; i <= size; i++)
            {
                var swap = VanillaSwap.FromDates(today, today.AddYears(i), Annual); // nosemgrep: codacy.csharp.security.null-dereference
                ICurveInstrument instrument = discount is null ? swap : new ForecastInstrument(swap, discount);
                builder.Add(instrument, quotes.GetProperty(key)[i - 1].GetDouble());
            }
            return builder.Build();
        }
        var discount = Solve("usd_discount", null);
        var curves = new List<IYieldCurve> { discount };
        if (workload.GetProperty("kind").GetString() == "multi_staged")
        {
            curves.Add(Solve("usd_forward", discount));
        }
        var queries = Program.Dates(fixture.GetProperty("queries")); // nosemgrep: codacy.csharp.security.null-dereference
        return curves.SelectMany(curve => queries.Select(d => curve.DiscountFactor(d))).ToArray();
    }
}
