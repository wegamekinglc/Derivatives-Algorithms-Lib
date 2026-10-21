using System.Text.Json;
using InFine.QuantFinance.ClosedForm;
using InFine.QuantFinance.Market;
using InFine.QuantFinance.Models.Diffusion;
using InFine.QuantFinance.Random.QuasiRandom;

namespace Dal.Infine.Benchmarks;

internal static class Gsr
{
    internal static Func<double[]> Prepare(JsonElement market, JsonElement workload)
    {
        string operation = workload.GetProperty("operation").GetString()!;
        int size = workload.GetProperty("size").GetInt32();
        var today = Program.Date(market.GetProperty("today"));
        var expiryDate = Program.Date(market.GetProperty("expiry"));
        var maturityDate = Program.Date(market.GetProperty("maturity"));
        double rate = market.GetProperty("rate").GetDouble(), strike = market.GetProperty("strike").GetDouble();
        double vol = market.GetProperty("vol").GetDouble(), a = market.GetProperty("mean_reversion").GetDouble();
        if (a != 0) throw new InvalidOperationException("GSR comparison requires exactly zero mean reversion");
        if (operation is "gsr_swap_reused" or "gsr_swap_fresh")
        {
            var conventions = new SwapConventions(WeekendsOnlyCalendar.Instance, 0, BusinessDayConvention.Unadjusted,
                Frequency.Semiannual, DayCounter.Thirty360BondBasis, Frequency.Quarterly, DayCounter.Actual360);
            VanillaSwap Make() => VanillaSwap.FromDates(expiryDate, maturityDate, conventions, strike);
            var curve = new FlatForward(today, rate, DayCounter.Actual365Fixed);
            var swaps = Enumerable.Range(0, size).Select(_ => Make()).ToArray();
            return operation == "gsr_swap_reused" ? () => Rates.Prices(swaps, curve)
                : () => Enumerable.Range(0, size).Select(_ => Make().Npv(curve)).ToArray();
        }
        double Time(DateOnly date) => (date.DayNumber - today.DayNumber) / 365.0;
        double expiry = Time(expiryDate);
        var times = new[] { Time(Program.Date(market.GetProperty("first_coupon"))), Time(maturityDate) };
        var dfs = times.Select(t => Math.Exp(-rate * t)).ToArray();
        var accruals = new[] { .5, .5 };
        double dfExpiry = Math.Exp(-rate * expiry);
        HullWhiteStep.Coefficients(a, vol, expiry, out double decay, out double loading,
            out double l11, out double l21, out double l22);
        double integratedPhi = rate * expiry + vol * vol * expiry * expiry * expiry / 6;
        double[] Run()
        {
            if (operation == "gsr_swaption_gaussian1d")
                return new[] { HullWhiteKernels.Swaption(1, a, vol, expiry, dfExpiry, 2, times, dfs, accruals, strike) };
            if (operation != "gsr_swaption_mc") throw new InvalidOperationException("Unmapped GSR operation: " + operation);
            var generator = new SobolGenerator(2);
            var z = new double[2];
            double sum = 0;
            for (int p = 0; p < size; p++)
            {
                generator.NextGaussians(z);
                double state = HullWhiteStep.NextState(0, z[0], decay, l11);
                double integral = HullWhiteStep.IntegralIncrement(0, z[0], z[1], loading, l21, l22);
                double bond0 = Math.Exp(HullWhiteKernels.LogZeroBond(a, vol, Math.Log(dfExpiry), Math.Log(dfs[0]), expiry, times[0], state));
                double bond1 = Math.Exp(HullWhiteKernels.LogZeroBond(a, vol, Math.Log(dfExpiry), Math.Log(dfs[1]), expiry, times[1], state));
                sum += Math.Exp(-integratedPhi - integral) * Math.Max(1 - strike * .5 * bond0 - (1 + strike * .5) * bond1, 0);
            }
            return new[] { sum / size };
        }
        return Run;
    }
}
