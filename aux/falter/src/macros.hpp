#pragma once

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)
#define UNIQUE_ID(v) CONCAT(v, __COUNTER__)

#define _ARGS_MAP0(m)
#define _ARGS_MAP1(m, f, s) m(f, s)
#define _ARGS_MAP2(m, f, s, ...) m(f, s), _ARGS_MAP1(m, __VA_ARGS__)
#define _ARGS_MAP3(m, f, s, ...) m(f, s), _ARGS_MAP2(m, __VA_ARGS__)
#define _ARGS_MAP4(m, f, s, ...) m(f, s), _ARGS_MAP3(m, __VA_ARGS__)
#define _ARGS_MAP5(m, f, s, ...) m(f, s), _ARGS_MAP4(m, __VA_ARGS__)
#define _ARGS_MAP6(m, f, s, ...) m(f, s), _ARGS_MAP5(m, __VA_ARGS__)

#define _ARGS_MAP(n, ...) CONCAT(_ARGS_MAP, n)(__VA_ARGS__)

#define _NR_ARGS_IMPL(a, b, c, d, e, f, g, h, i, j, k, l, nr, ...) nr
#define _NR_ARGS(...)                                                          \
    _NR_ARGS_IMPL(__VA_ARGS__ __VA_OPT__(, ) 6, -1, 5, -1, 4, -1, 3, -1, 2,    \
                  -1, 1, -1, 0)

#define ARGS_MAP(m, ...)                                                       \
    _ARGS_MAP(_NR_ARGS(__VA_ARGS__), m __VA_OPT__(, ) __VA_ARGS__)

#define ARG_DECL(ty, name) ty name
#define ARG_NAME(ty, name) name
#define ARG_TYPE(ty, name) ty
