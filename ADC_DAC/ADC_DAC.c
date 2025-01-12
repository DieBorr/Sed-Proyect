#include <LPC17xx.h>
#include <math.h>
#include "../PWM/reg_masks.h"

// Constantes
#define F_cpu         100e6        // Frecuencia del sistema (xtal = 12 MHz)
#define F_pclk        F_cpu/4      // PCLK configurado por defecto
#define voltageDiv    2.568
#define Vref          3.3
#define M_PI          3.14159265358979323846f
#define N_POINTS      16
#define DAC_N_BITS    10
#define DAC_N_LEVELS (1U << DAC_N_BITS)
#define DAC_MID_RANGE (1U << (DAC_N_BITS-1))

static uint16_t sample_table[N_POINTS];
static uint8_t  sample_idx;

// Variables globales
float speed = -1;
volatile float* speedPtr = &speed;
volatile float batValue;
volatile float* batVauePtr = &batValue;
volatile uint8_t speedTimerTrigger = 0;
static uint8_t batteryInit = 0;
static uint8_t speedInit = 0;
volatile uint8_t batteryTrigger = 0;
volatile uint8_t speedTrigger = 0;
volatile float batteryMinValue;

float get_speed() 
{
 return speed; 
}
void alarm_init(void)
{
  int i;
  float x;
  LPC_PINCON->PINSEL1 |= (2 << 20);            // DAC output = P0.26 (AOUT)
  LPC_PINCON->PINMODE1 |= (2 << 20);           // disable pull up & down
  LPC_DAC->DACCTRL = 0;
  LPC_SC->PCONP |= (1 << 22);
  LPC_TIM2->MCR = (1 << 0) | (1 << 1);         // Reset en MR0.0 e interrupción
  NVIC_EnableIRQ(TIMER2_IRQn);                 // Habilitar IRQ de Timer1

  for(i = 0; i < N_POINTS; i++)
  {
    x = DAC_MID_RANGE+(DAC_MID_RANGE-1)*sinf((2*M_PI/N_POINTS)*i);
    sample_table[i] = ((uint16_t)x) << 6;
  }
  sample_idx = 0;
}

void alarm_gen_sample(void)
{
  LPC_DAC->DACR = (uint32_t) sample_table[sample_idx];
  sample_idx = (sample_idx == (N_POINTS-1))? 0 : sample_idx+1;
}

void alarm_set_freq(int freq_hz)
{
  if ( F_pclk / LPC_TIM2->MR0 != freq_hz)
  {
    LPC_TIM2->TCR   |=   (3 << 0);                    // Reset Timer2 y Stop
    if(freq_hz != 0) 
    {
      LPC_TIM2->TCR &=  ~(3 << 0);                    // Limpiamos ese uno
      LPC_TIM2->MR0  =   ( F_pclk / freq_hz );
      LPC_TIM2->TCR |=   (1 << 0);                    // Iniciar Timer2
    }
    else LPC_TIM2->TCR |=  (3 << 0);
  }
}

void TIMER2_IRQHandler()
{
  LPC_TIM2->IR |= (1 << 0);                    // Clear int flag
  alarm_gen_sample();
}

void ADC_init()
{
  LPC_SC->PCONP |= (1 << 12);                  // Habilitar el ADC (PCONP bit 12)
  
  LPC_PINCON->PINSEL1 &= ~(3 << 14);           // Borrar bits para P0.23
  LPC_PINCON->PINSEL1 |= (1 << 14);            // Configurar P0.23 como AD0.0
  LPC_PINCON->PINSEL1 &= ~(3 << 16);           // Borrar bits para P0.24
  LPC_PINCON->PINSEL1 |= (1 << 16);            // Configurar P0.24 como AD0.1
  LPC_PINCON->PINMODE1 &= ~(3 << 14);          // Borrar bits de modo
  LPC_PINCON->PINMODE1 |= (2 << 14);           // Modo sin pull-up ni pull-down

  LPC_PINCON->PINMODE1 &= ~(3 << 16);          // Borrar bits de modo
  LPC_PINCON->PINMODE1 |= (2 << 16);           // Modo sin pull-up ni pull-down

  LPC_ADC->ADCR = (1 << 21)                    // Encender ADC
                 | (1 << 8)                    // CLKDIV = 2
                 | (1 << 0)                    // Seleccionar el canal AD0.0
                 | (1 << 21);                  // Habilitar ADC

  LPC_ADC->ADINTEN |= (3 << 0);                // AD0.0 y AD0.1 interrupción
  
  // Configuración de los timers
  LPC_SC->PCONP |= (1 << 1);                   // Encender Timer0
  LPC_TIM0->PR = 0;                            // Sin prescaler
  LPC_TIM0->MCR |= (1 << 4) | (1 << 3);        // Reset en MR0.1 e interrupción
  LPC_TIM0->EMR |= (2 << 6);                   // Set en Mat0.1


  //LPC_TIM1->EMR |= (2 << 4);                 // Set en Mat1.0

  NVIC_EnableIRQ(TIMER0_IRQn);                 // Habilitar IRQ de Timer0
  NVIC_SetPriority(ADC_IRQn,0);
  NVIC_EnableIRQ(ADC_IRQn);                    // Habilitar IRQ del ADC
}

void battery_sampling_init(float Ts, float batValueToGenTone)
{
  if (!batteryInit)
  {
    LPC_TIM0->MR1 = (Ts * F_pclk) - 1;             // Configurar tiempo Ts
    LPC_TIM0->EMR &= ~(3 << 6);                    // Clear del registro
    LPC_TIM0->EMR |=  (2 << 6);                    // Set en Mat0.1
    LPC_TIM0->TCR |=  (1 << 1);                    // Reset Timer0
    LPC_TIM0->TCR &= ~(1 << 1);               
    LPC_TIM0->TCR |=  (1 << 0);                    // Iniciar Timer0
    batteryMinValue = batValueToGenTone / 10;

    batteryInit = 1;                               // Marcar inicialización
    batteryTrigger = 0;
  }
}

void TIMER0_IRQHandler() {
  if(!speedTrigger)
  {
  LPC_TIM0->IR |= (1 << 1);       // Borrar el flag de interrupción del Match 0.1
  batteryTrigger = 1;
  LPC_TIM0->EMR &= ~(1 << 1);     // Mat0.1 a 0
  
  // Configuración del canal AD0.0 (batería)
  LPC_ADC->ADCR &= ~(0xFF);       // Limpiar cualquier canal previamente habilitado
  LPC_ADC->ADCR |= (1 << 0);      // Habilitar el canal AD0.0 para conversión de batería
  LPC_ADC->ADCR |= (1 << 24);     // Iniciar conversión
  }
}

void speed_get_value()
{
  if (!speedInit)
  {
    LPC_ADC->ADCR &= ~(0xFF);    // Limpiar cualquier canal previamente habilitado
    LPC_ADC->ADCR |= (1 << 1);   // Habilitar el canal AD0.0 para conversión de batería
    LPC_ADC->ADCR |= (1 << 24);  // Iniciar conversión
    speedInit = 1;               // Marcar inicialización
    speedTimerTrigger = 1;
  }
}

void ADC_IRQHandler(void)
{
  LPC_ADC->ADCR &= ~(1 << 24);
  // Verificar conversión de AD0.0 (batería)
  if (LPC_ADC->ADDR0 & (1U << 31) && batteryTrigger && !((LPC_ADC->ADGDR >> 24) & (0x7)) ) // Sabemos que se ha ejecutado esta conversión gracias al Mat0.1
  {
    float ADvoltage = ((LPC_ADC->ADDR0 >> 4) & 0xFFC);
    batValue =    ADvoltage * (( Vref * voltageDiv ) / 4095.0 ) ; // Leer valor ADC
    batteryTrigger = 0;
    batValue < batteryMinValue ? alarm_set_freq(10000) : alarm_set_freq(0);
  }

  // Verificar conversión de AD0.1 (velocidad)
  if ((LPC_ADC->ADDR1 & (1U << 31)) && speedInit)
  { 
    speed = ((LPC_ADC->ADDR1 >> 4) & 0x0FFC) / 4095.0;
    LPC_ADC->ADCR &= ~(1 << 1);      // Desactivamos el canal ya que solo lo necesitamos una vez
    LPC_ADC->ADCR &= ~(1 << 24);     // Paramos cualquier conversión
    speedInit = 0; 
    speedTrigger = 0;
  }
}
