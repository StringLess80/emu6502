/*
 * via.c - 6522 VIA emulation: ports and timers.
 */
#include "via.h"

void via_reset(Via *via)
{
    /* On reset the 6522 clears all its registers except the timers
     * and the shift register: every pin becomes an input. */
    via->orb = 0;
    via->ora = 0;
    via->ddrb = 0;
    via->ddra = 0;
    via->pins_a = 0xFF;   /* nothing connected: we read 1s */
    via->pins_b = 0xFF;
    via->t1_counter = 0xFFFF;
    via->t1_latch = 0xFFFF;
    via->t1_armed = false;
    via->t2_counter = 0xFFFF;
    via->t2_latch_lo = 0xFF;
    via->t2_armed = false;
    via->sr = 0;
    via->acr = 0;
    via->pcr = 0;
    via->ifr = 0;
    via->ier = 0;
}

/* Combine output bits (from OR) with input bits (from the pins). */
static uint8_t mix(uint8_t out, uint8_t ddr, uint8_t pins)
{
    return (uint8_t)((out & ddr) | (pins & (uint8_t)~ddr));
}

uint8_t via_port_a(const Via *via)
{
    return mix(via->ora, via->ddra, via->pins_a);
}

uint8_t via_port_b(const Via *via)
{
    return mix(via->orb, via->ddrb, via->pins_b);
}

uint8_t via_peek(const Via *via, uint8_t reg)
{
    switch (reg & 0x0F) {
    case VIA_ORB:    return via_port_b(via);
    case VIA_ORA:
    case VIA_ORA_NH: return via_port_a(via);
    case VIA_DDRB:   return via->ddrb;
    case VIA_DDRA:   return via->ddra;
    case VIA_T1CL:   return (uint8_t)(via->t1_counter & 0xFF);
    case VIA_T1CH:   return (uint8_t)(via->t1_counter >> 8);
    case VIA_T1LL:   return (uint8_t)(via->t1_latch & 0xFF);
    case VIA_T1LH:   return (uint8_t)(via->t1_latch >> 8);
    case VIA_T2CL:   return (uint8_t)(via->t2_counter & 0xFF);
    case VIA_T2CH:   return (uint8_t)(via->t2_counter >> 8);
    case VIA_SR:     return via->sr;
    case VIA_ACR:    return via->acr;
    case VIA_PCR:    return via->pcr;
    case VIA_IFR: {
        /* Bit 7 is set when any enabled flag is set. */
        uint8_t any = (via->ifr & via->ier & 0x7F) ? 0x80 : 0x00;
        return (uint8_t)(via->ifr | any);
    }
    default:         return (uint8_t)(via->ier | 0x80);   /* VIA_IER */
    }
}

uint8_t via_read(Via *via, uint8_t reg)
{
    uint8_t value = via_peek(via, reg);

    /* Some reads have side effects: they acknowledge a timer. */
    if ((reg & 0x0F) == VIA_T1CL) {
        via->ifr &= (uint8_t)~VIA_IRQ_T1;
    } else if ((reg & 0x0F) == VIA_T2CL) {
        via->ifr &= (uint8_t)~VIA_IRQ_T2;
    }
    return value;
}

void via_write(Via *via, uint8_t reg, uint8_t value)
{
    switch (reg & 0x0F) {
    case VIA_ORB:    via->orb = value;  break;
    case VIA_ORA:
    case VIA_ORA_NH: via->ora = value;  break;
    case VIA_DDRB:   via->ddrb = value; break;
    case VIA_DDRA:   via->ddra = value; break;

    case VIA_T1CL:                               /* writes the latch */
    case VIA_T1LL:
        via->t1_latch = (uint16_t)((via->t1_latch & 0xFF00) | value);
        break;
    case VIA_T1CH:                               /* load and start   */
        via->t1_latch = (uint16_t)((via->t1_latch & 0x00FF) | (value << 8));
        via->t1_counter = via->t1_latch;
        via->ifr &= (uint8_t)~VIA_IRQ_T1;
        via->t1_armed = true;
        break;
    case VIA_T1LH:
        via->t1_latch = (uint16_t)((via->t1_latch & 0x00FF) | (value << 8));
        via->ifr &= (uint8_t)~VIA_IRQ_T1;
        break;

    case VIA_T2CL:
        via->t2_latch_lo = value;
        break;
    case VIA_T2CH:
        via->t2_counter = (uint16_t)(via->t2_latch_lo | (value << 8));
        via->ifr &= (uint8_t)~VIA_IRQ_T2;
        via->t2_armed = true;
        break;

    case VIA_SR:  via->sr = value;  break;
    case VIA_ACR: via->acr = value; break;
    case VIA_PCR: via->pcr = value; break;

    case VIA_IFR:                       /* writing a 1 clears a flag */
        via->ifr &= (uint8_t)~(value & 0x7F);
        break;

    default:                            /* VIA_IER */
        if (value & 0x80) {
            via->ier |= (uint8_t)(value & 0x7F);     /* set bits   */
        } else {
            via->ier &= (uint8_t)~(value & 0x7F);    /* clear bits */
        }
        break;
    }
}

void via_tick(Via *via, int cycles)
{
    /* Timer 1 */
    if (cycles > via->t1_counter) {
        /* It reached zero during these cycles. */
        if (via->t1_armed) {
            via->ifr |= VIA_IRQ_T1;
        }
        if (via->acr & 0x40) {
            /* Free-run mode: reload from the latch and keep going.
             * (The real chip takes latch + 2 cycles per period.) */
            int left = cycles - via->t1_counter - 1;
            via->t1_counter = via->t1_latch;
            if (left > 0 && left <= via->t1_counter) {
                via->t1_counter = (uint16_t)(via->t1_counter - left);
            }
        } else {
            /* One-shot mode: flag only once, the counter keeps going. */
            via->t1_armed = false;
            via->t1_counter = (uint16_t)(via->t1_counter - cycles);
        }
    } else {
        via->t1_counter = (uint16_t)(via->t1_counter - cycles);
    }

    /* Timer 2 (one-shot only; pulse counting mode is not emulated) */
    if (!(via->acr & 0x20)) {
        if (cycles > via->t2_counter && via->t2_armed) {
            via->ifr |= VIA_IRQ_T2;
            via->t2_armed = false;
        }
        via->t2_counter = (uint16_t)(via->t2_counter - cycles);
    }
}
