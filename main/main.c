#include "../QEI/QEI.c"
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
  int minVoltage;
  int index = 3;
  int distance;
  
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
    
  uart0_fputs("Esto es una prueba de la UART0");
  uart0_fputs("\n");
  uart0_fputs("Introduzca el comando:\n");
  
  uart0_gets(rx_msg);
  uart0_fputs("El mensaje enviado ha sido: ");
  uart0_fputs(rx_msg);
  length = strlen(rx_msg);
  delimitedChar(rx_msg,strBuffer, 1,2);
  minVoltage = atoi(strBuffer);
  battery_sampling_init(1, minVoltage);

  while(index < length - 1) 
  {
  switch (rx_msg[index])
  {
    case 'A' : 
    {
      delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
      distance = atoi(strBuffer);
      QEI_go_front(distance);
      uart0_fputs("Haciendo A\n");
      break;
    }
    case 'R' : 
    {
      delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
      distance = atoi(strBuffer);
      QEI_turn_vehicle( 0 );
      QEI_go_front(distance);
      uart0_fputs("Haciendo R\n");
      break;
    }
    case 'L' : 
    {
      delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
      distance = atoi(strBuffer);
      QEI_turn_vehicle( 1 );
      QEI_go_front(distance);
      uart0_fputs("Haciendo L\n");
      break;
    }
  }
  index += 3;
  }
  while(1);
}
