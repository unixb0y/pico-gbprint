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
uint32_t last_word = 0;

void receive_data_reset(void) {
    receive_data_pointer = 0;
}

void receive_data_write(uint8_t b) {
    if (idle_len){
        if (receive_data_pointer < sizeof(receive_data))
            receive_data[receive_data_pointer++] = b;
        idle_len--;
    }else{
        if ((last_word>>16) == 0x0400){
            idle_len = last_word & 0xffff;
        }
    }
    last_word <<= 8;   
    last_word |= b;
}

void receive_data_commit(void) {
    printf("Got a chunk of data... %d\n", receive_data_pointer);
    print_image(receive_data,receive_data_pointer);
    receive_data_reset();
}

// link cable
bool link_cable_data_received = false;
void link_cable_ISR(void) {
    linkcable_send(protocol_data_process(linkcable_receive()));
    link_cable_data_received = true;
}

int64_t link_cable_watchdog(alarm_id_t id, void *user_data) {
    if (!link_cable_data_received) {
        linkcable_reset();
        protocol_reset();
    } else link_cable_data_received = false;
    return MS(300);
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
