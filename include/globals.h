#ifndef _GLOBALS_H_INCLUDE_
#define _GLOBALS_H_INCLUDE_

#define ENABLE_DEBUG            false
#define BUFFER_SIZE_KB          176

// UART pins for thermal printer
#define UART_PRINT_TX 4
#define UART_PRINT_RX 5

// UART pins for debugging with
// picoprobe, such as printf output
#define UART_DEBUG_TX 12
#define UART_DEBUG_RX 13

// Macros for bit operations on arrays
#define SetBit(A, k)    (A[(k)/8] |= (1 << ((k)%8)))
#define SetBitBIG(A, k)   (A[(k)/8] |= (1 << (7-((k)%8))))
#define ClearBit(A, k) (A[(k)/8] &= ~(1 << ((k)%8)))
#define TestBit(A, k)  (A[(k)/8] & (1 << ((k)%8)))

// LED pin, undefine to disable
#define LED_PIN                 25
#ifdef LED_PIN
    #define LED_SET(A)          (gpio_put(LED_PIN, (A)))
    #define LED_ON              LED_SET(true)
    #define LED_OFF             LED_SET(false)
    #define LED_TOGGLE          (gpio_put(LED_PIN, !gpio_get(LED_PIN)))
#else
    #define LED_ON
    #define LED_OFF
    #define LED_TOGGLE
#endif

// "Tear" button pin, undefine to disable
#define PIN_KEY                 23

// time intervals
#define MKS(A)                  (A)
#define MS(A)                   ((A) * 1000)
#define SEC(A)                  ((A) * 1000 * 1000)

#endif