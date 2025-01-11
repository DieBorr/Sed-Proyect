#include "QEI.c"
#include "../GLCD/GLCD.c"
#include "../GLCD/AsciiLib.c"
#include "../UART/UART.c"

char tx_msg[128];
char rx_msg[128];

int char_to_int(const char *str) {
  int result = 0;
  int i = 0;

    // Convertir cada carácter al entero correspondiente
  while (str[i] >= '0' && str[i] <= '9') {
      result = result * 10 + (str[i] - '0');
      i++;
  }

  return result;
}

void delimitedChar(const char* str, char* returnPointer, uint8_t from, uint8_t to)
{
  int i,j = 0;
  for(i = from; i <= to ; i++)
  {
    returnPointer[j] = str[i];
    j++;
  }
  returnPointer[j++] = '\0';
}
int main() 
{
  int ret;
  int length;
  char strBuffer[30];
  int num;
  int dummyVariable;
  int index = 3;
  char b;
  
  pwm_config(1/1e3);
  pwm_set_duty_cycle(0,0);
  QEI_config(10e-6, 10e-3);
  LCD_Initializtion();
  ADC_init();
  alarm_init();
  // Serial port initialization:
  ret = uart0_init(9600);
  if(ret < 0) {
    return -1;
  }
    
  uart0_fputs("Esto es una strBuffer de la UART0");
  uart0_fputs("\n");
  uart0_fputs("Introduzca el comando:\n");
  
  uart0_gets(rx_msg);
  uart0_fputs("El mensaje enviado ha sido: ");
  uart0_fputs(rx_msg);
  length = strlen(rx_msg);
  uart0_fputs("Su longitud es de: ");
  int_to_char(length, tx_msg);
  uart0_fputs(tx_msg);
  uart0_fputs("\n");
  uart0_fputs("Los dos primeros caracteres son: ");
  delimitedChar(rx_msg,strBuffer, 1,2);
  uart0_fputs(strBuffer);
  num = atoi(strBuffer);
  battery_sampling_init(1, num);
  uart0_fputs("Los numeros son: \n");
  int_to_char(num, strBuffer);
  uart0_fputs(strBuffer);
  GUI_Text(0,0,"La velocidad es: ",Blue,Black);
  GUI_Text(0,30,"La distancia es: ",Blue,Black);

  while(index < length - 1) 
  {
  switch (rx_msg[index])
  {
    case 'A' : 
    {
      delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
      num = atoi(strBuffer);
      QEI_go_front(num,0.5);
      uart0_fputs("Haciendo A\n");
      break;
    }
    case 'R' : 
    {
      delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
      num = atoi(strBuffer);
      QEI_turn_vehicle( 0 );
      QEI_go_front(num,0.5);
      uart0_fputs("Haciendo R\n");
      break;
    }
    case 'L' : 
    {
      delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
      num = atoi(strBuffer);
      QEI_turn_vehicle( 1 );
      QEI_go_front(num,0.5);
      break;
    }
  }
  uart0_fputs("Bucle\n");
  index += 3;
  }
  while(1);
}
