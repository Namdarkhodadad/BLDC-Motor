/*
 * Skooter_avr_main.c
 *
 * Created: 07/22/2023 12:43:40 ب.ظ
 * Author: AVIDEH
 */
#define F_CPU             16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define SPEED_UP          PC0
#define SPEED_DOWN        PC1
#define PWM_MAX_DUTY      250
#define PWM_MIN_DUTY      15
#define PWM_START_DUTY    150




int bldc_step = 0;
int motor_speed;
int i;
int speed_step = 5;


void BEMF_A_RISING(){ 
  SFIOR = (0<<ACME);       // Select AIN1 as comparator negative input
  ACSR |= 0x03;            // Set interrupt on rising edge
}
void BEMF_A_FALLING(){
  SFIOR = (0<<ACME);       // Select AIN1 as comparator negative input
  ACSR &= ~0x01;           // Set interrupt on falling edge
}
void BEMF_B_RISING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module 
  SFIOR = (1<<ACME);      // Analog comparator multiplexer enable: AIN1 no longer considered as the negative input
  ADMUX = 2;              // Select analog channel 2 as comparator negative input
  ACSR |= 0x03;          // Analog comparator control and statues register
}
void BEMF_B_FALLING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  SFIOR = (1<<ACME);      // Analog comparator multiplexer enable: AIN1 no longer considered as the negative input
  ADMUX = 2;              // Select analog channel 2 as comparator negative input
  ACSR &= ~0x01;          // Analog comparator control and statues register 0<<ACIE
}
void BEMF_C_RISING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  SFIOR = (1<<ACME);      // Analog comparator multiplexer enable: AIN1 no longer considered as the negative input
  ADMUX = 3;              // Select analog channel 3 as comparator negative input
  ACSR |= 0x03;          // Analog comparator control and statues register
}
void BEMF_C_FALLING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  SFIOR = (1<<ACME);      // Analog comparator multiplexer enable: AIN1 no longer considered as the negative input
  ADMUX = 3;              // Select analog channel 3 as comparator negative input
  ACSR &= ~0x01;          // Analog comparator control and statues register 0<<ACIE
}

void AH_BL(){
  PORTB  =  0x04;         // Set PB2 = 1
  PORTD &= ~0x18;         // Set PD3 & PD4 = 0
  PORTD |=  0x20;         // Set PD5 = 1 
  TCCR2  =  (1<<COM21) | (0<<COM20) | (0<<WGM21) | (1<<WGM20);  
  TCCR1A =  (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (0 << WGM10);           // Turn pin 11 (OC2A) PWM ON (pin 9 & pin 10 OFF)
}
void AH_CL(){
  PORTB  =  0x02;         // Set PB1 = 1
  PORTD &= ~0x18;         // Set PD3 & PD4 = 0
  PORTD |=  0x20;         // Set PD5 = 1 
  TCCR2  =  (1<<COM21) | (0<<COM20) | (0<<WGM21) | (1<<WGM20);                                                    
  TCCR1A =  (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (0 << WGM10);          // Turn pin 11 (OC2A) PWM ON (pin 9 & pin 10 OFF)
}
void BH_CL(){
  PORTB  =  0x02;         // Set PB1 = 1
  PORTD &= ~0x28;         // Set PD3 & PD5 = 0
  PORTD |=  0x10;         // Set PD4 = 1 
  TCCR2  =   (0<<COM21) | (0<<COM20) | (0<<WGM21) | (0<<WGM20);                                                   // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
  TCCR1A =   (0 << COM1A1) | (0 << COM1A0) | (1 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (1 << WGM10);         //0x21
}
void BH_AL(){
  PORTB  =  0x08;         // Set PB3 = 1
  PORTD &= ~0x28;         // Set PD3 & PD5 = 0
  PORTD |=  0x10;         // Set PD4 = 1 
  TCCR2  =  (0<<COM21) | (0<<COM20) | (0<<WGM21) | (0<<WGM20);                                                    // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
  TCCR1A =  (0 << COM1A1) | (0 << COM1A0) | (1 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (1 << WGM10);          // 0x21
}
void CH_AL(){
  PORTB  =  0x08;         // Set PB3 = 1
  PORTD &= ~0x30;         // Set PD4 & PD5 = 0
  PORTD |=  0x08;         // Set PD3 = 1 
  TCCR2  =   (0<<COM21) | (0<<COM20) | (0<<WGM21) | (0<<WGM20);                                                    // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
  TCCR1A =   (1 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (1 << WGM10);          //0x81
}
void CH_BL(){
  PORTB  =  0x04;         // Set PB2 = 1
  PORTD &= ~0x30;         // Set PD4 & PD5 = 0
  PORTD |=  0x08;         // Set PD3 = 1 
  TCCR2 =  (0<<COM21) | (0<<COM20) | (0<<WGM21) | (0<<WGM20);                                                     // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
  TCCR1A =  (1 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (1 << WGM10);          //0x81
}

void SET_PWM_DUTY(int duty){
	// To not get out of range
  if(duty < PWM_MIN_DUTY)
    duty  = PWM_MIN_DUTY;
  if(duty > PWM_MAX_DUTY)
    duty  = PWM_MAX_DUTY;                                                
  OCR1A  = duty;                   // Set pin 9  PWM duty cycle
  OCR1B  = duty;                   // Set pin 10 PWM duty cycle
  OCR2  = duty;                   // Set pin 11 PWM duty cycle
  motor_speed = duty;
}
void bldc_move(){        // BLDC motor commutation function
  switch(bldc_step){
    case 0:
      AH_BL();
      BEMF_C_RISING();
      break;
    case 1:
      AH_CL();
      BEMF_B_FALLING();
      break;
    case 2:
      BH_CL();
      BEMF_A_RISING();
      break;
    case 3:
      BH_AL();
      BEMF_C_FALLING();
      break;
    case 4:
      CH_AL();
      BEMF_B_RISING();
      break;
    case 5:
      CH_BL();
      BEMF_A_FALLING();
      break;
  }
}
 ISR(ANA_COMP_vect)
 {
    // BEMF debounce
  for(i = 0; i < 10; i++) {
    if(bldc_step & 1){
      if(!(ACSR & 0x20)) i -= 1;
    }
    else {
      if((ACSR & 0x20))  i -= 1;
    }
  }
  bldc_move();
  bldc_step++;
  bldc_step %= 6;
 }


void main(void)
{
  DDRD  |= 0x38;           // Configure pins 3, 4 and 5 as outputs
  PORTD  = 0x00;
  DDRB  |= 0x0E;           // Configure pins 9, 10 and 11 as outputs
  PORTB  = 0x31;
  // Timer1 module setting: set clock source to clkI/O / 1 (no prescaling)
  TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (0 << WGM10);
  TCCR1B = 0x01;
  // Timer2 module setting: set clock source to clkI/O / 1 (no prescaling)  
  TCCR2 |=(0<<COM21) | (0<<COM20) | (0<<WGM21) | (0<<WGM20);
  TCCR2 |= (0<<CS22) | (0<<CS21) | (1<<CS20);
  // Analog comparator setting
  ACSR   = 0x10;           // Disable and clear (flag bit) analog comparator interrupt   
  ACSR |= (1<< ACIE);
  // configure pins C0 & C1 for push buttons (SPEED_UP & SPEED_DOWN)
  DDRC &= ~((1<<SPEED_UP) | (1<<SPEED_DOWN));
  PORTC |= (1<<SPEED_UP) | (1<<SPEED_DOWN);

  motor_speed = PWM_START_DUTY; 
  	int n;
  	int m;   
      sei();
while (1)
    {
    SET_PWM_DUTY(PWM_START_DUTY);    // Setup starting PWM with duty cycle = PWM_START_DUTY
	m = 22;
	n = 0;
  // Motor start: we started the motor off with a sequential loop changing its speed with
  //three different delay sequences.
  while(m > 15) {
    bldc_move();
    bldc_step++;
    bldc_step %= 6;
	if(m==22)
	{_delay_ms(22);}
	if(m==19)
	{_delay_ms(19);}
	if(m==16)
	{_delay_ms(16);}
	else
	{_delay_ms(15);}
    n++;
    if(n==30) {m = m-3;
               n = 0;}
    }
     ACSR |= 0x08;                    // Enable analog comparator interrupt for ISR(ANA_COMP_vect)
	 //This loop is our main loop after the motor start and works specifically with the ISR interrupt
	 //with the two push buttons(PC0 & PC1) sensed in every loop. we put the push button sensing in a while
	 //loop with 100ms delay for a smooth speed change.
  while(1) {
	  while(!(PINC & (1<<SPEED_UP)))
	  {
			if(motor_speed < PWM_MAX_DUTY){
				motor_speed  = motor_speed + speed_step;
				SET_PWM_DUTY(motor_speed);
				_delay_ms(100);}  
	  }
	  while(!(PINC & (1<<SPEED_DOWN)))
	  {
		  if(motor_speed > PWM_MIN_DUTY){
			  motor_speed = motor_speed - speed_step;
			  SET_PWM_DUTY(motor_speed);
			  _delay_ms(100);}
	  }
  }
}
}