// Este archivo sigue el coding style propuesto por la asignatura (Hoja de Estilo de Programacion en C) (Documentos)

#include <LPC17xx.h>
#include "reg_masks.h"

#define F_cpu 100e6          // Defecto Keil (xtal=12Mhz)
#define F_pclk F_cpu/4       // PCLK configurado por defecto

#define PIN_DIR_WHEEL_R 0x01 // P0.0 => direccion para rueda derecha
#define PIN_DIR_WHEEL_L 0x02 // P0.1 => direccion para rueda izquierda
#define WHEEL_R         0
#define WHEEL_L         1

void pwm_config(float Tpwm)
 {
  LPC_SC->PCONP |= (1<<6);
  LPC_GPIO0->FIODIR |= (PIN_DIR_WHEEL_L | PIN_DIR_WHEEL_R); // wheel dir pins como output In(3) / In(1)
  LPC_GPIO0->FIOCLR = (PIN_DIR_WHEEL_L | PIN_DIR_WHEEL_R);  // wheel dir pins = 0 (positiva)
  LPC_PINCON->PINSEL3 |= (0x2 << 10) | (0x2 << 16);         // P1.21 as PWM1.3 output  P1.24 output as PWM1.5 for R // Se colocan en In(4) / In(2) respectivamente.
  LPC_PWM1->MR0 = (F_pclk * Tpwm) - 1;                      // 25KHZ / 1000 -1 = 24 , mr0 envia un cambio de nivel al contar 24 entradas (24+1) 
  LPC_PWM1->PCR |= (1 << 11) | (1 << 13);                   // Enable PWM1.3 y PWM1.5
  LPC_PWM1->MCR |= (1<<1);                                  // Reset cuando mr0 tenga valor seleccionado
  LPC_PWM1->TCR = (1 << 0) | (1 << 3); ;                    // Reset PWM TC & PR // Enable PWM, Enable Counter
}

void pwm_set_period( float Tpwm) 
{
  LPC_PWM1->MR0 = (F_pclk * Tpwm) - 1; // Misma operacion que al principio;
}

float pwm_get_period() 
{
  return ( (LPC_PWM1->MR0 + 1) / F_pclk ) ; // operacion inversa
}

void pwm_set_duty_cycle(float dc_L, float dc_R) 
{
  if( dc_L >= 0 ) // horario visto desde el culo del motor
  {  
    LPC_GPIO0->FIOCLR = PIN_DIR_WHEEL_L;
    LPC_PWM1->MR3 = LPC_PWM1->MR0 * dc_L; // La se;al empieza en nivel alto, cuando llega a MR3 baja, y despues al llegar a mr0 vuelve a subir, por tanto MR3 es de menor valor que mr0
  }
  else
  {
    LPC_GPIO0->FIOSET = PIN_DIR_WHEEL_L;
    LPC_PWM1->MR3 = LPC_PWM1->MR0 * (1 + dc_L );
  }
  if( dc_R >= 0 )
  {
    LPC_GPIO0->FIOCLR = PIN_DIR_WHEEL_R;
    LPC_PWM1->MR5 = LPC_PWM1->MR0 * dc_R; // La se;al empieza en nivel alto, cuando llega a MR3 baja, y despues al llegar a mr0 vuelve a subir, por tanto MR3 es de menor valor que mr0
  } 
  else
  {
    LPC_GPIO0->FIOSET = PIN_DIR_WHEEL_R;
    LPC_PWM1->MR5 = LPC_PWM1->MR0 * (1 + dc_R );
  }
  LPC_PWM1->LER |= (1 << 0) | (1 << 3) | (1 << 5); // Carga MR0 (período), MR3 (PWM1.3), MR5 (PWM1.5)
}

float pwm_get_duty_cycle(int wheel) 
{
  float duty;
  if(wheel == WHEEL_L) // Si es la rueda izquierda
  { 
    if (LPC_GPIO0->FIOPIN & PIN_DIR_WHEEL_L)
    {
      // Rueda en sentido antihorario (negativo)
      duty = -((LPC_PWM1->MR0 - LPC_PWM1->MR3) / (float)LPC_PWM1->MR0);
    } 
    else 
    {
      // Rueda en sentido horario (positivo)
      duty = LPC_PWM1->MR3 / (float)LPC_PWM1->MR0;
    }
  } 
  else if(wheel == WHEEL_R) // Si es la rueda derecha
  { 
    if (LPC_GPIO0->FIOPIN & PIN_DIR_WHEEL_R)
    {
      // Rueda en sentido antihorario (negativo)
      duty = -((LPC_PWM1->MR0 - LPC_PWM1->MR5) / (float)LPC_PWM1->MR0);
    }
    else
    {
      // Rueda en sentido horario (positivo)
      duty = LPC_PWM1->MR5 / (float)LPC_PWM1->MR0;
    }
  }  
  return duty;
}


void pwm_stop(void) 
{
  pwm_set_duty_cycle(0,0) ;
  LPC_GPIO0->FIOCLR |= PIN_DIR_WHEEL_L; // Igualamos tensiones de los bornes para que se pare cuanto antes
  LPC_GPIO0->FIOCLR |= PIN_DIR_WHEEL_R;
}

int main()
{
  int static delay;
  pwm_config(1.0/1e3); // freq_PWM = 1KHz
  while(1)
  {
    pwm_set_duty_cycle( 1 , 1);                    // float dc_L, float dc_R
    for (  delay = 0; delay < 10000000; delay++);  // Agregar un retraso determinado
    pwm_set_duty_cycle( -1 , 1);                   // float dc_L, float dc_R
    for (  delay = 0; delay < 10000000; delay++);  // Agregar un retraso determinado
    pwm_set_duty_cycle( 1 , -1);                   // float dc_L, float dc_R
    for (  delay = 0; delay < 10000000; delay++);  // Agregar un retraso determinado
    pwm_set_duty_cycle( -1 , -1);                  // float dc_L, float dc_R
    for (  delay = 0; delay < 10000000; delay++);  // Agregar un retraso determinado
    pwm_stop();                                    // Parada
  }
  return 0; 
}
