#pragma once

#include <source_location>
#include <string_view>

void WarnMst(std::string_view what,
             const std::source_location& loc = std::source_location::current());

[[noreturn]] void
Fail(std::string_view what,
     const std::source_location& loc = std::source_location::current());
