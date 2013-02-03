#ifndef __MATH_HPP__
#define __MATH_HPP__

#include <kernel/assert.h>

namespace Math
{
    template <typename T>
        inline T RoundDown (T base, size_t boundary)
        {
            assert(boundary != 0);

            #ifdef __GNUC__
                assert(__builtin_clz(boundary) + __builtin_ctz(boundary) + 1 == sizeof(boundary) * 8);
            #endif

            return base & ~(boundary - 1);
        }

    /**
     * Compute the closest value not lesser than base which is an
     * even multiple of boundary.
     *
     * @param boundary      must be an even power of two
     */
    template <typename T>
        inline T RoundUp (T base, size_t boundary)
        {
            assert(boundary != 0);

            #ifdef __GNUC__
                assert(__builtin_clz(boundary) + __builtin_ctz(boundary) + 1 == sizeof(boundary) * 8);
            #endif

            return (base + boundary - 1) & ~(boundary - 1);
        }
}

#endif
