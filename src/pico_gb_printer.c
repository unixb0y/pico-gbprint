#include "pico/stdlib.h"
#include "time.h"
#include "pico/bootrom.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/pio.h"
#include <stdio.h>
#include <string.h>

#include "gb_printer.h"
#include "linkcable.h"
#include "globals.h"

//bool debug_enable = ENABLE_DEBUG;
bool speed_240_MHz = false;
void print_image(uint8_t *data, size_t data_len);

// storage implementation
volatile uint32_t receive_data_pointer = 0;
uint8_t receive_data[BUFFER_SIZE_KB * 1024] = {};    // buffer length is 96K
uint16_t idle_len = 0;
uint32_t cur_word = 0;
bool isPrinting = false;

void receive_data_reset(void) {
    receive_data_pointer = 0;
    idle_len = 0;
    cur_word = 0;
}

void receive_data_write(uint8_t b) {
    if (idle_len){
        if (receive_data_pointer < sizeof(receive_data)) {
            receive_data[receive_data_pointer++] = b;
            idle_len--;
        }
    }
    else{
        cur_word <<= 8;   
        cur_word |= b;
        if ((cur_word) == 0x04008002){
            idle_len = (cur_word & 0xff)<<8 | cur_word>>8;
        }
    }
}

void receive_data_commit(void) {
    printf("Got a chunk of data... %d\n", receive_data_pointer);

    isPrinting = true;
    print_image(receive_data,receive_data_pointer);
}

// link cable
bool link_cable_data_received = false;
void link_cable_ISR(void) {
    linkcable_send(protocol_data_process(linkcable_receive()));
    link_cable_data_received = true;
}

int64_t link_cable_watchdog(alarm_id_t id, void *user_data) {
    if (!isPrinting){
        //do not reset cable while we are printing
        if (!link_cable_data_received) {
            linkcable_reset();
            protocol_reset();
        } else link_cable_data_received = false;        
    }
    return MS(300);
}

void did_finish_printing(){
    isPrinting = false;
    receive_data_reset();
}

// key button
#ifdef PIN_KEY
static void key_callback(uint gpio, uint32_t events) {
    linkcable_reset();
    protocol_reset();
    receive_data_reset();
    LED_OFF;
}
#endif
