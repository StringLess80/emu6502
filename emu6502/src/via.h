/*
 * via.h - The 6522 VIA (Versatile Interface Adapter) of the SBC.
 *
 * Sixteen registers, selected by address lines A3..A0. We emulate the
 * two 8-bit ports and the two timers. The shift register and the
 * handshake lines are stored but have no effect.
 */
#ifndef VIA_H
#define VIA_H

#include <stdbool.h>
#include <stdint.h>

enum {
    VIA_ORB  = 0x0,   /* output/input register B           */
    VIA_ORA  = 0x1,   /* output/input register A           */
    VIA_DDRB = 0x2,   /* data direction B (1 = output)     */
    VIA_DDRA = 0x3,   /* data direction A                  */
    VIA_T1CL = 0x4,   /* timer 1 counter low               */
    VIA_T1CH = 0x5,   /* timer 1 counter high (starts T1)  */
    VIA_T1LL = 0x6,   /* timer 1 latch low                 */
    VIA_T1LH = 0x7,   /* timer 1 latch high                */
    VIA_T2CL = 0x8,   /* timer 2 counter low               */
    VIA_T2CH = 0x9,   /* timer 2 counter high (starts T2)  */
    VIA_SR   = 0xA,   /* shift register                    */
    VIA_ACR  = 0xB,   /* auxiliary control                 */
    VIA_PCR  = 0xC,   /* peripheral control                */
    VIA_IFR  = 0xD,   /* interrupt flags                   */
    VIA_IER  = 0xE,   /* interrupt enable                  */
    VIA_ORA_NH = 0xF  /* register A without handshake      */
};

/* Interrupt flag bits we generate. */
enum {
    VIA_IRQ_T2 = 0x20,
    VIA_IRQ_T1 = 0x40
};

typedef struct {
    uint8_t  orb, ora;        /* what the CPU wrote to the ports     */
    uint8_t  ddrb, ddra;      /* which pins are outputs              */
    uint8_t  pins_b, pins_a;  /* what the outside world drives in    */

    uint16_t t1_counter, t1_latch;
    bool     t1_armed;        /* will set its flag at next underflow */
    uint16_t t2_counter;
    uint8_t  t2_latch_lo;
    bool     t2_armed;

    uint8_t  sr, acr, pcr, ifr, ier;
} Via;

void    via_reset(Via *via);
uint8_t via_read(Via *via, uint8_t reg);
uint8_t via_peek(const Via *via, uint8_t reg);
void    via_write(Via *via, uint8_t reg, uint8_t value);
void    via_tick(Via *via, int cycles);

/* The level on each pin of a port: outputs show what the CPU wrote,
 * inputs show what the outside world drives. */
uint8_t via_port_a(const Via *via);
uint8_t via_port_b(const Via *via);

#endif /* VIA_H */
