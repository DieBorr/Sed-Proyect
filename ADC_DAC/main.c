#include "ADC_DAC.c"

int main()
{
  ADC_init();
  pwm_config(1/1e3);
  alarm_init();
  alarm_set_freq(0);
  battery_sampling_init(1.5);
  alarm_set_freq(10000);
  while(1)
  {
    speed_get_value(1);
    pwm_set_duty_cycle(-*speedPtr, *speedPtr);
    alarm_set_freq(10000);
  }
}