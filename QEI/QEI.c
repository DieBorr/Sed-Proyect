#include "../PWM/pwm.c"
#include "../ADC_DAC/ADC_DAC.c"
#include "../GLCD/GLCD.h"
#include <LPC17xx.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
float qei_speed;
int qei_err;
int qei_new_pos;
int cm;
int dir;
int qei_pos_old;
volatile int totalDistance = 0;
char buffer[3];

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
  return speed_ValueToInt;
}

void int_to_char(int num, char *buffer) {
    int i = 0;
    int j;
    int is_negative = 0;
    char temp;

    while (buffer[i] != '\0')
    {
        buffer[i] = '\0';
        i++;
    }
    i = 0;
    if (num < 0)
    {
        is_negative = 1;
        num = -num;
    }
    do
    {
        buffer[i++] = (num % 10) + '0';
        num /= 10;
    } while (num > 0);
    if (is_negative) buffer[i++] = '-';
    buffer[i] = '\0';

    for (j = 0; j < i / 2; j++)
    {
        temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
}

int QEI_go_front( float distance )
{
  float old_distance = cm;
  int old_speed = 0;
  float speedValue;
  
  speed_get_value();
  cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR) );
  pwm_set_duty_cycle(-1 * (speedValue), 1 * (speedValue));
  while( distance * 0.93 + old_distance > cm)
  {
    speedValue = get_speed();
    speed_get_value();
    pwm_set_duty_cycle(-1 * (speedValue), 1 * (speedValue));
    qei_new_pos = LPC_QEI->QEIPOS;
    dir = (LPC_QEI->QEISTAT & 0x1)? -1 : +1;
    if( dir == 1)
    {
      if( qei_new_pos > qei_pos_old)
      {
        qei_pos = qei_pos + qei_new_pos - qei_pos_old;
        qei_pos_old = qei_new_pos;
      }
    }
    else 
    {
      if( qei_pos_old > qei_new_pos )
      {
        qei_pos = qei_pos + qei_pos_old - qei_new_pos;
        qei_pos_old = qei_new_pos;
      }
    }
    cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR * EDGES )) * 2 * M_PI * Wheel_R ;
    if(abs(QEI_get_speed() - old_speed) > 1 && abs(QEI_get_speed()) < 60)
    {
      int_to_char(QEI_get_speed(), buffer);
      GUI_Text(180,0,(uint8_t *)buffer,Blue,Black) ;
      old_speed = QEI_get_speed();
    }
    if(cm % 5 == 0)
    {
    int_to_char(cm, buffer);
    GUI_Text(180,30,(uint8_t *)buffer,Blue,Black);
    }
  }
  totalDistance += distance;
  int_to_char(totalDistance, buffer);
  GUI_Text(180,30,(uint8_t *)buffer,Blue,Black);
  pwm_stop();
  return 1;
}

int QEI_turn_vehicle( uint8_t left )
{
  float old_distance = cm;
  float distance = (2 * M_PI * 12.5) / 4 ;
  cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR) );
  left ? pwm_set_duty_cycle(0.4, 0.4) : pwm_set_duty_cycle(-0.4, -0.4); // velocidad fija

  while( distance * 0.85 + old_distance > cm)   
  {
    qei_new_pos = LPC_QEI->QEIPOS;
    dir = (LPC_QEI->QEISTAT & 0x1)? -1 : +1;
    if( dir == 1)
    {
      if( qei_new_pos > qei_pos_old)
      {
        qei_pos = qei_pos + qei_new_pos - qei_pos_old;
        qei_pos_old = qei_new_pos;
      }
    }
    else 
    {
      if( qei_pos_old > qei_new_pos )
      {
        qei_pos = qei_pos + qei_pos_old - qei_new_pos;
        qei_pos_old = qei_new_pos;
      }
    }
    cm = ( qei_pos / ( R_GEARBOX * ENCODER_PPR * EDGES )) * 2 * M_PI * Wheel_R ;
  }
  totalDistance += distance;
  int_to_char(totalDistance, buffer);
  GUI_Text(150,30,(uint8_t *)buffer,Blue,Black);
  pwm_stop();
  return 1;
}
