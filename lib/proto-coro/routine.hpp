#pragma once

template <class Runtime>
struct IRoutine : Runtime::RoutineAux {
    virtual void Step(Runtime* rt) = 0;
};
