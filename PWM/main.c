#include "pwm.c"

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