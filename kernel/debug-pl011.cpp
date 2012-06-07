#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sys/arch.h>
#include <sys/bits.h>
#include <sys/compiler.h>

#include <kernel/debug.hpp>
#include <kernel/mmu.hpp>

#define VERSATILE_UART0_BASE    0x101F1000
#define VERSATILE_UART0_IRQ     12
#define PL011_MMAP_SIZE         4096
#define KERNEL_UART0_ADDRESS    0xfffe0000

typedef struct
{
    uint32_t        DR;         /* Offset 0x000 */
    uint32_t        SR;         /* Offset 0x004 */
    uint32_t        reserved1;  /* Offset 0x008 */
    uint32_t        reserved2;  /* Offset 0x00c */
    uint32_t        reserved3;  /* Offset 0x010 */
    uint32_t        reserved4;  /* Offset 0x014 */
    uint32_t const  FR;         /* Offset 0x018 */
    uint32_t        reserved5;  /* Offset 0x01c */
    uint32_t        ILPR;       /* Offset 0x020 */
    uint32_t        IBRD;       /* Offset 0x024 */
    uint32_t        FBRD;       /* Offset 0x028 */
    uint32_t        LCR_H;      /* Offset 0x02C */
    uint32_t        CR;         /* Offset 0x030 */
    uint32_t        IFLS;       /* Offset 0x034 */
    uint32_t        IMSC;       /* Offset 0x038 */
    uint32_t const  RIS;        /* Offset 0x03C */
    uint32_t const  MIS;        /* Offset 0x040 */
    uint32_t        ICR;        /* Offset 0x044 */
    uint32_t        DMACR;      /* Offset 0x048 */
} pl011_t;

COMPILER_ASSERT(sizeof(pl011_t) == (0x048 + 4));

enum
{
    /**
     * \brief   Receive Fifo Empty
     *
     * Bit in FR register which, when set, says that there are not
     * bytes available to read.
     */
    FR_RXFE = SETBIT(4),

    /**
     * \brief   Transmit Fifo Full
     *
     * Bit in FR register which, when set, says that the output
     * pipeline is full.
     */
    FR_TXFF = SETBIT(5),
};

enum
{
    /**
     * \brief   Transmit enabled
     */
    CR_TXE = SETBIT(8),

    /**
     * \brief   Receive enabled
     */
    CR_RXE = SETBIT(9),

    /**
     * \brief   UART enabled
     */
    CR_UARTEN = SETBIT(0),
};

enum
{
    /**
     * \brief   Receive interrupt masked
     */
    IMSC_RX = SETBIT(4),

    /**
     * \brief   Transmit interrupt masked
     */
    IMSC_TX = SETBIT(5),
};

enum
{
    /**
     * \brief   Receive interrupt is high
     */
    MIS_RX = SETBIT(4),

    /**
     * \brief   Transmit interrupt is high
     */
    MIS_TX = SETBIT(5),
};

enum
{
    /**
     * \brief   Clear Receive interrupt
     */
    ICR_RX = SETBIT(4),

    /**
     * \brief   Clear Transmit interrupt
     */
    ICR_TX = SETBIT(5),

    /**
     * \brief   Clear all interrupts
     */
    ICR_ALL = 0x7ff,
};

static bool pl011_read_ready (pl011_t volatile * uart)
{
    return (uart->FR & FR_RXFE) == 0;
}

__attribute__((used))
static uint8_t pl011_blocking_read (pl011_t volatile * uart)
{
    while (!pl011_read_ready(uart));
    return uart->DR;
}

static bool pl011_write_ready (pl011_t volatile * uart)
{
    return (uart->FR & FR_TXFF) == 0;
}

static void pl011_blocking_write (pl011_t volatile * uart, uint8_t c)
{
    while (!pl011_write_ready(uart));
    uart->DR = c;
}

char my_toupper (char c)
{
    if (c >= 'a' && c <= 'z') {
        return c + ('A' - 'a');
    }
    else if (c >= 'A' && c <= 'Z') {
        return c;
    }
    else if (c == '\r' || c == '\n') {
        return c;
    }
    else {
        return '?';
    }
}

/**
 * \brief   Output printk() messages on a PL011 UART
 *
 * Concrete implementation of DebugDriver that uses an ARM PrimeCell
 * PL011 UART on the VersatilePB board to output printk() messages.
 *
 * Operates in non-blocking (busy-polling) mode to write out messages
 * without needing to sleep for interrupt acknowledgement.
 */
class Pl011DebugDriver : public DebugDriver
{
public:
    Pl011DebugDriver ();
    virtual ~Pl011DebugDriver ();

    virtual void Init ();
    virtual void PrintMessage (const char message[]);

private:
    pl011_t volatile * mUart0;
};

void Pl011DebugDriver::Init ()
{
    assert (PL011_MMAP_SIZE <= PAGE_SIZE);
    bool mapped = TranslationTable::GetKernel()->MapPage(KERNEL_UART0_ADDRESS, VERSATILE_UART0_BASE, PROT_KERNEL);
    assert(mapped);

    mUart0 = (pl011_t volatile *)KERNEL_UART0_ADDRESS;
}

void Pl011DebugDriver::PrintMessage (const char message[])
{
    const char * c = message;

    while (*c != '\0') {
        pl011_blocking_write(mUart0, *c);
        ++c;
    }
}

Pl011DebugDriver::Pl011DebugDriver ()
{
    Debug::RegisterDriver(this);
}

Pl011DebugDriver::~Pl011DebugDriver ()
{
    TranslationTable::GetKernel()->UnmapPage(KERNEL_UART0_ADDRESS);
}

/**
 * Singleton instance whose automatically executed constructor will
 * will cause this driver class to be registered.
 */
static Pl011DebugDriver pl011_debug_instance;
