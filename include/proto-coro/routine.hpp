#pragma once

#include "ctx.hpp"

struct IRoutine {
    virtual void Step(IRuntime* ctx) = 0;
};
