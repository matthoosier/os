#ifndef __BITS_H__
#define __BITS_H__

/*
_type:      datatype of result
_position:  zero-indexed position of bit desired set
*/
#define setbit(_position) \
    (1 << (_position))

/*
_val:       value in which a bit is to be tested
_position:  zero-indexed position of bit desired set
*/
#define testbit(_val, _position) \
    ((setbit(_position) & _val) != 0)

#endif /* __BITS_H__ */
