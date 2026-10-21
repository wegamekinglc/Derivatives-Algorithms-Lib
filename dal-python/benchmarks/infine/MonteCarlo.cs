using System.Text.Json;
using InFine.QuantFinance.Models.Diffusion;
using InFine.QuantFinance.Payoffs.Equity;
using InFine.QuantFinance.Random.QuasiRandom;

// Suppressions below cover value-type parameters, which cannot be null.
namespace Dal.Infine.Benchmarks;

internal static class MonteCarlo
{
    internal static Func<double[]> Prepare(JsonElement market, JsonElement workload)
    {
        int paths = workload.GetProperty("size").GetInt32(), steps = workload.GetProperty("steps").GetInt32(); // nosemgrep: codacy.csharp.security.null-dereference
        bool barrier = workload.GetProperty("kind").GetString() == "barrier"; // nosemgrep: codacy.csharp.security.null-dereference
        bool greeks = workload.GetProperty("operation").GetString() == "mc_greeks"; // nosemgrep: codacy.csharp.security.null-dereference
        double spot = market.GetProperty("spot").GetDouble(), vol = market.GetProperty("vol").GetDouble(); // nosemgrep: codacy.csharp.security.null-dereference
        double rate = market.GetProperty("rate").GetDouble(), dividend = market.GetProperty("dividend").GetDouble(); // nosemgrep: codacy.csharp.security.null-dereference
        double strike = market.GetProperty("strike").GetDouble(), barrierLevel = market.GetProperty("barrier").GetDouble(); // nosemgrep: codacy.csharp.security.null-dereference
        double maturity = market.GetProperty("maturity").GetDouble(); // nosemgrep: codacy.csharp.security.null-dereference
        double[] bumps = Program.Numbers(market.GetProperty("bumps")); // nosemgrep: codacy.csharp.security.null-dereference

        double Price(double s, double v, double r)
        {
            var generator = new SobolGenerator(steps);
            var z = new double[steps];
            var path = new double[steps + 1];
            var logPath = new double[steps + 1];
            var dt = Enumerable.Repeat(maturity / steps, steps).ToArray();
            var rates = Enumerable.Repeat(r, steps).ToArray(); // nosemgrep: codacy.csharp.security.null-dereference
            var vols = Enumerable.Repeat(v, steps).ToArray(); // nosemgrep: codacy.csharp.security.null-dereference
            var dividends = Enumerable.Repeat(dividend, steps).ToArray();
            var dates = Enumerable.Range(0, steps + 1).Select(i => i * maturity / steps).ToArray();
            var discounts = dates.Select(t => Math.Exp(-r * t)).ToArray();
            var monitor = Enumerable.Range(1, steps).ToArray();
            double sum = 0;
            for (int p = 0; p < paths; p++)
            {
                generator.NextGaussians(z);
                double df = LogNormalAdjoint.Forward(s, rates, dividends, vols, dt, steps, z, 0, logPath, 0); // nosemgrep: codacy.csharp.security.null-dereference
                if (barrier)
                {
                    for (int t = 0; t <= steps; t++)
                    {
                        path[t] = Math.Exp(logPath[t]);
                    }
                    sum += BarrierMath.PathPremium(path, dates, discounts, monitor, steps, 1, true, false,
                        strike, barrierLevel, 0, v, BarrierMonitoring.Discrete, null);
                }
                else
                {
                    sum += BarrierMath.Premium(1, false, strike, 0, 1, 0, Math.Exp(logPath[steps]), df);
                }
            }
            return sum / paths;
        }

        double[] Run()
        {
            if (greeks && !barrier)
            {
                var generator = new SobolGenerator(steps);
                var result = AdjointMonteCarlo.LogNormal(AdjointPayoff.European(1, strike), spot,
                    new[] { rate }, new[] { dividend }, new[] { vol }, new[] { maturity }, paths,
                    (_, z) => generator.NextGaussians(z));
                return new[] { result.Price, result.Gradient[0], result.Gradient[1], result.Gradient[2] };
            }
            double price = Price(spot, vol, rate);
            if (!greeks)
            {
                return new[] { price };
            }
            return new[] { price, (Price(spot + bumps[0], vol, rate) - Price(spot - bumps[0], vol, rate)) / (2 * bumps[0]),
                (Price(spot, vol + bumps[1], rate) - Price(spot, vol - bumps[1], rate)) / (2 * bumps[1]),
                (Price(spot, vol, rate + bumps[2]) - Price(spot, vol, rate - bumps[2])) / (2 * bumps[2]) };
        }
        return Run;
    }
}
