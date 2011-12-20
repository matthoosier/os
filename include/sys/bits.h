#ifndef __BITS_H__
#define __BITS_H__

#include <stdbool.h>

#include <sys/decls.h>

/*
_type:      datatype of result
_position:  zero-indexed position of bit desired set
*/
#define SETBIT(_position) \
    (1 << (_position))

/*
_val:       value in which a bit is to be tested
_position:  zero-indexed position of bit desired set
*/
#define TESTBIT(_val, _position) \
    ((SETBIT(_position) & _val) != 0)

/*
_count:     number of bits
*/
#define BITS_TO_BYTES(_count) ((_count) >> 3)

/*
_count:     number of bytes
*/
#define BYTES_TO_BITS(_count) ((_count) << 3)

BEGIN_DECLS

static inline void BitmapSet (uint8_t * bitmap_base, unsigned int index)
{
    bitmap_base[index >> 3] |= SETBIT(index & 0x7);
}

static inline void BitmapClear (uint8_t * bitmap_base, unsigned int index)
{
    bitmap_base[index >> 3] &= ~((uint8_t)SETBIT(index & 0x7));
}

static inline bool BitmapGet (uint8_t * bitmap_base, unsigned int index)
{
    return TESTBIT(bitmap_base[index >> 3], index & 0x7) != 0;
}

END_DECLS

#endif /* __BITS_H__ */
