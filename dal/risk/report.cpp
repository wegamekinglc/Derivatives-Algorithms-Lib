#include <dal/platform/platform.hpp>
#include <dal/risk/report.hpp>
#include <dal/platform/strict.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/numerics.hpp>

/*IF--------------------------------------------------------------------------
storable Report
version 1
manual
&members
name is ?string
axis_names is string[]
sizes is integer[]
vals is number[]
header_labels is +string[]
header_values is +cell[][]
&conditions
axis_names.size() == sizes.size()
        Each axis must have a size
header_labels.size() == sizes.size()
        Each axis must have a header
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
#include <dal/auto/MG_Report_v1_Read.inc>
#include <dal/auto/MG_Report_v1_Write.inc>

        Storable_* Report_v1::Reader_::Build() const {
            const auto n = sizes_.size();
            Vector_<Report::Axis_> axes(n);
            for (auto ii = 0; ii < n; ++ii) {
                axes[ii].size_ = sizes_[ii];
                axes[ii].name_ = axis_names_[ii];
                axes[ii].labels_ = header_labels_[ii];
            }
            std::unique_ptr<Report_> retval(new Report_(name_, axes));

            // write in header values through the public interface
            for (auto jj = 0; jj < n; ++jj) {
                const auto& vals = header_values_[jj];
                Vector_<Cell_> temp(header_labels_[jj].size());
                for (int kr = 0; kr < vals.Rows(); ++kr) {
                    Copy(vals.Row(kr), &temp);
                    retval->AddHeaderRow(axis_names_[jj], kr, temp);
                }
            }
            // widen interface with this function that lets us set values
            retval->SetAll(vals_);
            return retval.release();
        }
    } // namespace

    Report_::Report_(const String_& name, const Vector_<Report::Axis_>& axes)
        : Storable_("Report", name), headers_(axes.size()) {
        REQUIRE(!axes.empty(), "Report has no axes");
        for (const auto& a : axes) {
            NOTICE(a.name_);
            const int which = static_cast<int>(axes_.size()); // number which preceded it
            REQUIRE(a.size_ > 0 || which == 0, "Zero size is not allowed");
            axes_[a.name_] = which;
            strides_ *= a.size_;
            strides_.push_back(1);
            headers_[which].labels_ = a.labels_;
            if (a.size_ > 0)
                headers_[which].values_.Resize(a.size_, a.labels_.size()); // grab the space now
        }
        vals_.resize(axes[0].size_ * strides_[0]);
    }

    void Report_::Write(Archive::Store_& dst) const {
        const int n = static_cast<int>(axes_.size());
        Vector_<int> sizes(n);
        Vector_<String_> names(n);
        for (const auto& [first, second] : axes_) {
            const int which = second;
            names[which] = first;
            sizes[which] = (which > 0 ? strides_[which - 1] : static_cast<int>(vals_.size())) / strides_[which];
        }
        static const auto Labels = [](const Report::Header_& h) { return h.labels_; };
        static const auto Values = [](const Report::Header_& h) { return h.values_; };
        Report_v1::XWrite(dst, name_, names, sizes, Vector_<>(vals_.begin(), vals_.end()), Apply(Labels, headers_),
                          Apply(Values, headers_));
    }

    int Report_::Size(const String_& axis) const {
        REQUIRE(axes_.count(axis), "No axis '" + axis + "'");
        const int which = axes_.find(axis)->second;
        return static_cast<int>(which > 0 ? strides_[which - 1] : vals_.size()) / strides_[which];
    }

    double& Report_::operator[](const Report::Address_& loc) {
        assert(*MinElement(loc.locs_) >= 0);
        const int which = InnerProduct(loc.locs_, strides_);
        if (which >= vals_.size()) {
            vals_.resize((loc.locs_[0] + 1) * strides_[0]); // make it big enough by extending the first axis only
            REQUIRE(which < vals_.size(), "Invalid index into report");
        }
        return vals_[which];
    }

    const double& Report_::operator[](const Report::Address_& loc) const {
        assert(*MinElement(loc.locs_) >= 0);
        const int which = InnerProduct(loc.locs_, strides_);
        REQUIRE(which < vals_.size(), "Invalid index into const report");
        return vals_[which];
    }

    void Report_::SetAll(const Vector_<>& vals) {
        REQUIRE(!strides_.empty(), "Can't set values -- they have no meaning");
        REQUIRE(!(vals.size() % strides_[0]), "Values size is not a multiple of first stride");
        REQUIRE(vals.size() >= vals_.size(), "Value setting loses information");
        vals_.assign(vals.begin(), vals.end());
    }

    namespace {
        int FindAxis(const std::map<String_, int>& axes, const String_& axis) {
            NOTICE(axis);
            const auto pw = axes.find(axis);
            REQUIRE(pw != axes.end(), "No such axis");
            return pw->second;
        }
    } // namespace

    int& Report::Address_::operator[](const String_& axis) { return locs_[FindAxis(axes_, axis)]; }

    Report::Address_ Report_::MakeAddress() const { return Report::Address_(axes_); }

    void Report_::AddHeaderRow(const String_& axes, int offset, const Vector_<Cell_>& values) {
        Report::Header_& header = headers_[FindAxis(axes_, axes)];
        const auto nLabels = header.labels_.size();
        NOTICE(static_cast<int>(nLabels));
        REQUIRE(values.size() == nLabels, "Wrong number (= " + ToString(static_cast<int>(values.size())) + " of labels");
        if (offset >= header.values_.Rows())
            header.values_.Resize(offset + 1, header.labels_.size());
        auto row = header.values_.Row(offset);
        Copy(values, &row);
    }

    const Report::Header_& Report_::Header(const String_& axis) const { return headers_[FindAxis(axes_, axis)]; }
}
