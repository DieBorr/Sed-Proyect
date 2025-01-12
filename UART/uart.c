#include "uart.h"
#include <LPC17xx.h>

// Accepted Error baud rate value (in percent unit)
#define UART_ACCEPTED_BAUDRATE_ERROR    3

#define CHAR_8_BITS                     0x03
#define STOP_1_BIT                      0x00
#define STOP_2_BITS                     0x04
#define PARITY_ENABLE                   0x08
#define PARITY_EVEN                     0x10
#define PARITY_ODD                      0x00
#define BREAK_ENABLE                    0x40
#define DLAB_ENABLE                     0x80

#define THRE_STATUS                     0x20
#define TEMT_STATUS                     0x40

//#define STOP_1_BIT                      (0 << 2)
//#define PARITY_NONE                     (0 << 3)




#define FIFO_ENABLE                     (1 << 0)
#define RBR_IRQ_ENABLE                  (1 << 0)
#define THRE_IRQ_ENABLE                 (1 << 1)
#define UART_LSR_THRE   				        (1 << 5)
#define RDA_INTERRUPT                   (2 << 1)
#define CTI_INTERRUPT                   (6 << 1)

#define CTI_INTERRUPT                   (6 << 1)

#define RLS_INT_ID      0x3   // Receive Line Status
#define RDA_INT_ID      0x2   // Receive Data Available
#define CTI_INT_ID      0x6   // Character Time-out Indicator
#define THRE_INT_ID     0x1   // Transmitter Holding Register Empty

#define GET_INT_ID(x) (((x) >> 1) & 0x7)

static int uart0_set_baudrate(unsigned int baudrate) {
  int errorStatus = -1; // failure

  unsigned int uClk = SystemCoreClock/4;
  unsigned int calcBaudrate = 0;
  unsigned int temp = 0;

  unsigned int mulFracDiv, dividerAddFracDiv;
  unsigned int divider = 0;
  unsigned int mulFracDivOptimal = 1;
  unsigned int dividerAddOptimal = 0;
  unsigned int dividerOptimal = 0;

  unsigned int relativeError = 0;
  unsigned int relativeOptimalError = 100000;

  uClk = uClk >> 4; // div by 16

  //  The formula is :
  //  BaudRate= uClk * (mulFracDiv/(mulFracDiv+dividerAddFracDiv) / (16 * DLL)
  //
  //  The value of mulFracDiv and dividerAddFracDiv should comply to the following expressions:
  //  0 < mulFracDiv <= 15, 0 <= dividerAddFracDiv <= 15
  for (mulFracDiv = 1; mulFracDiv <= 15; mulFracDiv++) {
    for (dividerAddFracDiv = 0; dividerAddFracDiv <= 15; dividerAddFracDiv++) {
      temp = (mulFracDiv * uClk) / (mulFracDiv + dividerAddFracDiv);
      divider = temp / baudrate;
      if ((temp % baudrate) > (baudrate / 2))
        divider++;

      if (divider > 2 && divider < 65536) {
        calcBaudrate = temp / divider;
        if (calcBaudrate <= baudrate) {
          relativeError = baudrate - calcBaudrate;
        } else {
          relativeError = calcBaudrate - baudrate;
        }

        if (relativeError < relativeOptimalError) {
          mulFracDivOptimal = mulFracDiv;
          dividerAddOptimal = dividerAddFracDiv;
          dividerOptimal = divider;
          relativeOptimalError = relativeError;
          if (relativeError == 0)
            break;
        }
      }
    }

    if (relativeError == 0)
      break;
  }

  if (relativeOptimalError < ((baudrate * UART_ACCEPTED_BAUDRATE_ERROR) / 100)) {
    LPC_UART0->LCR |= DLAB_ENABLE; 	// importante poner a 1
    LPC_UART0->DLM = (unsigned char) ((dividerOptimal >> 8) & 0xFF);
    LPC_UART0->DLL = (unsigned char) dividerOptimal;
    LPC_UART0->LCR &= ~DLAB_ENABLE;	// importante poner a 0

    LPC_UART0->FDR = ((mulFracDivOptimal << 4) & 0xF0) | (dividerAddOptimal & 0x0F);
    errorStatus = 0; // success
  }

  return errorStatus;
}

//--------------------------------

#define FIFO_SIZE   512

typedef struct {
  int wr_idx;
  int rd_idx;
  int cnt;
//  int size;
  uint8_t data[FIFO_SIZE];
} fifo;

static int fifo_empty(fifo *f) {
  return (f->cnt == 0);
}

static int fifo_full(fifo *f) {
  return (f->cnt == FIFO_SIZE);
}

static void fifo_flush(fifo *f) {
  f->wr_idx = f->rd_idx = 0;
  f->cnt = 0;
}

void fifo_init(fifo *f) {
  fifo_flush(f);
}

int fifo_put_nb(fifo *f, uint8_t src) {
  if(!fifo_full(f)) {
    // critical section begin
    f->data[f->wr_idx] = src;
    f->wr_idx++;
    f->wr_idx &= (FIFO_SIZE-1);
    f->cnt++;
    // critical section end
    return 1;
  }
  return 0;
}

int fifo_get_nb(fifo *f, uint8_t *dst) {
  if(!fifo_empty(f)) {
    // critical section begin
    *dst = f->data[f->rd_idx] ;
    f->rd_idx++;
    f->rd_idx &= (FIFO_SIZE-1);
    f->cnt--;
    // critical section end
    return 1;
  }
  return 0;
}

static fifo rx_fifo;
static fifo tx_fifo;

static int uart_err_flag;
static int rx_err_flag;

//Transmit Holding Register (THR) and Transmit Shift Register (TSR)

int uart0_init(int baudrate) {
  int ret;
  
  fifo_init(&tx_fifo);
  fifo_init(&rx_fifo);
  
  LPC_PINCON->PINSEL0 |= (1 << 4) | (1 << 6); // Change P0.2 and P0.3 mode to TXD0 and RXD0

  LPC_UART0->LCR =  CHAR_8_BITS | STOP_1_BIT; // Set 8N1 mode (8 bits, no parity, 1 stop bit)
  ret = uart0_set_baudrate(baudrate);         // Set the baud rate

  LPC_UART0->IER = THRE_IRQ_ENABLE | RBR_IRQ_ENABLE;  // Enable UART TX and RX interrupt (for LPC17xx UART)
  NVIC_EnableIRQ(UART0_IRQn);                         // Enable the UART interrupt (for Cortex-CM3 NVIC)
  
  return ret;
}

static void send_byte() {
  uint8_t tx_byte;
  if( fifo_get_nb(&tx_fifo, &tx_byte) ) {
    LPC_UART0->THR = tx_byte;
  }
}

void UART0_IRQHandler(void) {
  int ret, int_id;
  
  int_id = GET_INT_ID(LPC_UART0->IIR);
  switch(int_id) {
    case RDA_INT_ID:  // Rx data ready
      ret = fifo_put_nb(&rx_fifo, LPC_UART0->RBR);
      if(!ret) {
        rx_err_flag = 1;
      }
      break;
    case THRE_INT_ID: // Tx data ready
      send_byte();
      break;
    default:
      uart_err_flag = 1;      
  }
}

#include <string.h>

// Non blocking function:

int uart0_recv(void *buf, size_t len) {
  uint8_t *dst = (uint8_t *) buf;
  int ret;
  
  while(len > 0) {
    ret = fifo_get_nb(&rx_fifo, dst);
    if(ret) {
      dst++;
      len--;
    }
    else {
      break;  
    }    
  }
    
  return (int) (dst - ((uint8_t *) buf));
}

// gets() reads a line from stdin into the buffer pointed to by s until 
// either a terminating newline or EOF, which it replaces with a null 
// byte ('\0'). 

// Note: Putty sends \r when enter key is pressed.

//#define END_OF_LINE '\n'
#define END_OF_LINE '\n'

char *uart0_gets(char *s) {
  uint8_t *dst = (uint8_t *) s;
  
  while(!fifo_get_nb(&rx_fifo, dst));
  while(*dst != END_OF_LINE) {
    dst++;
    while(!fifo_get_nb(&rx_fifo, dst));
  }
  *dst = 0;
  return s;
}


// uart0_getchar() reads a character from standard input. 
// It returns the character read. These functions wait for input and don't 
// return until input is available.


int uart0_getchar(void) {
  uint8_t c;
  while(!fifo_get_nb(&rx_fifo, &c));
  return ((int) c);
}

// uart0_kbhit() checks the console for keyboard input.
// uart0_kbhit returns a nonzero value if a key has been pressed. Otherwise, 
// it returns 0. If the function returns a nonzero value, a keystroke is 
// waiting in the buffer. The program can then call uart0_getchar() to get the 
// keystroke.

int uart0_kbhit(void) {
  return !fifo_empty(&rx_fifo);
}

//-------------------------------------------

// Coding note: If the uart tx module is idle (TEMT status bit active) when 
// uart0_send is called, perform a write access to THR register to trigger 
// the interrupt-driven communication

int uart0_send(const void *buf, size_t len) {
  uint8_t *src = (uint8_t *) buf;
  int tx_trigger = (LPC_UART0->LSR & TEMT_STATUS);
  int ret;
  
  while(len > 0) {
    ret = fifo_put_nb(&tx_fifo, *src);
    if(ret) {
      src++; len--;
    }
    else {
      break;  
    }    
  }
  
  ret = (int) (src - ((uint8_t *) buf));
  if (ret && tx_trigger) {
    send_byte();
  }
    
  return ret;
}

// fputs() writes the string s to stream, without its terminating null byte ('\0').

int uart0_fputs(const char *s) {
  const uint8_t *src = (const uint8_t *) s;
  int tx_trigger = (LPC_UART0->LSR & TEMT_STATUS);

  if (!*s)
    return 0;
    
  while(*src) {
    while(!fifo_put_nb(&tx_fifo, *src));
    src++;
  }
  
  if (tx_trigger) {
    send_byte();
  }
  return 0;
}

// puts() writes the string s and a trailing newline to stdout.

int uart0_puts(const char *s) {
  uart0_fputs(s);
  uart0_fputs("\n\r");
  return 0;
}
