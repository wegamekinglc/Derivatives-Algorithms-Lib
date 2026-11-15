#include <dal/platform/platform.hpp>

#include <chrono>
#include <iostream>
#include <limits>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/compoundedois.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/curvescript.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/buffercapacity.hpp>
#include <dal/math/matrix/linearsolveaccuracy.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    using Clock_ = std::chrono::steady_clock;
    struct Fixture_ {
        CurveInstrumentCalibrationSpec_ spec_;
        Vector_<Date_> dates_;
        Vector_<> theta_, background_;
        String_ role_, mode_;
    };
    bool Business(Date_ date) { return date == Date_(2024, 1, 13) || (!Date::IsWeekEnd(date) && date != Date_(2024, 1, 15)); }
    Date_ Step(Date_ date, int direction) {
        do {
            date = date.AddDays(direction);
        } while (!Business(date));
        return date;
    }
    Date_ Back(Date_ date, int count) {
        for (int i = 0; i < count; ++i)
            date = Step(date, -1);
        return date;
    }
    double LogDiscount(const Fixture_& f, Date_ date, const Vector_<>& theta) {
        const double days = date - f.dates_.front();
        const double split = f.spec_.geometry_.knotDates_[1] - f.dates_.front();
        const double last = f.spec_.geometry_.knotDates_[2] - f.dates_.front();
        return days <= split ? theta[0] * days / split : theta[0] + (theta[1] - theta[0]) * (days - split) / (last - split);
    }
    double Product(const Fixture_& f, size_t first, size_t last, const Vector_<>& projection) {
        double product = 1.;
        const int lookback = f.mode_ == "ObservationShift" ? 5 : 0;
        for (size_t i = first; i < last; ++i) {
            const auto sample = f.mode_ == "Lockout" && i >= last - 5 ? last - 6 : i;
            const auto start = Back(f.dates_[sample], lookback), end = Back(f.dates_[sample + 1], lookback);
            const double rate = start < f.dates_.front()
                                    ? .02
                                    : std::expm1(LogDiscount(f, start, projection) - LogDiscount(f, end, projection)) / ((end - start) / 360.);
            const double days = f.mode_ == "ObservationShift" ? end - start : f.dates_[i + 1] - f.dates_[i];
            product *= 1. + days / 360. * rate;
        }
        return product;
    }
    // Independent scalar calendar/product reference; fixture work stays outside timed actions.
    Vector_<> Oracle(const Fixture_& f, const Vector_<>& theta, bool shortMaturity, double coupon = .025) {
        const auto& projection = f.role_ == "Discount" ? f.background_ : theta;
        const auto& discount = f.role_ == "Overnight" ? f.background_ : theta;
        const size_t count = f.dates_.size() - 1, split = count / 2;
        double floating = 0., annuity = 0.;
        size_t first = 0;
        for (const auto last : {split, count}) {
            const double df = std::exp(LogDiscount(f, f.dates_[last].AddDays(2), discount));
            floating += (Product(f, first, last, projection) - 1.) * df;
            annuity += (f.dates_[last] - f.dates_[first]) / 365. * df;
            if (shortMaturity)
                break;
            first = last;
        }
        return {floating - coupon * annuity, floating / annuity};
    }
    CompoundedOisScriptSpec_ Contract(const Fixture_& f, bool shortMaturity) {
        CompoundedOisScriptSpec_ s;
        s.instrumentId_ = shortMaturity ? "calibration-short" : "calibration-long";
        s.direction_ = "PayFixed";
        s.notional_ = 1.;
        s.coupon_ = .025;
        s.currency_ = s.collateralCurrency_ = Ccy_("USD");
        s.collateralType_ = CollateralType_("OIS");
        s.discountRoute_ = "USD.OIS";
        s.projectionRoute_ = "USD.SOFR";
        s.fixingIndex_ = "USD-SOFR";
        s.observationBasis_ = DayBasis_("ACT_360");
        s.fixedBasis_ = DayBasis_("ACT_365F");
        s.calendar_ = HolidayCenterData_("explicit-calendar", {Date_(2024, 1, 15)}, {Date_(2024, 1, 13)});
        s.ruleId_ = "dal.compounded-ois-cashflows/2";
        s.observationConvention_ = CompoundedOisObservationConvention_{CompoundedOisObservationMode_(f.mode_), f.mode_ == "ObservationShift" ? 5 : 0,
                                                                       f.mode_ == "Lockout" ? 5 : 0};
        const size_t count = f.dates_.size() - 1, split = count / 2;
        size_t first = 0;
        for (const auto last : {split, count}) {
            const auto payment = f.dates_[last].AddDays(2);
            s.coupons_.push_back({first ? "second" : "first", f.dates_[first], f.dates_[last], payment});
            s.fixedLeg_.push_back({f.dates_[first], f.dates_[last], payment, (f.dates_[last] - f.dates_[first]) / 365.});
            if (shortMaturity)
                break;
            first = last;
        }
        return s;
    }
    void AddContracts(Fixture_* f) {
        auto& s = f->spec_;
        for (const bool shortMaturity : {true, false}) {
            const bool deposit = f->role_ == "Discount" && shortMaturity;
            const auto tables = PrepareCompoundedOis(Contract(*f, shortMaturity), s.valuation_);
            s.compoundedOis_.push_back(tables);
            const auto end = s.geometry_.knotDates_[1];
            const auto instrument = deposit ? NewDepositScriptInstrument(Deposit_(s.geometry_.today_, end, .02, DayBasis_("ACT_360")),
                                                                         {"anchor", Ccy_("USD"), "USD.OIS", 1.})
                                            : NewCompoundedOisScriptInstrument(tables);
            const double quote = deposit ? std::expm1(-f->theta_[0]) / ((end - s.geometry_.today_) / 360.) : Oracle(*f, f->theta_, shortMaturity)[1];
            const String_ id = shortMaturity ? "short" : "long";
            s.observations_.push_back(NewCalibrationObservation(id, instrument, "ParRate",
                                                                NewCalibrationMarketQuote(id, quote, CalibrationValueUnit_::Parse("Rate")), 1e-12));
        }
        auto pv = Contract(*f, false);
        pv.instrumentId_ = "registered-pv";
        pv.coupon_ = .031;
        s.compoundedOis_.push_back(PrepareCompoundedOis(pv, s.valuation_));
    }
    Fixture_ Fixture(size_t count, const String_& role, const String_& mode) {
        Fixture_ f;
        f.role_ = role;
        f.mode_ = mode;
        f.dates_.push_back(Date_(2024, 1, 2));
        while (f.dates_.size() <= count)
            f.dates_.push_back(Step(f.dates_.back(), 1));
        auto& s = f.spec_;
        auto& g = s.geometry_;
        g.today_ = f.dates_.front();
        g.ccy_ = "USD";
        g.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        g.knotDates_ = {g.today_, f.dates_[count / 2], f.dates_.back().AddDays(10)};
        g.calibrateDiscountCurve_ = role == "Discount";
        g.targetOvernightRoute_ = role == "Discount" ? "" : "USD.SOFR";
        s.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, role != "Overnight"},
                             {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        const double split = (g.knotDates_[1] - g.today_) / 365., last = (g.knotDates_[2] - g.today_) / 365.;
        f.theta_ = {-.03 * split, -.04 * last};
        f.background_ = {-.02 * split, -.025 * last};
        const Handle_<DiscountCurve_> passive(new DiscountLogDF_("passive", "USD", g.knotDates_, {0., f.background_[0], f.background_[1]},
                                                                 DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
        if (role == "Discount")
            s.market_.overnightCurves_["USD.SOFR"] = passive;
        if (role == "Overnight")
            s.market_.discountCurves_[CollateralType_("OIS")] = passive;
        s.valuation_.evaluationDate_ = g.today_;
        std::map<DateTime_, double> history;
        for (auto date = Back(g.today_, 5); date < g.today_; date = Step(date, 1))
            history.emplace(DateTime_(date), .02);
        s.valuation_.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", history}}));
        s.solver_.initialGuessPerNode_ = f.theta_ * .95;
        AddContracts(&f);
        return f;
    }
    CurveInstrumentCalibrationOptions_ Options() {
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_("AAD");
        return options;
    }
    template <class F_> void Timed(const std::string& name, const F_& action, int inner = 1) { Bench::Print(Bench::Run(name, action, 2, 10, inner)); }
    void Verify(const Fixture_& f, const CurveInstrumentCalibrationResult_& fit, const PreparedCurveInstruments_& prepared) {
        REQUIRE(fit.fitWithinTolerance_ && fit.evidence_, "evidenced EXACT OIS root");
        for (int i = 0; i < 2; ++i) {
            REQUIRE(std::abs(fit.parameters_[i] - f.theta_[i]) <= 1e-8, "independent calibration parameters");
            REQUIRE(std::abs(fit.rawResiduals_[i]) <= 1e-12, "raw Rate root residual");
        }
        const auto evaluated = prepared.Evaluate(fit.parameters_);
        SquareMatrix_<> matrix(2, 0.);
        Matrix_<> rhs(2, 2, 0.);
        for (int i = 0; i < 2; ++i) {
            rhs(i, i) = 1.;
            for (int j = 0; j < 2; ++j)
                matrix(i, j) = evaluated.quoteJacobian_(i, j);
        }
        const CheckedLinearSolve_ checked(matrix, rhs, {1e-12, 1e-12});
        REQUIRE(checked.Diagnostics().reciprocalConditionInfinity_ >= 1e-10, "full-rank square OIS root");
    }
    void Run(size_t count, const String_& role, const String_& mode) {
        const auto f = Fixture(count, role, mode);
        const auto options = Options();
        const auto start = Clock_::now();
        const auto fit = CalibrateCurveInstruments(f.spec_, options);
        const auto coldNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock_::now() - start).count();
        const PreparedCurveInstruments_ prepared(f.spec_, options);
        Verify(f, fit, prepared);
        const auto context = CurveInstrumentInternal::PrepareCurvePriceContext(prepared);
        const auto pv = NewCompoundedOisScriptInstrument(f.spec_.compoundedOis_.back());
        const auto price = CurveInstrumentInternal::PrepareCurvePrice(context, pv, "Price");
        const CandidateCurveProvider_<double> provider(price.geometry_, fit.parameters_);
        REQUIRE(std::abs(CurveScriptInternal::Measure(price.plan_, provider, f.spec_.geometry_.today_) -
                         Oracle(f, fit.parameters_, false, .031)[0]) <= 1e-10,
                "independent registered PV amount");
        BufferCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        {
            BufferCapacityScope_ scope(&budget);
            const PreparedCurveInstruments_ measured(f.spec_, options);
        }
        REQUIRE(budget.CapacityBytes() == 0, "prepared registry vector capacities released");
        double sink = 0.;
        const auto prefix = std::string("OISC ") + role.c_str() + " " + mode.c_str() + " D=" + std::to_string(count) + " ";
        Timed(prefix + "prepare/freeze registry", [&] { sink += PreparedCurveInstruments_(f.spec_, options).Plans().size(); });
        Timed(prefix + "EXACT AAD calibration", [&] { sink += CalibrateCurveInstruments(f.spec_, options).parameters_[0]; });
        Timed(prefix + "prepared native Jacobian", [&] { sink += prepared.Evaluate(fit.parameters_).quoteJacobian_(0, 0); }, 10);
        Timed(
            prefix + "registered PV reuse/value",
            [&] {
                const auto same = CurveInstrumentInternal::PrepareCurvePrice(context, pv, "Price");
                sink += CurveScriptInternal::Measure(same.plan_, provider, f.spec_.geometry_.today_);
            },
            10);
        const auto tape = AAD::MeasureTape(*AAD::Tape());
        REQUIRE(tape.nodes_ == 0 && tape.liveBytes_ == 0, "calibration AAD cleanup");
        Bench::DoNotOptimize(&sink);
        std::cout << "OIS_CALIBRATION role=" << role << " mode=" << mode << " D=" << count
                  << " native_parameters=2 quotes=2 registered_tables=3 cold_ns=" << coldNs << " evaluations=" << fit.evaluations_
                  << " gradients=" << fit.gradients_ << " vector_peak_bytes=" << budget.PeakCapacityBytes()
                  << " after_vector_bytes=" << budget.CapacityBytes() << " after_tape_nodes=" << tape.nodes_
                  << " after_tape_live_bytes=" << tape.liveBytes_ << " retained_tape_capacity=" << tape.capacityBytes_ << '\n';
    }
} // namespace

void RunCompoundedOisCalibration() {
    Bench::PrintHeader();
    for (const size_t count : {30u, 365u, 1825u})
        for (const auto* role : {"Overnight", "Discount", "Shared"})
            Run(count, role, "ObservationShift");
    Run(1825, "Shared", "Lockout");
    std::cout << "Compounded OIS calibration verified: three targets, D=30,365,1825; frozen registered PV\n";
}
