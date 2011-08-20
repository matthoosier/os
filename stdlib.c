#if defined(__ARM_EABI__) && (__ARM_EABI__ != 0)
    unsigned __aeabi_uidiv(unsigned numerator, unsigned denominator)
    {
        unsigned result = 0;

        while (numerator >= denominator) {
            numerator -= denominator;
            ++result;
        }

        return result;
    }
#endif
