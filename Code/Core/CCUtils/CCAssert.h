#ifndef CCASSERT_H_INCLUDED
#define CCASSERT_H_INCLUDED
#include <assert.h>

#ifdef CC_DEBUG
#define CC_ASSERT(A,B) assert(A)
#else
#define CC_ASSERT(A,B)
#endif

#endif // CCASSERT_H_INCLUDED
