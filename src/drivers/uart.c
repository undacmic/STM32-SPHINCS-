#include "common/defines.h"
#include "common/ring_buffer.h"
#include "drivers/uart.h"
#include "drivers/io.h"

#define UART_BAUD_RATE  (115200U)
#define UART_PRESCALER  (2U)
#define UART_PRESCLK    (CYCLES_16MHZ / UART_PRESCALER)    
#define UARTDIV         (UART_PRESCLK / UART_BAUD_RATE)
#define USART2_IRQn     (28)

#define BUFFER_SIZE     (32u)
RING_BUFFER(tx, BUFFER_SIZE, static);

static void USART_Configure(void) {
    USART2->PRESC &= ~USART2_PRESC_PRESCALER_MASK;
    USART2->PRESC |= USART2_PRESC_PRESCALER(1); 

    USART2->CR1 &= ~USART2_CR1_M0_MASK;
    USART2->CR1 &= ~USART2_CR1_M1_MASK;
    USART2->CR1 &= ~USART2_CR1_OVER8_MASK;

    USART2->BRR = UARTDIV;

    USART2->CR2 &= ~USART2_CR2_STOP_MASK;

    USART2->CR1 |= USART2_CR1_UE_MASK;

    USART2->CR1 |= USART2_CR1_TE_MASK;

    /**
     * Reset value of TC flag is 1, therefore activating interrupts on transmission complete 
     * will generate an interrupt at boot, even though I haven't sent a byte.
     * 
     * Need to deactivate the TC flag
     */
    USART2->ICR |= USART2_ICR_TCCF_MASK;

    USART2->CR1 |= USART2_CR1_TCIE_MASK;

    NVIC_EnableIRQ(USART2_IRQn);
}

void USART_Init(void) {
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    
    RCC->CCIPR &= ~RCC_CCIPR_USART2SEL_MASK;
    RCC->CCIPR |= RCC_CCIPR_USART2SEL(2);

    USART_Configure();
}

INTERRUPT_VECTOR USART2_LPUART2_IRQHandler() {

    if (! (USART2->ISR & USART2_ISR_TC_MASK)) {
        return;
    }

    USART2->ICR |= USART2_ICR_TCCF_MASK;

    if (!is_empty(&tx_ring_buffer)) {
        USART2->TDR = get(&tx_ring_buffer);
    }
}

// mpaland/printf needs this to be named _putchar
void _putchar (char c) {
    while(is_full(&tx_ring_buffer)) {}

    USART2->CR1 &= ~USART2_CR1_TCIE_MASK;
    
    bool is_transmitting = !is_empty(&tx_ring_buffer);
    put(&tx_ring_buffer, c);

    if (!is_transmitting) {
        USART2->TDR = get(&tx_ring_buffer);
    }

    USART2->CR1 |= USART2_CR1_TCIE_MASK;
}