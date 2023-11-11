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
static uint32_t receive_data_pointer = 0;
static uint32_t receive_data_pointer_bk = 0;
static uint8_t receive_data[1024 * 9] = {};    // real pritner has buffer length is 8KB
static struct PrintCFG print_config;

#pragma mark statemachine functions
void receive_data_reset(void) {
    receive_data_pointer = 0;
    memset(&print_config, 0, sizeof(print_config));
}

void start_packet(){
    receive_data_pointer_bk = receive_data_pointer;
}

void discard_packet(){
    if (receive_data_pointer > receive_data_pointer_bk)
        receive_data_pointer = receive_data_pointer_bk;
    else
        receive_data_pointer = 0;
}

int receive_data_write(uint8_t b) {
    if (receive_data_pointer < sizeof(receive_data)){
        receive_data[receive_data_pointer++] = b;
        return receive_data_pointer == sizeof(receive_data);
    }else{
        return -1;
    }
}

void receive_config_write(uint8_t b){
    uint32_t *cfg = (uint32_t*)&print_config;
    *cfg |= b;
    *cfg <<= 8;
}

void receive_data_start_printing(void) {
    print_image(receive_data,receive_data_pointer, &print_config);
}

#pragma mark link cable function
static bool link_cable_data_received = false;
void link_cable_ISR(void) {
    uint8_t reply = protocol_data_process(linkcable_receive());
    linkcable_send(reply);
    link_cable_data_received = true;
}

int64_t link_cable_watchdog(alarm_id_t id, void *user_data) {
    if (!link_cable_data_received) {
        linkcable_reset();
        protocol_reset();
    } else link_cable_data_received = false;        
    return MS(300);
}