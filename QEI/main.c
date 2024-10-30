#include "../PWM/pwm.h" //incluimos el header con funciones de pwm del anterior hito
#include <LPC17xx.h>

#define F_CPU             SystemCoreClock // From system_LPC17xx.h
#define F_PCLK            F_CPU/4
#define QEI_FUNCTION      0x1
#define PCLK_QEI_MASK     0x00000003
#define PCLK_QEI_DIV_4    0x2
#define F_PCLK_QEI        F_PCLK
#define ENCODER_PPR       11          // encoder pulses per revolution
#define EDGES             4           // QEI resolution (inc/dec per cycle)
#define R_GEARBOX         35          // gearbox reduction ratio
#define Wheel_R           6.656 / 2


float static k_speed;
void pwm_config(float Tpwm);

void enc_setup_hw(float t_glitch) {
  uint32_t filter; // F_PCLK
  LPC_SC->PCLKSEL1 |= (0 << 0); // CCLK/4 PCLK_QEI
  LPC_SC->PCONP |= PCQEI; // Quadrature Encoder Interface power On
  LPC_PINCON->PINSEL3 |= (QEI_FUNCTION << 8); // P1.20 MCI0
  LPC_PINCON->PINSEL3 |= (QEI_FUNCTION << 14); // P1.23 MCI1
  filter = (uint32_t)(F_PCLK*t_glitch);
  LPC_QEI->QEICON = QEICON_RESP; // Reset position counter
  LPC_QEI->QEICONF = QEICONF_CAPMODE; // Resolution 4X
  LPC_QEI->QEIMAXPOS = 0xFFFFFFFF; // maximum value of the encoder position
  LPC_QEI->FILTER = filter;
  // Interrupt configuration.
  // Because QEIIE register is read only:
  LPC_QEI->QEIIEC = ~0x0; // firstly clear the register...
  LPC_QEI->QEIIES = QEI_ENCLK_INT | QEI_ERR_INT; // ...and then enable the ints
  NVIC_EnableIRQ(QEI_IRQn);
  NVIC_SetPriority(QEI_IRQn, 4);
  
}

void QEI_config(float t_glitch, float T_obs) {
  uint32_t filter_code, n_obs;
  LPC_SC->PCLKSEL1 &= ~PCLK_QEI_MASK;
  LPC_SC->PCLKSEL1 |= PCLK_QEI_DIV_4; // PCLK_QEI = CCLK/4 (25 MHz)
  LPC_SC->PCONP |= PCQEI; // Quadrature Encoder Interface power on
  LPC_PINCON->PINSEL3 |= (QEI_FUNCTION << 8); // P1.20 MCI0
  LPC_PINCON->PINSEL3 |= (QEI_FUNCTION << 14); // P1.23 MCI1
  filter_code = (uint32_t)(F_PCLK_QEI * t_glitch);
  LPC_QEI->QEICON = QEICON_RESP | QEICON_RESV; // reset position & velocity counters
  LPC_QEI->QEICONF = QEICONF_CAPMODE; // Quadrature mode & resolution (edges) = x4
  LPC_QEI->FILTER = filter_code;
  n_obs = (uint32_t) (T_obs*F_PCLK_QEI); // Observation time
  LPC_QEI->QEILOAD = n_obs;
  k_speed = (60.0 )/(EDGES * ENCODER_PPR * n_obs ); // for wheel speed
  // Interrupt configuration. Because QEIIE register is read only:
  LPC_QEI->QEIIEC = ~0x0; // first clear the register...
  LPC_QEI->QEIIES = QEI_TIM_INT; // ...and then enable the ints
  NVIC_EnableIRQ(QEI_IRQn);
  NVIC_SetPriority(QEI_IRQn, 4);
}

volatile int _qei_pos;
volatile int _qei_dir;
volatile int _qei_speed;
volatile int _qei_err;
volatile int _qei_pos_token = 0;
volatile int _rpm;
volatile int _cm;
void QEI_IRQHandler(void) {
  uint32_t active_flags;
  uint8_t dir;
  active_flags = LPC_QEI->QEIINTSTAT & LPC_QEI->QEIIE;
  LPC_QEI->QEICLR = active_flags; // Clear active flags
  if (active_flags & QEI_ENCLK_INT) {
    dir = (LPC_QEI->QEISTAT & 0x01)? -1 : +1;
    _qei_pos = dir ? LPC_QEI->QEIPOS : _qei_pos - LPC_QEI->QEIPOS + _qei_pos;
    _cm = ( _qei_pos / ( R_GEARBOX * ENCODER_PPR) ) * 2 * 3.14 * Wheel_R ;
    _qei_pos_token = 1;
  }
  if (active_flags & QEI_ERR_INT) {
  _qei_err = 1;
  }
}


int QEI_get_speed(float *speed_rpm);
float dbg_motor_speed;

int main() {
  float motor_speed = 0.0;
  pwm_config(1.0/20e3); // Set PWM freq = 20 KHz
  pwm_set_duty_cycle(1,1); // Set duty cycle to 100%
  QEI_config(10e-6, 10e-3);
  while(1) {

  return 0;
}
