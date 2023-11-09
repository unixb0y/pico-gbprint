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

// storage implementation
volatile uint32_t receive_data_pointer = 0;
uint8_t receive_data[BUFFER_SIZE_KB * 1024] = {};    // buffer length is 96K

uint8_t json_buffer[1024] = {0};                // buffer for rendering of status json
uint8_t buffer0[5200] = {0};
uint8_t buffer1[5200] = {0};
uint8_t buf0done = false;
uint8_t buf1done = false;
uint16_t buf0_len = 0;
uint16_t buf1_len = 0;

void receive_data_reset(void) {
    receive_data_pointer = 0;
}

void receive_data_write(uint8_t b) {
    if (receive_data_pointer < sizeof(receive_data))
         receive_data[receive_data_pointer++] = b;
}

void receive_data_commit(void) {
    printf("Got a chunk of data... %d\n", receive_data_pointer);
    if (!buf0done) {
        buf0_len = receive_data_pointer;
        printf("Data length: %d\n", buf0_len);
        for(uint16_t i=0; i<buf0_len; i++) {
            memcpy(&buffer0[i], &receive_data[i], 1);
        }
        buf0done = true;
    }
    else {
        buf1_len = receive_data_pointer;
        printf("Data length: %d\n", buf1_len);
        for(uint16_t i=0; i<buf1_len; i++) {
            memcpy(&buffer1[i], &receive_data[i], 1);
        }
        buf1done = true;
    }
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
