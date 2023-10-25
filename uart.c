#include "uart.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "circular_buffer.h"
#include "irq.h"
#include "util.h"

#define MMIO_BASE ((char*) 0xFE000000)

/*********** GPIO CONFIGURATION ********************************/

static char* const GPIO_BASE = (MMIO_BASE + 0x200000);
static const uint32_t GPFSEL_OFFSETS[6] = {0x00, 0x04, 0x08, 0x0c, 0x10, 0x14};
static const uint32_t GPIO_PUP_PDN_CNTRL_OFFSETS[4] = {0xe4, 0xe8, 0xec, 0xf0};

// GPIO Function Select
#define GPFSEL_REG(reg) (*(uint32_t*) (GPIO_BASE + GPFSEL_OFFSETS[reg]))
#define GPIO_PUP_PDN_CNTRL_REG(reg) (*(uint32_t*) (GPIO_BASE + GPIO_PUP_PDN_CNTRL_OFFSETS[reg]))

// function control settings for GPIO pins
static const uint32_t GPIO_INPUT = 0x00;
static const uint32_t GPIO_OUTPUT = 0x01;
static const uint32_t GPIO_ALTFN0 = 0x04;
static const uint32_t GPIO_ALTFN1 = 0x05;
static const uint32_t GPIO_ALTFN2 = 0x06;
static const uint32_t GPIO_ALTFN3 = 0x07;
static const uint32_t GPIO_ALTFN4 = 0x03;
static const uint32_t GPIO_ALTFN5 = 0x02;

// pup/pdn resistor settings for GPIO pins
static const uint32_t GPIO_NONE = 0x00;
static const uint32_t GPIO_PUP = 0x01;
static const uint32_t GPIO_PDP = 0x02;

const size_t UART_CONSOLE = 1;
const size_t UART_MARKLIN = 2;

// 4, 5, 6, 7
static void setup_gpio(uint32_t pin, uint32_t setting, uint32_t resistor) {
  // BCM2711 (pg 68)
  // 6 32-bit registers (first 2 bits of every register is unused)
  uint32_t reg = pin / 10;
  // 10 sets of 3 bits per GPIO pin per register.
  uint32_t shift = (pin % 10) * 3;
  uint32_t status = GPFSEL_REG(reg);  // read status
  // 7 = 0b111
  status &= ~(7u << shift);      // clear bits
  status |= (setting << shift);  // set bits
  GPFSEL_REG(reg) = status;

  reg = pin / 16;
  shift = (pin % 16) * 2;
  status = GPIO_PUP_PDN_CNTRL_REG(reg);  // read status
  status &= ~(3u << shift);              // clear bits
  status |= (resistor << shift);         // set bits
  GPIO_PUP_PDN_CNTRL_REG(reg) = status;  // write back
}

/*********** UART CONTROL ************************ ************/

#define UART0_BASE (MMIO_BASE + 0x201000)
#define UART3_BASE (MMIO_BASE + 0x201600)

// line_uarts[] maps the each serial line on the RPi hat to the UART that drives it
// currently:
//   * there is no line 0
//   * line 1 (console) is driven by RPi UART0
//   * line 2 (train control) is driven by RPi UART3
static char* const line_uarts[] = {NULL, UART0_BASE, UART3_BASE};

// UART register offsets
static const uint32_t UART_DR = 0x00;
static const uint32_t UART_FR = 0x18;
static const uint32_t UART_IBRD = 0x24;
static const uint32_t UART_FBRD = 0x28;
static const uint32_t UART_LCRH = 0x2c;
static const uint32_t UART_CR = 0x30;
static const uint32_t UART_IFLS = 0x34;
static const uint32_t UART_IMSC = 0x38;
static const uint32_t UART_MIS = 0x40;
static const uint32_t UART_ICR = 0x44;

#define UART_REG(line, offset) (*(volatile uint32_t*) (line_uarts[line] + offset))

// masks for specific fields in the UART registers
static const uint32_t UART_FR_RXFE = 0x10;
static const uint32_t UART_FR_TXFF = 0x20;
static const uint32_t UART_FR_RXFF = 0x40;
static const uint32_t UART_FR_TXFE = 0x80;

static const uint32_t UART_CR_UARTEN = 0x01;
static const uint32_t UART_CR_LBE = 0x80;
static const uint32_t UART_CR_TXE = 0x100;
static const uint32_t UART_CR_RXE = 0x200;
static const uint32_t UART_CR_RTS = 0x800;
static const uint32_t UART_CR_RTSEN = 0x4000;
static const uint32_t UART_CR_CTSEN = 0x8000;

static const uint32_t UART_LCRH_PEN = 0x2;
static const uint32_t UART_LCRH_EPS = 0x4;
static const uint32_t UART_LCRH_STP2 = 0x8;
static const uint32_t UART_LCRH_FEN = 0x10;
static const uint32_t UART_LCRH_WLEN_LOW = 0x20;
static const uint32_t UART_LCRH_WLEN_HIGH = 0x40;

static const uint32_t UART_CTS_MASK = 1 << 1;
static const uint32_t UART_RX_MASK = 1 << 4;
static const uint32_t UART_TX_MASK = 1 << 5;
static const uint32_t UART_RT_MASK = 1 << 6;

// line 1 and line 2
struct CircularBuffer write_buffer[2];
struct CircularBuffer read_buffer[2];

static const uint32_t* REG_PACTL_CS = (uint32_t*) 0xFE204E00;

// UART initialization, to be called before other UART functions
// Nothing to do for UART0, for which GPIO is configured during boot process
// For UART3 (line 2 on the RPi hat), we need to configure the GPIO to route
// the uart control and data signals to the GPIO pins expected by the hat
void uart_init() {
  setup_gpio(4, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(5, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(6, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(7, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(14, GPIO_ALTFN0, GPIO_NONE);
  setup_gpio(15, GPIO_ALTFN0, GPIO_NONE);

  // enable interrupts from UART in GIC
  irq_enable(IRQ_UART);
}

void uart_putc(size_t line, unsigned char ch) {
  while (uart_tx_fifo_full(line)) {}
  UART_REG(line, UART_DR) = ch;
}

void uart_puts(size_t line, const char* buf) {
  while (*buf) {
    uart_putc(line, *buf);
    buf++;
  }
}

void uart_enable_tx_irq(size_t line) {
  UART_REG(line, UART_IMSC) |= UART_TX_MASK;
}

void uart_disable_tx_irq(size_t line) {
  UART_REG(line, UART_IMSC) &= ~UART_TX_MASK;
}

static const uint32_t UARTCLK = 48000000;

// Configure the line properties (e.g, parity, baud rate) of a UART
// and ensure that it is enabled
void uart_config_and_enable(size_t line, uint32_t baudrate, bool two_stop_bits, bool fifo_buffer) {
  // to avoid floating point, this computes 64 times the required baud divisor
  uint32_t baud_divisor = (uint32_t) ((((uint64_t) UARTCLK) * 4) / baudrate);

  // line control registers should not be changed while the UART is enabled, so disable it
  uint32_t cr_state = UART_REG(line, UART_CR);
  UART_REG(line, UART_CR) = cr_state & ~UART_CR_UARTEN;

  uint32_t lcrh_state = UART_LCRH_WLEN_HIGH | UART_LCRH_WLEN_LOW;

  if (fifo_buffer) {
    lcrh_state |= UART_LCRH_FEN;
  }

  if (two_stop_bits) {
    lcrh_state |= UART_LCRH_STP2;
  }

  UART_REG(line, UART_LCRH) = lcrh_state;

  // set the baud rate
  UART_REG(line, UART_IBRD) = baud_divisor >> 6;
  UART_REG(line, UART_FBRD) = baud_divisor & 0x3f;
  // re-enable the UART
  // enable both transmit and receive regardless of previous state
  UART_REG(line, UART_CR) = cr_state | UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;

  // configure fifo interrupt trigger points
  // RX irq when fifo at least 1/8 full
  // TX irq when fifo less than full
  // UART_REG(line, UART_IFLS) |= 0x4;

  // enable interrupts (rx timeout, tx)
  // we disable tx interrupts initially since we do not have
  // data to send
  UART_REG(line, UART_IMSC) |= UART_RT_MASK | UART_RX_MASK | UART_CTS_MASK;
}

static size_t get_irq_line() {
  uint32_t pactl_cs = *REG_PACTL_CS;

  // check if uart 0 (line 1)
  if (pactl_cs & (1 << 20)) {
    return UART_CONSOLE;
  }

  // must be line 2
  return UART_MARKLIN;
}

bool uart_tx_fifo_full(size_t line) {
  return UART_REG(line, UART_FR) & UART_FR_TXFF;
}

bool uart_hasc(size_t line) {
  return !(UART_REG(line, UART_FR) & UART_FR_RXFE);
}

unsigned char uart_getc(size_t line) {
  return UART_REG(line, UART_DR);
}

enum Event uart_handle_irq() {
  size_t line = get_irq_line();
  uint32_t mis = UART_REG(line, UART_MIS);
  bool cts = false;
  bool tx = false;

  if (mis & UART_CTS_MASK) {
    // clear interrupt
    UART_REG(line, UART_ICR) = UART_CTS_MASK;
    cts = true;
  } else if (mis & UART_RT_MASK) {
    // clear interrupt
    UART_REG(line, UART_ICR) = UART_RT_MASK;
  } else if (mis & UART_TX_MASK) {
    // clear interrupt
    UART_REG(line, UART_ICR) = UART_TX_MASK;
    tx = true;
  } else if (mis & UART_RX_MASK) {
    // clear interrupt
    UART_REG(line, UART_ICR) = UART_RX_MASK;
  }

  if (cts) {
    if (line == UART_CONSOLE) {
      return EVENT_UART_CONSOLE_CTS;
    } else {
      return EVENT_UART_MARKLIN_CTS;
    }
  } else if (tx) {
    if (line == UART_CONSOLE) {
      return EVENT_UART_CONSOLE_TX;
    } else {
      return EVENT_UART_MARKLIN_TX;
    }
  } else {
    if (line == UART_CONSOLE) {
      return EVENT_UART_CONSOLE_RX;
    } else {
      return EVENT_UART_MARKLIN_RX;
    }
  }

  return EVENT_UNKNOWN;
}
