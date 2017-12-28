//
// Created by Cheng Li on 2017/12/28.
//

#pragma once

#include <exception>
#include <memory>
#include <sstream>
#include <string>

namespace dal {

    class Error : public std::exception {
    public:
        /*
         * The explicit use of this constructor is not advised.
         * Use the DAL_ASSERT macro instead.
         */
        Error(const std::string& file, long line, const std::string& functionName, const std::string& message = "");
        /*
         * the automatically generated destructor would
         * not have the throw specifier.
         */
        ~Error() noexcept override = default;
        const char* what() const noexcept override;

    private:
        /*
         * can't use std::string directly
         * the copy constructor for std::string is not declared noexcept
         */
        std::shared_ptr<std::string> message_;
    };

} // namespace dal

#define DAL_ASSERT(condition, message)                                                                                 \
    if (!(condition)) {                                                                                                \
        std::ostringstream _ql_msg_stream;                                                                             \
        _ql_msg_stream << (message);                                                                                   \
        throw dal::Error(__FILE__, __LINE__, __func__, _ql_msg_stream.str());                                          \
    } else
