#include <LPC17xx.h>
#include "../UART/uart.c"
#include "../GLCD/GLCD.c"
#include <string.h>
#include <stdio.h>

#define F_CPU     (SystemCoreClock)   // From system_LPC17xx.h
#define F_PCLK    (F_CPU/4)
#define MENU_DICT_LEN (sizeof(menu_dict)/sizeof(dict_entry))

char tx_msg[128];
char rx_msg[128];
/*
int prueba() {
  int ret;

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
*/

