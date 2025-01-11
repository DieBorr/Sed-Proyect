#include "../PWM/pwm.c" //incluimos el archivo con funciones de pwm del anterior hito
#include <LPC17xx.h>
#include "../GLCD/GLCD.h"
#include <stdio.h>
#include <string.h>
#include "../ADC_DAC/ADC_DAC.c"

#define F_CPU             SystemCoreClock // From system_LPC17xx.h
#define F_PCLK            F_CPU/4
#define QEI_FUNCTION      0x1
#define PCLK_QEI_MASK     0x00000003
#define PCLK_QEI_DIV_4    0x2
#define F_PCLK_QEI        F_PCLK
#define ENCODER_PPR       11          // pulsos de encoder por revolucion
#define EDGES             4           // Resolucion de QEI
#define R_GEARBOX         34          // factor de la reductora
#define Wheel_R           6.60 / 2
#define M_PI              3.14159265358979323846f

float static T_Speed;
float qei_pos;
volatile int _qei_dir;
float qei_speed;
int* qei_speedPtr;
int qei_err;
int rpm;
int cm;
int dir;
int qei_pos_old;
static int velocidad;
volatile float speed_Value2;
volatile int  variable = 1;
volatile int totalDistance = 0;
char buffer[2];
volatile int prueba;

void QEI_config(float t_glitch, float T_obs)
 {
  uint32_t filter_code, n_obs;
  T_Speed = T_obs;
  
  LPC_SC->PCLKSEL1 &= ~PCLK_QEI_MASK;
  LPC_SC->PCLKSEL1 |= PCLK_QEI_DIV_4;               // PCLK_QEI = CCLK/4 (25 MHz)
  LPC_SC->PCONP |= PCQEI;                           // Quadrature Encoder Interface power on
  LPC_PINCON->PINSEL3 |= (QEI_FUNCTION << 8);       // P1.20 MCI0
  LPC_PINCON->PINSEL3 |= (QEI_FUNCTION << 14);      // P1.23 MCI1
  filter_code = (uint32_t)(F_PCLK_QEI * t_glitch);
  LPC_QEI->QEICON = QEICON_RESP | QEICON_RESV;      // reset position & velocity counters
  LPC_QEI->QEICONF = QEICONF_CAPMODE;               // Modo de cuadratura x4
  LPC_QEI->QEIMAXPOS = 0xFFFFFFFF;
  LPC_QEI->FILTER = filter_code;
  n_obs = (uint32_t) (T_obs*F_PCLK_QEI);            // Tiempo de observ
  LPC_QEI->QEILOAD = n_obs;

  LPC_QEI->QEIIEC = ~0x0;
  LPC_QEI->QEIIES = QEI_ERR_INT;                    // Solo habilitamos la interrupcion por error.
  NVIC_EnableIRQ(QEI_IRQn);
  NVIC_SetPriority(QEI_IRQn, 4);
   
  dir = (LPC_QEI->QEISTAT & 0x01)? -1 : 1;
  qei_pos = dir ? LPC_QEI->QEIPOS : qei_pos - LPC_QEI->QEIPOS + qei_pos;
  qei_pos_old = qei_pos;
}

void QEI_IRQHandler(void) 
 {
  uint32_t active_flags;
  active_flags = LPC_QEI->QEIINTSTAT & LPC_QEI->QEIIE;
  LPC_QEI->QEICLR = active_flags;                           // Clear active flags
  if (active_flags & QEI_ERR_INT) {
  qei_err = 1;
  }
}

int QEI_get_speed()
{
  int speed_ValueToInt = ( LPC_QEI->QEICAP * dir * 2 * M_PI * Wheel_R) / ( R_GEARBOX * ENCODER_PPR * EDGES * T_Speed );
  if(speed_ValueToInt > 100) speed_ValueToInt = LPC_QEI->QEIMAXPOS - speed_ValueToInt;
  prueba = speed_ValueToInt;
  return speed_ValueToInt;
}

void int_to_char(int num, char *buffer) {
    int i = 0;
    int j;
    int is_negative = 0;
    char temp;

    // Limpiar el buffer
    while (buffer[i] != '\0') {
        buffer[i] = '\0';
        i++;
    }
    i = 0; // Reiniciar el índice para llenar el buffer

    // Manejar el caso de un número negativo
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    // Extraer dígitos de forma inversa
    do {
        buffer[i++] = (num % 10) + '0'; // Convertir dígito a carácter
        num /= 10;
    } while (num > 0);

    // Agregar el signo negativo si corresponde
    if (is_negative) {
        buffer[i++] = '-';
    }

    // Terminar la cadena con un carácter nulo
    buffer[i] = '\0';

    // Invertir el buffer para obtener el orden correcto
    for (j = 0; j < i / 2; j++) {
        temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
}


int QEI_go_front( float distance, float duty_cycle )
{
  float old_distance = cm;
  int almendra = 3;
  int old_speed = 0;
  float speedValue;
  
  speed_get_value(0.001);
  cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR) );                              // * 2 * 3.14 * Wheel_R ;
  pwm_set_duty_cycle(-1 * (speedValue), 1 * (speedValue));                  // Set duty cycle to 25%
  // La finalidad del ciclo while es que la distancia siempre sea positiva aunque la direccion sea negativa para poder tener la distancia recorrida efectiva.
  //distance = (2 * M_PI * 11.8) / 4 ;
  while( distance * 0.93 + old_distance > cm)   
  {
    speedValue = get_speed();
    speed_get_value();
    pwm_set_duty_cycle(-1 * (speedValue), 1 * (speedValue));
    rpm = LPC_QEI->QEIPOS;
    dir = (LPC_QEI->QEISTAT & 0x1)? -1 : +1;
    if( dir == 1)
    {
      if( rpm > qei_pos_old)
      {
        qei_pos = qei_pos + rpm - qei_pos_old;
        qei_pos_old = rpm;
      }
    }
    else 
    {
      if( qei_pos_old > rpm )
      {
        qei_pos = qei_pos + qei_pos_old - rpm;
        qei_pos_old = rpm;
      }
    }
    cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR * EDGES )) * 2 * M_PI * Wheel_R ;
    //QEI_get_speed();
    if(abs(QEI_get_speed() - old_speed) > 1) {
      almendra = distance + + old_distance;
      //if(abs(QEI_get_speed() - old_speed) > 2) continue;
      int_to_char(QEI_get_speed(), buffer);
      GUI_Text(150,0,buffer,Blue,Black) ;
      variable = 0;
      old_speed = QEI_get_speed();
    }
    else variable++; 
    if(cm % 5 == 0) {
    int_to_char(cm, buffer);
    GUI_Text(150,30,buffer,Blue,Black);
    }
  }
  totalDistance += distance;
  int_to_char(totalDistance, buffer);
  GUI_Text(150,30,buffer,Blue,Black);
  pwm_stop();
  return 1;
}

int QEI_turn_vehicle( uint8_t left )
{
  float old_distance = cm;
  float distance = (2 * M_PI * 11.8) / 4 ;
  cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR) );                              // * 2 * 3.14 * Wheel_R ;
  left ? pwm_set_duty_cycle(0.4, 0.4) : pwm_set_duty_cycle(-0.4, -0.4);

  while( distance * 0.93 + old_distance > cm)   
  {
    rpm = LPC_QEI->QEIPOS;
    dir = (LPC_QEI->QEISTAT & 0x1)? -1 : +1;
    if( dir == 1)
    {
      if( rpm > qei_pos_old)
      {
        qei_pos = qei_pos + rpm - qei_pos_old;
        qei_pos_old = rpm;
      }
    }
    else 
    {
      if( qei_pos_old > rpm )
      {
        qei_pos = qei_pos + qei_pos_old - rpm;
        qei_pos_old = rpm;
      }
    }
    cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR * EDGES )) * 2 * M_PI * Wheel_R ;
  }
  totalDistance += distance;
  int_to_char(totalDistance, buffer);
  GUI_Text(150,30,buffer,Blue,Black);
  pwm_stop();
  return 1;
}
