#pragma once

struct IRuntime;
struct IRoutine;

struct Context {
    IRoutine* self;
    IRuntime* rt;
};
