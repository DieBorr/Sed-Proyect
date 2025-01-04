#include <LPC17xx.h>
#include "../UART/uart.h"
#include "../GLCD/GLCD.h"

// Board features:
#define F_CPU     (SystemCoreClock)   // From system_LPC17xx.h
#define F_PCLK    (F_CPU/4)
//#define T_SYSTICK (1.0e-3)

// Forward function declaration:
void pins_config(void);

#define LED_1 (1 << 25)// Led 1 is P3_25
#define LED_2 (1 << 26)// Led 2 is P3_26

int delay_tick;


#include <string.h>
#include <stdio.h>

char tx_msg[128];
char rx_msg[128];
static int test_uart_token;


#define MENU_DICT_LEN (sizeof(menu_dict)/sizeof(dict_entry))

int main() {
  int ret;
  int len, msg_cnt = 0;
  char *msg;
  int rem;
  int key, opt, i; 
  
  
  // Serial port initialization:
  ret = uart0_init(9600);
  if(ret < 0) {
    return -1;
  }
  LCD_Initializtion();
  // Using blocking API:
  uart0_fputs("Esto es una prueba de la UART0 del LPC1768 a 9600 baudios\r");

  
  uart0_gets(rx_msg);
  uart0_fputs("El mensaje enviado ha sido: ");
  uart0_fputs(rx_msg);
  GUI_Text(0,0,rx_msg,Blue,Black);
  
  while(1);
  return 0;
}


