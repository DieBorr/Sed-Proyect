#include "QEI.c"
#include "../GLCD/GLCD.c"
#include "../GLCD/AsciiLib.c"
#include "../UART/UART.c"
#include "../ADC_DAC/ADC_DAC.c"

char tx_msg[128];
char rx_msg[128];


int char_to_int(const char *str) {
    int result = 0;
    int sign = 1; // Por defecto positivo
    int i = 0;

    // Manejar signo
    if (str[0] == '-') {
        sign = -1; // Si es negativo
        i++;
    } else if (str[0] == '+') {
        i++; // Si es explícitamente positivo
    }

    // Convertir cada carácter al entero correspondiente
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    // Devolver el resultado con el signo
    return result * sign;
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
  
  pwm_config(1/1e3);
  pwm_set_duty_cycle(0,0);
  QEI_config(10e-6, 10e-3);
  LCD_Initializtion();
  ADC_init();
  alarm_init();
   alarm_set_freq(10000);
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
  QEI_go_front(100,0.5);
  QEI_turn_vehicle( 1 );
  while(1);
}
