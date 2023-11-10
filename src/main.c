#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <host/hcd.h>
#include <tusb.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "hardware/pio.h"

#include "globals.h"
#include "linkcable.h"
#include "mew.h"
#include "gb_printer.h"

#define info(a ...) printf("[gbprinter] " a)
#define safeFree(ptr) ({if (ptr) {free(ptr); ptr=NULL;}})

void link_cable_ISR(void);
int64_t link_cable_watchdog(alarm_id_t id, void *user_data);
bool print_usb(void *buf, size_t bufLen);

tusb_desc_device_t desc_device = {};
int16_t gDaddr = -1;
uint16_t gPrinterWidth = 384;

#define LANGUAGE_ID 0x0409  // English

static uint8_t *gPrintData = NULL;
static size_t gPrintDataSize = 0;

void print_image(uint8_t *data, size_t data_len) {
    gPrintData = data;
    gPrintDataSize = data_len;
}

void do_print_image(uint8_t *data, size_t data_len) {
    bool disableUSB = false;
    uint16_t printerWidth = gPrinterWidth;

    if (printerWidth < 320){
        disableUSB = true;
        printf("ERROR: usb printer width smaller than hardcoded with");
        printerWidth = 320;
    }

    uint16_t tilebuffer_size = data_len;
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
    uint8_t *pixel_bytes_nrm = (uint8_t *)malloc(pixel_bytes_count * sizeof(uint8_t));
    uint8_t *pixel_bytes_rot = (uint8_t *)malloc(pixel_bytes_count * sizeof(uint8_t));
    memset(pixel_bytes_nrm, 0, pixel_bytes_count);
    memset(pixel_bytes_rot, 0, pixel_bytes_count);

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
                    uint16_t nrm_idx = offset_printbit;
                    SetBit(pixel_bytes_rot, image_size-1-rot_idx);
                    SetBit(pixel_bytes_nrm, image_size-1-nrm_idx);
                }
            }
        }
    }

    //printf("Bytes of rotated bit image:\n");
    //for(int i=0; i<image_size/8; i++) {
    //    printf("%02X", pixel_bytes[image_size/8-1-i]);
    //}
    //printf("\n-----\n");

    uint32_t scaled_bytes_count_nrm = 2*image_height*printerWidth/8;
    uint8_t *scaled_bytes_nrm = (uint8_t *)malloc(scaled_bytes_count_nrm * sizeof(uint8_t));
    memset(scaled_bytes_nrm, 0, scaled_bytes_count_nrm);


    // --- BEGIN 4x integer scaling of the normal image ---
    uint32_t halfPad = 0;//(printerWidth-image_width*2)/2;
    for (uint16_t h = 0; h < image_height; h++){
        uint16_t th = (image_height-h-1);
        for (uint16_t w = 0; w < image_width; w++){
            uint16_t tw = (image_width-w-1);
            if (TestBit(pixel_bytes_nrm,h*image_width + w)){
                SetBitBIG(scaled_bytes_nrm,(2*th)*printerWidth   + tw*2-0 + halfPad);
                SetBitBIG(scaled_bytes_nrm,(2*th)*printerWidth   + tw*2-1 + halfPad);
                SetBitBIG(scaled_bytes_nrm,(2*th-1)*printerWidth + tw*2-0 + halfPad);
                SetBitBIG(scaled_bytes_nrm,(2*th-1)*printerWidth + tw*2-1 + halfPad);
            }
        }
    }

    // printf("Bytes of scaled bit image:\n");
    // for(int i=0; i<scaled_bytes_count_nrm; i++) {
    //     uint8_t b = scaled_bytes_nrm[scaled_bytes_count_nrm-1-i];
    //     printf("%d%d%d%d%d%d%d%d",  (b >> 7) & 1,
    //                                 (b >> 6) & 1,
    //                                 (b >> 5) & 1,
    //                                 (b >> 4) & 1,
    //                                 (b >> 3) & 1,
    //                                 (b >> 2) & 1,
    //                                 (b >> 1) & 1,
    //                                 (b >> 0) & 1);
    // }
    // printf("\n-----\n");


    // --- BEGIN USB PRINTING ---
    if (!disableUSB){
        printf("printing on USB!\n");
        print_usb(scaled_bytes_nrm, scaled_bytes_count_nrm);
    }
    // --- END USB PRINTING
    safeFree(scaled_bytes_nrm);
    safeFree(pixel_bytes_nrm);

    // scaled_bytes stores the 4x integer-upscaled version of it
    uint16_t scaled_bytes_count_rot = 4*pixel_bytes_count;
    uint8_t *scaled_bytes_rot = (uint8_t *)malloc(scaled_bytes_count_rot * sizeof(uint8_t));
    memset(scaled_bytes_rot, 0, scaled_bytes_count_rot);

    // --- BEGIN 4x integer scaling of the rotated image ---
    uint16_t w = image_height;
    uint16_t h = image_width;
    for (uint32_t nb=0; nb<(w*h*4); nb++) {
        uint32_t oldrow = nb/(w*2)/2;
        uint32_t oldcol = nb%(w*2)/2;
        uint32_t oldidx = oldrow*w+oldcol;
        if (TestBit(pixel_bytes_rot, w*h-1-oldidx) > 0) {
            SetBit(scaled_bytes_rot, w*h*4-1-nb);
        }
    }

    safeFree(pixel_bytes_rot);


    // printf("Bytes of scaled bit image:\n");
    // for(int i=0; i<4*image_size/8; i++) {
    //    printf("%02X", scaled_bytes_nrm[4*image_size/8-1-i]);
    // }
    // printf("\n-----\n");
    // --- END SCALING ---



    // slice_bytes stores the upscaled image in slices of 24x160px
    uint8_t *slice_bytes = (uint8_t *)malloc(scaled_bytes_count_rot * sizeof(uint8_t));
    memset(slice_bytes, 0, scaled_bytes_count_rot);

    // --- BEGIN SLICING ---
    uint8_t slice_width = 24;
    w = image_height*2;
    h = image_width*2;
    uint32_t slice_num = w/slice_width;
    uint32_t pixel_size = w*h;
    uint32_t slice_size = slice_width*h;

    for (uint32_t byte_idx = 0; byte_idx < scaled_bytes_count_rot; byte_idx++) {
        /* non-flipped version
        uint32_t offset_idx = (((byte_idx/3)%h)*(w/8) + byte_idx%3 + 3*(byte_idx/(3*h)));
        */
        uint32_t offset_idx = ((h-1-(byte_idx/3)%h)*(w/8) + byte_idx%3 + 3*(byte_idx/(3*h)));
        offset_idx = scaled_bytes_count_rot-1-offset_idx;
        memcpy(&slice_bytes[byte_idx], &scaled_bytes_rot[offset_idx], 1);
    }

    safeFree(scaled_bytes_rot);

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

    printf("printing on UART!\n");
    uart_write_blocking(uart1, start, sizeof(start));
    for (uint8_t i=0; i<slice_num; i++) {
        uart_write_blocking(uart1, header, sizeof(header));
        uart_write_blocking(uart1, &slice_bytes[i*(2*image_width)*3], (2*image_width)*3);
        uart_write_blocking(uart1, nl, 1);
    }
    uart_write_blocking(uart1, end, sizeof(end));
    uart_puts(uart1, "\n\n\n");
    // --- END PRINTING ---

    safeFree(slice_bytes);
}

int main() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    stdio_init_all();
    stdio_uart_init_full(uart0, PICO_DEFAULT_UART_BAUD_RATE, UART_DEBUG_TX, UART_DEBUG_RX);

    tusb_init();

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

    info("--- gbprinter start ----\n");

    while (true) {
        if (gPrintData && gPrintDataSize){
            do_print_image(gPrintData, gPrintDataSize);
            gPrintData = NULL;
            gPrintDataSize = 0;
            did_finish_printing();
        }
        tuh_task();                            
    }

    return 0;
}

#pragma mark function definitions

bool printSynchronous(void *buf, size_t bufLen){
    if (gDaddr == -1){
        return false;
    }
    tuh_xfer_t xfer ={
        .daddr       = (uint8_t)gDaddr,
        .ep_addr     = 2,
        .buflen      = bufLen,
        .buffer      = buf,
        .complete_cb = NULL,
        .user_data   = 0
    };
    bool ret = tuh_edpt_xfer(&xfer);
    if (ret){
        while (usbh_edpt_busy(gDaddr, 2)){
            tuh_task();
        }
    }
    return ret;
}

bool print_usb(void *buf_, size_t bufLen){
    uint8_t *buf = (uint8_t*)buf_;
    if (gDaddr == -1){
        printf("ERROR: no printer connected!");
        return false;
    }
    uint32_t chunksize = 0x800;
    uint16_t img_width = 384; //Peripage A6 image width
    uint16_t img_height = bufLen*8/img_width;
    char cmd_header[] = 
    "\x10\xff\xfe\x01" //comand header
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" //header padding??

    "\x1d\x76\x30\x00\x30\x00" //command height
    "\x41\x41" //placeholder values
    "\x10\xff\xfe\x01" //command print
    ;

    *((uint16_t*)&cmd_header[sizeof(cmd_header)-1-4-2]) = img_height; //is this even needed?

    char cmd_footer[] = "\x1b\x4a\x40\x10\xff\xfe\x45";

    if (!printSynchronous(cmd_header,sizeof(cmd_header)-1)){ //send header
            printf("ERROR: printing header failed!\n");
            return false;
    }
    int i=0;
    while (bufLen > 0){
        size_t curChunk = (chunksize > bufLen) ? bufLen : chunksize;
        printf("printing chunk=%d\n",++i);
        if (!printSynchronous(buf,curChunk)){
            printf("ERROR: printing failed!\n");
            return false;
        }
        bufLen -= curChunk;
        buf += curChunk;
    }

    // {
    //     char empty_line[122];
    //     memset(empty_line,0,sizeof(empty_line));
    //     for (size_t i = 0; i < 35; i++){
    //         printSynchronous(empty_line,sizeof(empty_line));
    //     }
    // }

    if (!printSynchronous(cmd_footer,sizeof(cmd_footer)-1)){//send footer
            printf("ERROR: printing footer failed!\n");
            return false;
    }
    printf("print_usb done!\n");
    return true;
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t *buf, size_t len) {
  size_t total_bytes = 0;
  for (size_t i = 0; i < len; i++) {
    uint16_t chr = buf[i];
    if (chr < 0x80) {
      total_bytes += 1;
    } else if (chr < 0x800) {
      total_bytes += 2;
    } else {
      total_bytes += 3;
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
  return total_bytes;
}

static void _convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len, uint8_t *utf8, size_t utf8_len) {
  // TODO: Check for runover.
  (void)utf8_len;
  // Get the UTF-16 length out of the data itself.

  for (size_t i = 0; i < utf16_len; i++) {
    uint16_t chr = utf16[i];
    if (chr < 0x80) {
      *utf8++ = chr & 0xff;
    } else if (chr < 0x800) {
      *utf8++ = (uint8_t)(0xC0 | (chr >> 6 & 0x1F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (uint8_t)(0xE0 | (chr >> 12 & 0x0F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 6 & 0x3F));
      *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

static void print_utf16(uint16_t *temp_buf, size_t buf_len) {
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = _count_utf8_bytes(temp_buf + 1, utf16_len);

  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t *) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t*) temp_buf)[utf8_len] = '\0';

  printf("%s",temp_buf);
}

void getPrinterInfo(uint8_t daddr){
    char printerInfo[0x100] = {};

    tusb_control_request_t const request ={
        .bmRequestType_bit =
        {
            .recipient = TUSB_REQ_RCPT_INTERFACE,
            .type      = TUSB_REQ_TYPE_CLASS,
            .direction = TUSB_DIR_IN
        },
        .bRequest = 0,
        .wValue   = 0,
        .wIndex   = 0,
        .wLength  = sizeof(printerInfo)
    };

    tuh_xfer_t xfer ={
        .daddr       = daddr,
        .ep_addr     = 0,
        .setup       = &request,
        .buffer      = printerInfo,
        .complete_cb = NULL,
        .user_data   = 0
    };

    bool ret = tuh_control_xfer(&xfer);
    if (ret && xfer.actual_len){
        size_t len = xfer.actual_len - 1;
        info("Got printer info: '%.*s'\n",len,&printerInfo[1]);
    }

    ret = tuh_configuration_set(daddr, 1, NULL, 0);
    if (!ret){
        info("Failed to set config!\n");
        return;
    }
    tusb_desc_endpoint_t ep =
    {
        .bLength          = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType  = TUSB_DESC_ENDPOINT,
        .bEndpointAddress = 2,
        .bmAttributes     = { .xfer = TUSB_XFER_BULK },
        .wMaxPacketSize   = 0x40,
        .bInterval        = 0
    };
    ret = tuh_edpt_open(daddr,&ep);
    if (ret){
        info("Successfully openend endpoint!\n");
    }

    info("PRINTER IS READY NOW!\n");
    gDaddr = daddr;
}

void print_device_descriptor(tuh_xfer_t* xfer){
  if ( XFER_RESULT_SUCCESS != xfer->result ){
    printf("Failed to get device descriptor\n");
    return;
  }

  uint8_t const daddr = xfer->daddr;

  info("Device %u: ID %04x:%04x\n", daddr, desc_device.idVendor, desc_device.idProduct);
  info("Device Descriptor:\n");
  info("  bLength             %u\n"     , desc_device.bLength);
  info("  bDescriptorType     %u\n"     , desc_device.bDescriptorType);
  info("  bcdUSB              %04x\n"   , desc_device.bcdUSB);
  info("  bDeviceClass        %u\n"     , desc_device.bDeviceClass);
  info("  bDeviceSubClass     %u\n"     , desc_device.bDeviceSubClass);
  info("  bDeviceProtocol     %u\n"     , desc_device.bDeviceProtocol);
  info("  bMaxPacketSize0     %u\n"     , desc_device.bMaxPacketSize0);
  info("  idVendor            0x%04x\n" , desc_device.idVendor);
  info("  idProduct           0x%04x\n" , desc_device.idProduct);
  info("  bcdDevice           %04x\n"   , desc_device.bcdDevice);
    // Get String descriptor using Sync API
  uint16_t temp_buf[128];

  info("  iManufacturer       %u     "     , desc_device.iManufacturer);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf)) ){
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\n");

  info("  iProduct            %u     "     , desc_device.iProduct);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf))){
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\n");

  info("  iSerialNumber       %u     "     , desc_device.iSerialNumber);
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, temp_buf, sizeof(temp_buf))){
    print_utf16(temp_buf, TU_ARRAY_SIZE(temp_buf));
  }
  printf("\n");

  info("  bNumConfigurations  %u\n"     , desc_device.bNumConfigurations);

  getPrinterInfo(daddr);
}

//tuh USB lowlevel
void tuh_umount_cb(uint8_t daddr){
    info("USB detach!\n");    
    gDaddr = -1;
}

void tuh_mount_cb (uint8_t daddr){
    info("USB attach!\n"); 
    tuh_descriptor_get_device(daddr, &desc_device, sizeof(desc_device), print_device_descriptor, 0);
}