using System.Diagnostics;
using System.Text.Json;

namespace Dal.Infine.Benchmarks;

internal static class Program
{
    private static int Main(string[] args)
    {
        try
        {
            Run(args);
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }

    private static void Run(string[] args)
    {
        using var document = JsonDocument.Parse(File.ReadAllText(args[0]));
        var input = document.RootElement;
        var results = new List<object>();
        foreach (var fixture in input.GetProperty("cases").EnumerateArray())
        {
            var workload = fixture.GetProperty("workload");
            string name = workload.GetProperty("name").GetString()!;
            if (fixture.TryGetProperty("unsupported", out var reason))
            {
                results.Add(new { name, workload, status = "unsupported", reason = reason.GetString() });
                continue;
            }
            string operation = workload.GetProperty("operation").GetString()!;
            Func<double[]> run = operation.StartsWith("mc_", StringComparison.Ordinal)
                ? MonteCarlo.Prepare(input.GetProperty("equity"), workload)
                : operation.StartsWith("gsr_", StringComparison.Ordinal)
                    ? Gsr.Prepare(input.GetProperty("gsr"), workload)
                    : Rates.Prepare(input.GetProperty("rates"), fixture);
            for (int i = 0; i < 3; i++) Check(fixture, run());
            long start = Stopwatch.GetTimestamp();
            double[] values = run();
            long stop = Stopwatch.GetTimestamp();
            Check(fixture, values);
            long duration = checked((long)Math.Round((stop - start) * 1e9 / Stopwatch.Frequency));
            if (duration <= 0) throw new InvalidOperationException("Nonpositive native timing");
            results.Add(new { name, workload, status = "passed", values,
                samples_ns = new[] { duration }, method = fixture.GetProperty("method").GetString() });
        }
        File.WriteAllText(args[1], JsonSerializer.Serialize(new {
            schema = input.GetProperty("schema").GetString(), backend = "infine", results
        }));
    }

    private static void Check(JsonElement fixture, double[] values)
    {
        double[] reference = Numbers(fixture.GetProperty("reference"));
        double[] tolerance = Numbers(fixture.GetProperty("tolerance"));
        if (values.Length != reference.Length || values.Length != tolerance.Length)
            throw new InvalidOperationException("Comparison output/tolerance width mismatch");
        for (int i = 0; i < values.Length; i++)
            if (!double.IsFinite(values[i]) || Math.Abs(values[i] - reference[i]) >
                    Math.Max(tolerance[i], 1e-10 * Math.Max(Math.Abs(values[i]), Math.Abs(reference[i]))))
                throw new InvalidOperationException($"{fixture.GetProperty("workload").GetProperty("name")}[{i}]: {values[i]:R} != {reference[i]:R}");
    }

    internal static double[] Numbers(JsonElement values) => values.EnumerateArray().Select(x => x.GetDouble()).ToArray();
    internal static DateOnly Date(JsonElement value) => DateOnly.Parse(value.GetString()![..10], System.Globalization.CultureInfo.InvariantCulture);
    internal static DateOnly[] Dates(JsonElement values) => values.EnumerateArray().Select(Date).ToArray();
}
