#ifndef __BITS_H__
#define __BITS_H__

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

#endif /* __BITS_H__ */
