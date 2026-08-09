#include "oge/assert.hpp"

namespace oge
{

static StackTraceFn g_stackTraceFn = nullptr;

void SetStackTraceFn(StackTraceFn fn)
{
    g_stackTraceFn = fn;
}

StackTraceFn GetStackTraceFn()
{
    return g_stackTraceFn;
}

}  // namespace oge
