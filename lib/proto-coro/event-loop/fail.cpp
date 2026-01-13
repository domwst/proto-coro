#include "fail.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

void WarnMsg(std::string_view what, const std::source_location& loc) {
    int err = errno;
    std::clog << loc.file_name() << ":" << loc.line() << " "
              << loc.function_name() << " Failed to " << what << ": "
              << std::strerror(err) << " (" << err << ")\n";
}

[[noreturn]] void Fail(std::string_view what, const std::source_location& loc) {
    WarnMsg(what, loc);
    std::clog.flush();
    std::abort();
}
