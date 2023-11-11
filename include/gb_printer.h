#ifndef _GB_PRINTER_H_INCLUDE_
#define _GB_PRINTER_H_INCLUDE_

#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"

struct PrintCFG {
    /*
        Number of Sheets to print
        0 means line feed only
    */
    uint8_t sheets_num;


    /*
        -25% exposure       : 0x00
        default exposure    : 0x40
        +25% exposure       : 0x7f
    */
    uint8_t exposure;

    /*
        typically 0xE4 //todo: what does it mean??
    */
    uint8_t pallet;

    /*
        GB Camera sends postPrintMargins=3, prePrintMargins=1 by default
    */
    uint8_t postPrintMargins : 4;
    uint8_t prePrintMargins  : 4;
};

//state machine
void receive_config_write(uint8_t b);
void receive_data_reset(void);
int receive_data_write(uint8_t b);
void receive_data_start_printing(void);

void start_packet();
void discard_packet();

//external
void did_finish_printing();

//link cable
uint8_t protocol_reset();
void link_cable_ISR(void);
int64_t link_cable_watchdog(alarm_id_t id, void *user_data);


//implemented in main function
void print_image(uint8_t *data, size_t data_len, const struct PrintCFG *cfg);


#endif