#ifndef __MINMAX_HPP__
#define __MINMAX_HPP__

template <typename T>
    inline T MIN (T a, T b)
    {
        return a < b ? a : b;
    }

template <typename T>
    inline T MAX (T a, T b)
    {
        return a > b ? a : b;
    }

#endif /* __MINMAX_HPP__ */
