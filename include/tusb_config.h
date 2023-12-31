#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
// #define CFG_TUSB_DEBUG 3
#define CFG_TUSB_MCU OPT_MCU_RP2040
#define CFG_TUSB_OS OPT_OS_PICO

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
  #error CFG_TUSB_MCU must be defined
#endif

#if CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
  #define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_HOST | OPT_MODE_HIGH_SPEED)
#else
  #define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                 OPT_OS_NONE
#endif

// CFG_TUSB_DEBUG is defined by compiler in DEBUG build
// #define CFG_TUSB_DEBUG           0

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

#ifndef CFG_TUH_API_EDPT_XFER
#define CFG_TUH_API_EDPT_XFER 1
#endif

#ifndef CFG_TUH_ENABLED
#define CFG_TUH_ENABLED 1
#endif

//--------------------------------------------------------------------
// CONFIGURATION
//--------------------------------------------------------------------

// #define CFG_TUH_VENDOR 2

// // Size of buffer to hold descriptors and other data used for enumeration
// #define CFG_TUH_ENUMERATION_BUFSIZE 256

// #define CFG_TUH_HUB                 0
// #define CFG_TUH_CDC                 0
// #define CFG_TUH_HID                 4 // typical keyboard + mouse device can have 3-4 HID interfaces
// #define CFG_TUH_MSC                 0
// #define CFG_TUH_VENDOR              0

// // max device support (excluding hub device)
// // 1 hub typically has 4 ports
// #define CFG_TUH_DEVICE_MAX          (CFG_TUH_HUB ? 4 : 1)

// //------------- HID -------------//

// #define CFG_TUH_HID_EP_BUFSIZE      64

#define CFG_TUH_ENDPOINT_MAX 3

#endif