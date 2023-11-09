#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "hardware/pio.h"

#include "globals.h"
#include "linkcable.h"
#include "mew.h"

void link_cable_ISR(void);
int64_t link_cable_watchdog(alarm_id_t id, void *user_data);

void print(uint8_t *data) {
    uint16_t tilebuffer_size = sizeof(mew_tile_data);
    uint16_t bit_size = tilebuffer_size*4; // 8 bits per byte, 2 bits per pixel 
    uint16_t tile_count = tilebuffer_size/16;

    // All GameBoy Color prints are 160px wide
    uint16_t image_width = 160;
    // 192px height for typical pokedex entry
    uint16_t image_height = bit_size/image_width; 
    // number of tiles in horizontal direction
    uint16_t tile_width = image_width/8;
    uint16_t image_size = image_width*image_height;
    
    // pixel_bytes is just a binary bitmap of the 2BPP image
    uint16_t pixel_bytes_count = image_size/8;
    uint8_t *pixel_bytes = (uint8_t *)malloc(pixel_bytes_count * sizeof(uint8_t));
    memset(pixel_bytes, 0, pixel_bytes_count);

    // scaled_bytes stores the 4x integer-upscaled version of it
    uint16_t scaled_bytes_count = 4*pixel_bytes_count;
    uint8_t *scaled_bytes = (uint8_t *)malloc(scaled_bytes_count * sizeof(uint8_t));
    memset(scaled_bytes, 0, scaled_bytes_count);

    // slice_bytes stores the upscaled image in slices of 24x160px
    uint8_t *slice_bytes = (uint8_t *)malloc(scaled_bytes_count * sizeof(uint8_t));
    memset(slice_bytes, 0, scaled_bytes_count);

    for (uint16_t t=0; t<tile_count; t++) {
        uint8_t tile_x = t%tile_width;
        uint8_t tile_y = t/tile_width;

        for (uint8_t i=0; i<8; i++) {
            uint16_t byte0_idx = t*16 + i*2;
            uint16_t byte1_idx = t*16 + i*2+1;

            uint8_t byte0 = data[byte0_idx];
            uint8_t byte1 = data[byte1_idx];

            for (uint8_t j=0; j<8; j++) {
                uint16_t offset_printbit = tile_x*8 + i*2*image_width/2 + j + tile_y*tile_width*64;
                // 2BPP conversion: combine the 2 bytes bit-wise to get the pixel's shade from 0b00 to 0b11
                // We just check if it's > 0 since the printer only prints in black and white...
                if ((((byte0 >> (8-1-j)) & 1) << 1 | ((byte1 >> (8-1-j)) & 1)) > 0) {
                    // Some whacky calculations to translate the pixels by 90 degrees counter-clockwise
                    uint16_t rot_idx = bit_size - (((offset_printbit % image_width)+1)*image_height - offset_printbit/image_width);
                    SetBit(pixel_bytes, image_size-1-rot_idx);
                }
            }
        }
    }

    //printf("Bytes of rotated bit image:\n");
    //for(int i=0; i<image_size/8; i++) {
    //    printf("%02X", pixel_bytes[image_size/8-1-i]);
    //}
    //printf("\n-----\n");

    // --- BEGIN 4x integer scaling of the rotated image ---
    uint16_t w = image_height;
    uint16_t h = image_width;
    for (uint32_t nb=0; nb<(w*h*4); nb++) {
        uint32_t oldrow = nb/(w*2)/2;
        uint32_t oldcol = nb%(w*2)/2;
        uint32_t oldidx = oldrow*w+oldcol;
        if (TestBit(pixel_bytes, w*h-1-oldidx) > 0) {
            SetBit(scaled_bytes, w*h*4-1-nb);
        }
    }

    //printf("Bytes of scaled bit image:\n");
    //for(int i=0; i<4*image_size/8; i++) {
    //    printf("%02X", scaled_bytes[4*image_size/8-1-i]);
    //}
    //printf("\n-----\n");
    // --- END SCALING ---

    // --- BEGIN SLICING ---
    uint8_t slice_width = 24;
    w = image_height*2;
    h = image_width*2;
    uint32_t slice_num = w/slice_width;
    uint32_t pixel_size = w*h;
    uint32_t slice_size = slice_width*h;

    for (uint32_t byte_idx = 0; byte_idx < scaled_bytes_count; byte_idx++) {
        /* non-flipped version
        uint32_t offset_idx = (((byte_idx/3)%h)*(w/8) + byte_idx%3 + 3*(byte_idx/(3*h)));
        */
        uint32_t offset_idx = ((h-1-(byte_idx/3)%h)*(w/8) + byte_idx%3 + 3*(byte_idx/(3*h)));
        offset_idx = scaled_bytes_count-1-offset_idx;
        memcpy(&slice_bytes[byte_idx], &scaled_bytes[offset_idx], 1);
    }

    //printf("Bytes of sliced bit image (slice 1):\n");
    //for(int i=1*960; i<2*960; i++) {
    //    printf("%02X", slice_bytes[i]);
    //}
    //printf("\n-----\n");
    // --- END SLICING ---

    // --- BEGIN PRINTING ---
    // Header: 
    // ESC * 0x21 width (0x0140 = 320 pixels)
    //
    // Protocol:
    // Send 0x1b, 0x33, 0x10
    //
    // For each slice:
    // Send header + slice + 0x0a
    //
    // At the end:
    // Send 0x1b, 0x32
    uint8_t start[] = {0x1b, 0x33, 0x10};
    uint8_t header[] = {0x1b, 0x2a, 0x21, 0x40, 0x01};
    uint8_t nl[] = {0x0a};
    uint8_t end[] = {0x1b, 0x32};

    uart_write_blocking(uart1, start, sizeof(start));
    for (uint8_t i=0; i<slice_num; i++) {
        uart_write_blocking(uart1, header, sizeof(header));
        uart_write_blocking(uart1, &slice_bytes[i*(2*image_width)*3], (2*image_width)*3);
        uart_write_blocking(uart1, nl, 1);
    }
    uart_write_blocking(uart1, end, sizeof(end));
    uart_puts(uart1, "\n\n\n");
    // --- END PRINTING ---

    free(pixel_bytes);
    free(scaled_bytes);
    free(slice_bytes);
}

int main() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    stdio_init_all();
    stdio_uart_init_full(uart0, PICO_DEFAULT_UART_BAUD_RATE,
        UART_DEBUG_TX, UART_DEBUG_RX);

    // Link cable initialization
    sleep_ms(500);
    linkcable_init(link_cable_ISR);
    add_alarm_in_us(MS(300), link_cable_watchdog, NULL, true);
    sleep_ms(500);

    // Thermal printer setup
    uart_init(uart1, 9600);
    gpio_set_function(UART_PRINT_TX, GPIO_FUNC_UART);
    gpio_set_function(UART_PRINT_RX, GPIO_FUNC_UART);
    //uart_puts(uart1, "Hello World..\n\n\n");
    //print(mew_tile_data);

    extern uint8_t buffer0[];
    extern uint8_t buffer1[];

    while (true) {
        if (buffer1[0] != 0) {
            printf("Done printing...\n----------\n");

            // allocate 2BPP image buffer
            uint8_t databuffer[192*160*2/8];
            uint32_t data_counter = 0;
            uint32_t data_idx = 0;

            // protocol:
            // 0400
            // 8002 (0x0280 = 640 bytes)
            // packet of tile data (640 bytes)
            // repeat
            for (uint16_t i=0; i<5157; i++) {
                if (i+7 >= 5157) break; // end of whole dump
                if (buffer0[i] == 0x04 && buffer0[i+1] == 0x00) {
                    uint16_t packet_length = buffer0[i+3]<<8 | buffer0[i+2];
                    data_counter += packet_length;
                    i+=4; // skip packet start bytes
                    for (uint16_t j=0; j<packet_length; j++) {
                        memcpy(&databuffer[data_idx+j], &buffer0[i+j], 1);
                    }
                    data_idx += packet_length;
                }
            }

            for (uint16_t i=7; i<2588; i++) {
                if (i+7 >= 2588) break;
                if (buffer1[i] == 0x04 && buffer1[i+1] == 0x00) {
                    uint16_t packet_length = buffer1[i+3]<<8 | buffer1[i+2];
                    data_counter += packet_length;
                    i+=4; // skip chunk start bytes
                    for (uint16_t j=0; j<packet_length; j++) {
                        memcpy(&databuffer[data_idx+j], &buffer1[i+j], 1);
                    }
                    data_idx += packet_length;
                }
            }

            print((uint8_t *)databuffer);
            return 0;
        }
    }
    return 0;
}
