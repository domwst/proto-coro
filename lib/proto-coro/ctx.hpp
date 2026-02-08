#pragma once

template <class Self, class Runtime>
struct Context {
    Self* self;
    Runtime* rt;
};
