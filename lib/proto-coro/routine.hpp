#pragma once

template <class Runtime>
struct IRoutine {
    virtual void Step(Runtime* rt) = 0;
};
