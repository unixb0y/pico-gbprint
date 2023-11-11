#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <stdio.h>

#include "gb_printer.h"
#include "globals.h"

// #define DEBUG_TRAFFIC


// PI Pico printer
#define PRINTER_DEVICE_ID       0x81

#define PRN_MAGIC_1             0x88
#define PRN_MAGIC_2             0x33

#define PRN_STATUS_LOWBAT       0x80
#define PRN_STATUS_ER2          0x40
#define PRN_STATUS_ER1          0x20
#define PRN_STATUS_ER_PKT       0x10
#define PRN_STATUS_UNTRAN       0x08
#define PRN_STATUS_FULL         0x04
#define PRN_STATUS_BUSY         0x02
#define PRN_STATUS_SUM          0x01
#define PRN_STATUS_OK           0x00

#define TILE_SIZE               0x10
#define PRINTER_WIDTH           20
#define PRINTER_BUFFER_SIZE     (PRINTER_WIDTH * TILE_SIZE * 2)

// PXLR-Studio-next transfer
#define CAM_COMMAND_TRANSFER    0x10

enum printer_state {
    PRN_STATE_IDLE = 0,
    PRN_STATE_WAIT_MAGIC2,
    PRN_STATE_WAIT_COMMAND,
    PRN_STATE_WAIT_COMPRESSION,
    PRN_STATE_WAIT_DATA_LENGTH,
    PRN_STATE_WAIT_DATA_LENGTH2,
    PRN_STATE_RECV_DATA,
    PRN_STATE_WAIT_CHECKSUM1,
    PRN_STATE_WAIT_CHECKSUM2,
    /*
        Note:   there should be a  PRN_STATE_WAIT_ALIVE_CHECK,
                but since we need to prepare reply a cycle earlier, we have a PRN_STATE_STATUS_DONE state instead!
    */
    PRN_STATE_WAIT_STATUS,
    PRN_STATE_STATUS_DONE,
};

enum printer_command {
    PRN_COMMAND_UNDEFINED   = 0x00,
    PRN_COMMAND_INIT        = 0x01,
    PRN_COMMAND_PRINT       = 0x02,
    PRN_COMMAND_DATA        = 0x04,
    PRN_COMMAND_BREAK       = 0x08,
    PRN_COMMAND_STATUS      = 0x0F,
};

static volatile enum printer_state   printer_state = PRN_STATE_IDLE;
static volatile enum printer_command printer_cmd = PRN_COMMAND_UNDEFINED;
static volatile uint8_t  status = 0;
static volatile uint16_t remainingDataLen = 0;
static volatile uint16_t checksum = 0;


uint8_t chk1 = 0;
uint8_t chk2 = 0;

#pragma mark private functions
uint8_t protocol_reset(){
    receive_data_reset();
    printer_state = PRN_STATE_IDLE;
    printer_cmd = PRN_COMMAND_UNDEFINED;
    status |= PRN_STATUS_ER_PKT;
    remainingDataLen = 0;
    checksum = 0;
    return 0x00;
}

void did_finish_printing(){
    status &= ~PRN_STATUS_BUSY;
}

#pragma mark public functions
uint8_t protocol_data_process(uint8_t data_in) {
#ifdef DEBUG_TRAFFIC
    printf("%02x",data_in);
#endif
    switch (printer_state){
        case PRN_STATE_IDLE:
            if (data_in != PRN_MAGIC_1) return protocol_reset();
            else printer_state = PRN_STATE_WAIT_MAGIC2;
            return 0;
            break;

        case PRN_STATE_WAIT_MAGIC2:
            if (data_in != PRN_MAGIC_2) return protocol_reset();
            else printer_state = PRN_STATE_WAIT_COMMAND;
            return 0;
            break;

        case PRN_STATE_WAIT_COMMAND:
            switch (printer_cmd = data_in){
                case PRN_COMMAND_INIT:
                    receive_data_reset();
                    status = PRN_STATUS_OK;
                case PRN_COMMAND_DATA:                    
                    start_packet();
                    break;

                case PRN_COMMAND_STATUS:
                    //do nothing
                    break; 

                case PRN_COMMAND_PRINT:
                    //do nothing
                    break; 

                default:
                    printf("ERROR: Printer got unexpected command=%d\n",printer_cmd);
                    return protocol_reset();
                    break;
            }
            printer_state = PRN_STATE_WAIT_COMPRESSION;
            break;

        case PRN_STATE_WAIT_COMPRESSION:
            if (data_in){
                printf("ERROR: This implementation currently doesn't support compressed images!");
                return protocol_reset();
            }
            printer_state = PRN_STATE_WAIT_DATA_LENGTH;
            break;

        case PRN_STATE_WAIT_DATA_LENGTH:
            remainingDataLen = data_in;
            printer_state = PRN_STATE_WAIT_DATA_LENGTH2;
            break;

        case PRN_STATE_WAIT_DATA_LENGTH2:
            remainingDataLen |= ((uint16_t)data_in << 8);
            printer_state = PRN_STATE_WAIT_DATA_LENGTH2;
            if (remainingDataLen) printer_state = PRN_STATE_RECV_DATA;
            else printer_state = PRN_STATE_WAIT_CHECKSUM1;
            break;

        case PRN_STATE_RECV_DATA:
            switch (printer_cmd){
            case PRN_COMMAND_DATA:
                if (receive_data_write(data_in)) status |= PRN_STATUS_FULL;
                break;
            
            case PRN_COMMAND_PRINT:
                receive_config_write(data_in);
                break;

            default:
                break;
            }            
            if (--remainingDataLen == 0) printer_state = PRN_STATE_WAIT_CHECKSUM1;
            break;

        case PRN_STATE_WAIT_CHECKSUM1:
            printer_state = PRN_STATE_WAIT_CHECKSUM2;
            chk1 = data_in;
            checksum ^= data_in;
            return 0;
            break;

        case PRN_STATE_WAIT_CHECKSUM2:
            printer_state = PRN_STATE_WAIT_STATUS;
            chk2 = data_in;
            checksum ^= data_in << 8;
            return PRINTER_DEVICE_ID;
            break;

        case PRN_STATE_WAIT_STATUS:
            if (!checksum) status &= ~PRN_STATUS_SUM;
            else{
                status |= PRN_STATUS_SUM;
                discard_packet();
            }
            // if (status){
            //     printf("Replying with status=%d\n",status);
            // }
            if (printer_cmd == PRN_COMMAND_PRINT){
                status |= PRN_STATUS_BUSY;
                receive_data_start_printing();
            }
            printer_state = PRN_STATE_STATUS_DONE;
            return status;
            break;

        case PRN_STATE_STATUS_DONE:
            printer_state = PRN_STATE_IDLE;
#ifdef DEBUG_TRAFFIC
            printf(" -> %02x\n",status);
#endif
            break;

        default: //this shouldn't happen!
            printf("ERROR: Printer entered unexpected state=%d",printer_state);
            assert(0);
    }

    checksum += data_in;
    return 0;
}
