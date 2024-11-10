#include "../PWM/pwm.c" //incluimos el archivo con funciones de pwm del anterior hito
#include <LPC17xx.h>

#define F_CPU             SystemCoreClock // From system_LPC17xx.h
#define F_PCLK            F_CPU/4
#define QEI_FUNCTION      0x1
#define PCLK_QEI_MASK     0x00000003
#define PCLK_QEI_DIV_4    0x2
#define F_PCLK_QEI        F_PCLK
#define ENCODER_PPR       11          // pulsos de encoder por revolucion
#define EDGES             4           // Resolucion de QEI
#define R_GEARBOX         35          // factor de la reductora
#define Wheel_R           6.60 / 2


float static T_Speed;
volatile float _qei_pos;
volatile int _qei_dir;
volatile float _qei_speed;
volatile int _qei_err;
volatile int _rpm;
volatile float _cm;
volatile int dir;
volatile int _qei_pos_old;

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
}


void QEI_IRQHandler(void) 
 {
  uint32_t active_flags;
  active_flags = LPC_QEI->QEIINTSTAT & LPC_QEI->QEIIE;
  LPC_QEI->QEICLR = active_flags;                           // Clear active flags
  if (active_flags & QEI_ERR_INT) {
  _qei_err = 1;
  }
}


int QEI_get_speed(float *speed_rpm)
{
  *speed_rpm = ( LPC_QEI->QEICAP * dir * 2 * 3.14 * Wheel_R) / ( R_GEARBOX * ENCODER_PPR * EDGES * T_Speed );
  return 1;
}

int main() 
{
  float distance;
  pwm_config(1/1e3);
  QEI_config(10e-6, 10e-3);
  dir = (LPC_QEI->QEISTAT & 0x01)? -1 : +1;
  _qei_pos = dir ? LPC_QEI->QEIPOS : _qei_pos - LPC_QEI->QEIPOS + _qei_pos;
  _qei_pos_old = _qei_pos;
  _cm = ( _qei_pos / ( R_GEARBOX * ENCODER_PPR) );                              // * 2 * 3.14 * Wheel_R ;
   pwm_set_duty_cycle(-0.25,0.25);                                              // Set duty cycle to 25%
  
  distance = 2 * 3.14 * Wheel_R;
  while( distance * 0.93 > _cm)
  {
    _rpm = LPC_QEI->QEIPOS;
    dir = (LPC_QEI->QEISTAT & 0x1)? -1 : +1;
    if( dir == 1)
    {
      if( _rpm > _qei_pos_old)
      {
        _qei_pos = _qei_pos + _rpm - _qei_pos_old;
        _qei_pos_old = _rpm;
      }
    }
    else 
    {
      if( _qei_pos_old > _rpm )
      {
        _qei_pos = _qei_pos + _qei_pos_old - _rpm;
        _qei_pos_old = _rpm;
      }
    }
    _cm = ( _qei_pos / ( R_GEARBOX * ENCODER_PPR * EDGES )) * 2 * 3.14 * Wheel_R ;
    _qei_speed = ( LPC_QEI->QEICAP * dir * 2 * 3.14 * Wheel_R) / ( R_GEARBOX * ENCODER_PPR * EDGES * T_Speed );
  }  
  pwm_stop();
  return 0;
}
