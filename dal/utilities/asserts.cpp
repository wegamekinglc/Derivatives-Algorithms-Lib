//
// Created by Cheng Li on 2017/12/28.
//

#include <dal/utilities/asserts.hpp>
#include <stdexcept>

namespace {

#if defined(_MSC_VER) || defined(__BORLANDC__)
    // allow Visual Studio integration
    std::string format(const std::string& file, long line, const std::string& function, const std::string& message) {
        std::ostringstream msg;
        if (function != "(unknown)")
            msg << function << ": ";
        msg << "\n  " << file << "(" << line << "): \n";
        msg << message;
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
}

namespace dal {

    Error::Error(const std::string& file, long line, const std::string& function, const std::string& message) {
        message_ = std::make_shared<std::string>(format(file, line, function, message));
    }

    const char* Error::what() const throw() { return message_->c_str(); }

} // namespace dal