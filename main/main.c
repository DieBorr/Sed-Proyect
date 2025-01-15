#include "../QEI/QEI.c"
#include "../GLCD/GLCD.c"
#include "../GLCD/AsciiLib.c"
#include "../UART/UART.c"

char tx_msg[128];
char rx_msg[128];
uint8_t buttonPressed;

int char_to_int(const char *str)
{
  int result = 0;
  int i = 0;
  int length= strlen(str);
// Convertir cada carácter al entero correspondiente
  while (str[i] >= '0' && str[i] <= '9') 
  {
      result = result * 10 + (str[i] - '0');
      i++;
  }
  if(length != i) return -1;
  else return result;
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

void setupEINT2(void) {
    // Configura el pin P2.11 como EINT2
    LPC_PINCON->PINSEL4 &= ~(3 << 22); // Limpia bits 22 y 23
    LPC_PINCON->PINSEL4 |= (1 << 24);  // Configura P2.11 como EINT2

    // Configura el tipo de interrupción en el flanco de bajada
    LPC_SC->EXTMODE |= (1 << 2);       // Configura EINT2 como interrupción por flanco
    LPC_SC->EXTPOLAR |= (1 << 2);      // Configura flanco de subida

    // Habilita la interrupción de EINT2 en el NVIC
    NVIC_EnableIRQ(EINT2_IRQn);

    // Limpia cualquier flag previo
    LPC_SC->EXTINT |= (1 << 2);
}

void EINT2_IRQHandler(void) {
    // Verifica si la interrupción fue generada por EINT2
    if (LPC_SC->EXTINT & (1 << 2)) {
        // Aquí puedes realizar la acción deseada si el botón fue pulsado
        // Por ejemplo, encender un LED, enviar datos, etc.
        LPC_GPIO1->FIOSET = (1 << 18);  // Ejemplo: Enciende LED en P1.18
        buttonPressed = 1;

        // Limpia el flag de interrupción de EINT2
        LPC_SC->EXTINT |= (1 << 2);
    }
}

int main() 
{
  int ret;
  int length;
  char strBuffer[30];
  int minVoltage;
  int index = 0;
  int distance;
  
  pwm_config(1/1e3);
  pwm_set_duty_cycle(0,0);
  QEI_config(10e-6, 10e-3);
  LCD_Initializtion();
  ADC_init();
  alarm_init();
  setupEINT2();

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
  uart0_fputs("\n");
  // Esperamos a que se pulse KEY2
  buttonPressed = 0;
  uart0_fputs("Por favor presione KEY2.\n");
  while(!buttonPressed);
  GUI_Text(0,0,(uint8_t *)"Valor de la velocidad: ",Blue,Black);
  GUI_Text(0,30,(uint8_t *)"Valor de la distancia: ",Blue,Black);
  GUI_Text(0,60,(uint8_t *)"Valor de la bateria: ",Blue,Black);

  while(index < length - 1 )
  {
    switch (rx_msg[index])
    {
      case 'U' :
      {
        delimitedChar(rx_msg,strBuffer, 1,2);
        minVoltage = char_to_int(strBuffer);
        if(minVoltage == -1) goto error;
        battery_sampling_init(5, minVoltage);
        break;
      }
      case 'F' : 
      {
        delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
        distance = char_to_int(strBuffer);
        if(distance == -1) goto error;    //Usamos goTo para manejar todos los errores.
        uart0_fputs("Haciendo F\n");
        QEI_go_front(distance);
        break;
      }
      case 'R' :
      {
        delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
        distance = char_to_int(strBuffer);
        if(distance == -1) goto error;
        uart0_fputs("Haciendo R\n");
        QEI_turn_vehicle( 0 );
        QEI_go_front(distance);
        break;
      }
      case 'L' :
      {
        delimitedChar(rx_msg,strBuffer,index + 1, index + 2);
        distance = char_to_int(strBuffer);
        if(distance == -1) goto error;
        uart0_fputs("Haciendo L\n");
        QEI_turn_vehicle( 1 );
        QEI_go_front(distance);
        break;
      }
      default :
      {
        goto error;
      }
    }
    index += 3;
  }
  uart0_fputs("Programa finalizado.\n");
  while(1);

  error:  // Etiqueta del goTo
  {
    uart0_fputs("Error, comando desconocido, deteniendo vehiculo.\n");
    pwm_stop();
    while(1);
  }
}
