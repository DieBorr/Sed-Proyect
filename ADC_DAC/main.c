#include <LPC17xx.h>
#include <math.h>
#include "../PWM/reg_masks.h"
#include "../PWM/PWM.c"

// Constantes
#define F_cpu         100e6                        // Frecuencia del sistema (xtal = 12 MHz)
//#define F_pclk        F_cpu / 4                   // PCLK configurado por defecto
#define voltageDiv    2.568
#define Vref          3.3
#define M_PI          3.14159265358979323846f
#define N_POINTS      16
#define DAC_N_BITS    10
#define DAC_N_LEVELS (1U << DAC_N_BITS)
#define DAC_MID_RANGE (1U << (DAC_N_BITS-1))

static uint16_t sample_table[N_POINTS];
static uint8_t  sample_idx;

#define F_cpu 100e6          // Defecto Keil (xtal=12Mhz)
#define F_pclk F_cpu/4       // PCLK configurado por defecto

#define PIN_DIR_WHEEL_R 0x01 // P0.0 => direccion para rueda derecha
#define PIN_DIR_WHEEL_L 0x02 // P0.1 => direccion para rueda izquierda
#define WHEEL_R         0
#define WHEEL_L         1

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

  for(i = 0; i < N_POINTS; i++) {
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
  LPC_TIM2->TCR  =   (1 << 1);                    // Reset Timer2
  LPC_TIM2->TCR &=  ~(1 << 1);                    // Limpiamos ese uno
  LPC_TIM2->MR0  =   ( F_pclk / freq_hz );
  LPC_TIM2->TCR  =   (1 << 0);                    // Iniciar Timer2
}

void TIMER2_IRQHandler()
{
  LPC_TIM2->IR |= (1 << 0); // Clear int flag
  alarm_gen_sample();

}

// Variables globales
volatile float speed;
volatile float batValue;
volatile uint8_t speedValidation = 0;
volatile uint8_t speedTimerTrigger = 0;
volatile uint8_t batValidation = 0;
static uint8_t batteryInit = 0;
static uint8_t speedInit = 0;
volatile uint8_t batteryTrigger = 0;
volatile uint8_t speedTrigger = 0;
volatile uint8_t speedIsvalid = 0;
float* speedPtr;
volatile float ADvoltage;
volatile int prueba;
volatile int test;

// Funciones
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
                 | (1 << 8)                     // CLKDIV = 2
                 | (1 << 0)                     // Seleccionar el canal AD0.0
                 | (1 << 21);                   // Habilitar ADC

  LPC_ADC->ADINTEN |= (3 << 0);                 // AD0.0 y AD0.1 interrupción
  
  // Configuración de los timers
  LPC_SC->PCONP |= (1 << 1);                   // Encender Timer0
  LPC_TIM0->PR = 0;                            // Sin prescaler
  LPC_TIM0->MCR |= (1 << 4) | (1 << 3);         // Reset en MR0.1 e interrupción
  LPC_TIM0->EMR |= (2 << 6);                   // Set en Mat0.1

  LPC_SC->PCONP |= (1 << 2);                   // Encender Timer1
  LPC_TIM1->PR = 0;                            // Sin prescaler
  LPC_TIM1->MCR |= (7 << 0);         // Reset en MR0 e interrupción
  //LPC_TIM1->EMR |= (2 << 4);                   // Set en Mat1.0

  NVIC_EnableIRQ(TIMER0_IRQn);                 // Habilitar IRQ de Timer0
  NVIC_EnableIRQ(TIMER1_IRQn);                 // Habilitar IRQ de Timer1
  NVIC_SetPriority(ADC_IRQn,0);
  NVIC_EnableIRQ(ADC_IRQn);                    // Habilitar IRQ del ADC
}

void battery_sampling_init(float Ts)
{
  if (!batteryInit)
  {
    LPC_TIM0->MR1 = (Ts * F_pclk) - 1;           // Configurar tiempo Ts
    LPC_TIM0->EMR &= ~(3 << 6);                  // Clear del registro
    LPC_TIM0->EMR |=  (2 << 6);                   // Set en Mat0.1
    LPC_TIM0->TCR |=  (1 << 1);                    // Reset Timer0
    LPC_TIM0->TCR &= ~(1 << 1);               
    LPC_TIM0->TCR |=  (1 << 0);                    // Iniciar Timer0

    batteryInit = 1;                             // Marcar inicialización
    batteryTrigger = 0;
  }
}

void TIMER0_IRQHandler() {
  if(!speedTrigger) {
  LPC_TIM0->IR |= (1 << 1);  // Borrar el flag de interrupción del Match 0.1
  batteryTrigger = 1;
  LPC_TIM0->EMR &= ~(1 << 1);   // Mat0.1 a 0
  
  // Configuración del canal AD0.0 (batería)
  LPC_ADC->ADCR &= ~(0xFF);    // Limpiar cualquier canal previamente habilitado
  LPC_ADC->ADCR |= (1 << 0);   // Habilitar el canal AD0.0 para conversión de batería
  LPC_ADC->ADCR |= (1 << 24);  // Iniciar conversión
  }
}

void speed_get_value(float Ts)
{
  prueba++;
  if (!speedInit)
  {
    prueba = 9;
    LPC_TIM1->TCR |= (1 << 0);                    // Iniciar Timer1
    
    LPC_TIM1->MR0 = (Ts * F_pclk) - 1;           // Configurar tiempo Ts
        prueba = 15;
    

    speedInit = 1;                               // Marcar inicialización
    speedIsvalid = 0;
    prueba = 2;
    speedTimerTrigger = 1;
    
  }
}

void TIMER1_IRQHandler() {
  if(speedTimerTrigger)
  {
    LPC_TIM1->IR  |=  (1 << 0);   // Borrar el flag de interrupción del Match 1.0
    //LPC_TIM1->TCR &= ~(1 << 0);   // Paramos el timer, ya esta reseteado de antes
    speedTrigger = 1;
    // Configuración del canal AD0.1 (velocidad)
    LPC_ADC->ADCR &= ~(0xFF << 0);       // Limpiar cualquier canal previamente habilitado
    LPC_ADC->ADCR |=  (1 << 1);          // Habilitar el canal AD0.1 para conversión de velocidad
    LPC_ADC->ADCR |=  (1 << 24);
    speedTrigger = 1;
    speedTimerTrigger = 0;
  }
}

void ADC_IRQHandler(void)
{
  LPC_ADC->ADCR &= ~(1 << 24);
  // Verificar conversión de AD0.0 (batería)
  if (LPC_ADC->ADDR0 & (1 << 31) && batteryTrigger && !((LPC_ADC->ADGDR >> 24) & (0x7)) ) // Sabemos que se ha ejecutado esta conversión gracias al Mat0.1
  {
    prueba = ((LPC_ADC->ADDR0 >> 4) & 0xFFF);
    ADvoltage = ((LPC_ADC->ADDR0 >> 4) & 0xFFC);
    batValue =  ADvoltage * (( Vref * voltageDiv ) / 4095.0 ) ; // Leer valor ADC
    
    batValidation = 1;                         // Señalar nueva medición
    batteryTrigger = 0;
    batteryInit++;
    test = 99;
  }

  // Verificar conversión de AD0.1 (velocidad)
  if (LPC_ADC->ADDR1 & (1 << 31) && speedInit)
  { 
    speed = ((LPC_ADC->ADDR1 >> 4) & 0x0FFC) / 4095.0;
    speedValidation = 1;             // Señalar medición correcta
    LPC_ADC->ADCR &= ~(1 << 1);      // Desactivamos el canal ya que solo lo necesitamos una vez
    LPC_ADC->ADCR &= ~(1 << 24);     // Paramos cualquier conversión
    speedInit = 0; 
    speedTrigger = 0;
    speedIsvalid = 1;  
  }
}

int main()
{
  /*
  ADC_init();  // Inicializar ADC
  battery_sampling_init(2.5);
  speed_get_value(1);  // Inicializar muestreo de velocidad (3 Hz)

  while (1)
  {
    speed_get_value(1);
    if (speedValidation)
    {
      speedValidation = 0; // Limpiar flag de validación de velocidad
    }
    
    if (batValidation)
    {
      batValidation = 0; // Limpiar flag de validación de batería
    }
  }
  */
  batValue = 5 ;
  ADC_init();
  pwm_config(1/1e3);
  //alarm_init();
  //alarm_set_freq(10000);
  battery_sampling_init(1.5);
  
  while( 1)
  {
    speed_get_value(1);
    pwm_set_duty_cycle(-speed, speed);
  }
}