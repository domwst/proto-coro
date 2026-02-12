#pragma once

template <class Runtime>
struct RoutineAux {};

template <class Runtime>
    requires requires { typename Runtime::RoutineAux; }
struct RoutineAux<Runtime> : Runtime::RoutineAux {};

template <class Runtime>
struct IRoutine : RoutineAux<Runtime> {
    virtual void Step(Runtime* rt) = 0;
};
