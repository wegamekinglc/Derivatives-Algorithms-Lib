//
// Created by wegamekinglc on 18-1-15.
//

#include <sstream>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>


namespace Dal {
    namespace exception {
        XStackInfo_::XStackInfo_(const char *name, const int &val)
                : name_(name), value_(&val), type_(Type_::INT) {}

        XStackInfo_::XStackInfo_(const char *name, const double &val)
                : name_(name), value_(&val), type_(Type_::DBL) {}

        XStackInfo_::XStackInfo_(const char *name, const String_ &val)
                : name_(name), value_(&val), type_(Type_::STR) {}

        XStackInfo_::XStackInfo_(const char *name, const char *val)
                : name_(name), value_(val), type_(Type_::CSTR)    // capture as char*, not char**
        {}

        XStackInfo_::XStackInfo_(const char *name, const Date_& val)
                : name_(name), value_(&val), type_(Type_::DATE) {}

        XStackInfo_::XStackInfo_(const char *name, const DateTime_ &val)
                : name_(name), value_(&val), type_(Type_::DATETIME) {}

        XStackInfo_::XStackInfo_(const char *msg)
                : name_(msg), value_(nullptr), type_(Type_::VOID) {}

        std::string XStackInfo_::Message() const {
            static const std::string EQUALS(" = ");
            switch (type_) {
                case Type_::INT:
                    return name_ + EQUALS + std::to_string(*(reinterpret_cast<const int *>(value_)));
                case Type_::DBL:
                    return name_ + EQUALS + std::to_string(*(reinterpret_cast<const double *>(value_)));
                case Type_::CSTR:
                    return name_ + EQUALS + std::string(reinterpret_cast<const char *>(value_));
                case Type_::STR:
                    return name_ + EQUALS + reinterpret_cast<const String_ *>(value_)->c_str();
                case Type_::DATE:
                    return name_ + EQUALS + Date::ToString(*reinterpret_cast<const Date_*>(value_)).c_str();
                case Type_::DATETIME:
                    return name_ + EQUALS + DateTime::ToString(*reinterpret_cast<const DateTime_*>(value_)).c_str();
                case Type_::VOID:
                    return std::string(name_);
                default:
                    return std::string();
            }
        }

        namespace {
            /* more appropriate implementation for production
            Vector_<XStackInfo_>& TheStack()
            {
                static boost::thread_specific_ptr<Vector_<XStackInfo_>> INSTANCE;
                if (!INSTANCE.get())	// get is thread-specific
                    INSTANCE.reset(new Vector_<XStackInfo_>);
                return *INSTANCE;	// so is operator*
            }
            */

            /* less-efficient implementation, used here to avoid boost link dependency */
            Vector_<XStackInfo_> *XTheStack(bool free_if_empty = false) {
                thread_local static Vector_<XStackInfo_> *INSTANCE = nullptr;
                if (!INSTANCE)
                    INSTANCE = new Vector_<XStackInfo_>;
                else if (free_if_empty && INSTANCE->empty()) {
                    delete INSTANCE;
                    return INSTANCE = nullptr;
                }
                return INSTANCE;
            }

            Vector_<XStackInfo_> &TheStack() { return *XTheStack(); }

        }    // leave local

        void PushStack(const XStackInfo_ &info) {
            TheStack().push_back(info);
        }

        void PopStack() {
            if (!TheStack().empty())
                TheStack().pop_back();

            // the following statement cleans up the stack pointer when it becomes empty
            // this prevents a memory leak (though not as reliably as a smart pointer implementation)
            // of course there is a runtime cost to the extra delete/new cycle
            // erase this line for production implementation using thread_specific_ptr
            XTheStack(true);
        }
    }

    namespace {

#if defined(_MSC_VER) || defined(__BORLANDC__)
        // allow Visual Studio integration
        std::string format(const std::string& file, long line, const std::string& function, const char* message) {
            std::ostringstream msg;
            if (function != "(unknown)")
                msg << function << ": ";
            msg << "\n  " << file << "(" << line << "): \n";
            msg << std::string(message);
            return msg.str();
        }
#else
        // use gcc format (e.g. for integration with Emacs)
    std::string format(const std::string& file, long line, const std::string& function, const std::string& message) {
        std::ostringstream msg;
        msg << "\n" << file << ":" << line << ": ";
        if (function != "(unknown)")
            msg << "In function `" << function << "': \n";
        msg << message;
        return msg.str();
    }
#endif

        std::string MsgWithStack(const std::string& file, long line, const std::string& functionName, const char *msg) {
            std::string ret_val = format(file, line, functionName, msg);
            for (const exception::XStackInfo_ &si : exception::TheStack())
                ret_val += "\n" + si.Message();
            return ret_val;
        }
    }

    Exception_::Exception_(const std::string& file, long line, const std::string& functionName, const char *msg)
            : std::runtime_error(MsgWithStack(file, line, functionName, msg)) {}
}
