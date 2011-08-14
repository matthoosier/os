/* Symbolic names for the processor mode register settings */

.equ usr, 0b10000   /* User             */
.equ fiq, 0b10001   /* Fast Interrupt   */
.equ irq, 0b10010   /* Interrupt        */
.equ svc, 0b10011   /* Supervisor       */
.equ abt, 0b10111   /* Abort            */
.equ und, 0b11011   /* Undefined        */
.equ sys, 0b11111   /* System           */
