//New version of the BLDC Motor project:
//Authors: Parsa Mashayekhi & Nommy Khodadad

#define F_CPU             8000000UL
//include the necessary ATmega8a , interrupt , and delay libraries:
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

//defining the used pins:
#define SPEED_UP          PC0
#define SPEED_DOWN        PC1
#define Phase_A_LM        PC2
#define Phase_B_LM        PC3
#define Phase_C_LM        PC4
#define LED               PD0

//All our variables declared here:
int Case_Number;
int Prev_Case_Number;
int Start_Sequence = 1;
int Index;
int End = 16;
int Main;
int Speed;
int Speed_Step = 1;
int Max_Speed = 250;
int Min_Speed = 35;
int Timer_Flag;
int Timer_Counter;
int Timer1_Counter;
int Debounce_Counter;
int Button_Debounce_limit = 5000;
int Change;
int R_Prev;
int Stop_Counter;
int Reset = 30;
int t1;
int t2;
int Start_Duty = 127;
int Reverse;
int Amount;

//Our Function declarations:
void PWM_Duty(int duty);
int  Motor_Move(int Case_Number);
void Delay_ms(int Delay);
void Stopped(int Prev_Case_Number , int Case_Number);
void Pin_Change(int Case_Number);
void Motor_Stop();
void Motor_Brake();
void Timer1_Delay_ms(int Amount);

//Declaring the time ISR function:
ISR(TIMER0_OVF_vect)
{
	//Even with the maximum amount of preScaling (1/1024), Timer0 has an 8 bit counter which
	//brings the maximum overflow to 32ms. Therefore we introduce another timer counter int
	//to increase the delay. 32*25 = 800ms 
	Timer_Counter++;
	TCNT0 = 0;
	if(Timer_Counter > 25)
	{
		//Triggers Timer flag:
		Timer_Counter = 0;
		Timer_Flag = 1;
		Change = 1;
	}
}
ISR(TIMER1_OVF_vect)
{
	Timer1_Counter++;
	TCNT1 = 57344;
	if(Timer_Counter > Amount)
	{
		Timer1_Counter = 0;
		Amount --;
	}
}

int main(void)
{
	//Input/Output declaration:
	//Speed up and Speed down pins & Phases A , B , C LM339 outputs:
	DDRC &= ~((1<<SPEED_UP) | (1<<SPEED_DOWN) | (1<<Phase_A_LM) | (1<<Phase_B_LM) | (1<<Phase_C_LM));
	PORTC |= (1<<SPEED_UP) | (1<<SPEED_DOWN) | (1<<Phase_A_LM) | (1<<Phase_B_LM) | (1<<Phase_C_LM);
	//PWM IR2103 High pins: (pins 9 , 10 , 11)
	DDRB  |= 0x0E;
	PORTB  = 0x00;
	//IR2103 Low pins: (pins 5 , 6 , 7)
	DDRD  |= 0xFD;
	PORTD  = 0x00;
	//Timer 0 interrupt with 1024 preScaling:
	TCCR0 = (1<<CS02) | (0<<CS01) | (1<<CS00);
	//Timer 1 no preScaling (For Timer1 Delay Function):
	TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (0 << WGM10);
	TCCR1B = 0x01;
	//Timer 2 no preScaling (For PWM generation):
	TCCR2 |= (1<<COM21) | (0<<COM20) | (0<<WGM21) | (1<<WGM20) | (0<<CS22) | (0<<CS21) | (1<<CS20);
	//Setting a default value for Reverse:
	Reverse = 0;
	while (1)
	{
		//Start cycle:
		//In the Start cycle, Main = 0 and in the Main cycle, Main = 1.
		//We also set default values for the duty cycle (Start_Duty = 127 = 50% of the full speed).
		//We set all interrupt flags zero for now and we reset our timer values for the first cycle. 
		Main = 0;
		PWM_Duty(Start_Duty);
		Speed = Start_Duty;
		TIMSK |= (0<<TOIE0);
		Timer_Counter = 0;
		TCNT0 = 0;
		Case_Number = 0;
		Index = 100;
		Debounce_Counter = 0;
		t2 = 0;
		sei();
		while(Index > End)
		{
			Index -= Start_Sequence;
			//t2 and t1 are the current and next case numbers. 
			t1 = Motor_Move(t2);
			t2 = t1;
			Delay_ms(Index);
			//Timer1_Delay_ms(Index);
			//Gets out of the Start loop the moment it senses BEMF. 
			if (!(PINC & (1<<Phase_A_LM)) || !(PINC & (1<<Phase_B_LM)) || !(PINC & (1<<Phase_A_LM)))
			{
				break;
			}
			//Move for a specific amount of time in a fixed speed then Restart.
			if(Index == End)
			{
				for(int j=0; j<150 ; j++)
				{
					t1 = Motor_Move(t2);
					t2 = t1;
					Delay_ms(Index);
					//Timer1_Delay_ms(Index);
				}
				Index = 100;
			}
		}
		//Entering the Main loop:
		Main = 1;
		//Timer 0 interrupt with 1024 preScaling (for Timer0 overflow ISR):
		TCCR0 = (1<<CS02) | (0<<CS01) | (1<<CS00);
		//Enabling Timer0 overflow interrupt
		
		TCNT0 = 0;
		Timer_Counter = 0;
		TIMSK |= (1<<TOIE0);
		Timer_Flag = 0;
		//Main cycle:
		while(Main)
		{
			//This function determines the current step based on the previous BEMF inputs:
			Pin_Change(t2);
			//Only gets out of this loop if Timer_flag is activated (No motor movement for about 0.5s) 
			if(Timer_Flag)
			{
				Main = 0;
				Timer_Flag = 0;
			}
		}
	}
}

void Pin_Change(int Case_Number)
{
	//In this function, based on the mode of the motor movement (clock-wise &
	//counter clock-wise) we have 2 main conditions: Reverse = 0 and Reverse = 1.
	//The Change variable is to notice when the micro-controller senses a new case.
	//Typically we set its default value to zero.
	//When ever we sensed a change in BEMF  ==> Change = 1. 
	Change = 0;
	if(Reverse)
	{
		while(!Change)
		{
			switch (Case_Number)
			{
				case 0:
				if((PINC & (1<<Phase_A_LM)))
				{   //if A = 0
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 1:
				if(!(PINC & (1<<Phase_C_LM)))
				{	//if C = 1
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 2:
				if((PINC & (1<<Phase_B_LM)))
				{   //if B = 0
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 3:
				if(!(PINC & (1<<Phase_A_LM)))
				{	//if A = 1
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 4:
				if((PINC & (1<<Phase_C_LM)))
				{	//if C = 0
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 5:
				if(!(PINC & (1<<Phase_B_LM)))
				{	//if B = 1
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
			}
			//////////////Speed up & Speed down button change:(Or Reverse call)
			//Pushing one button will result in either speed up or speed down;
			//Pushing both buttons will result in reverse direction.
			if(!(PINC & (1<<SPEED_UP)))
			{
				if(Speed < Max_Speed)
				{
					Debounce_Counter ++;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
					//Change speed after DeBounce:
					if(Debounce_Counter > Button_Debounce_limit)
					{
						Speed  = Speed + Speed_Step;
						PWM_Duty(Speed);
						Debounce_Counter = 0;
					}
				}
				if(!(PINC & (1<<SPEED_DOWN)))
				{
					//If both buttons were pushed ==> Reverse direction
					//Exit Main loop:
					Main = 0;
					//Reverse direction:
					Reverse = 0;
					//Exit Pin change:
					Change = 1;
					Motor_Brake();
					return;
				}
			}
			if(!(PINC & (1<<SPEED_DOWN)))
			{
				if(Speed > Min_Speed)
				{
					Debounce_Counter ++;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
					//Change speed after DeBounce:
					if(Debounce_Counter > Button_Debounce_limit)
					{
						Speed  = Speed - Speed_Step;
						PWM_Duty(Speed);
						Debounce_Counter = 0;
					}
				}
				if(!(PINC & (1<<SPEED_UP)))
				{
					//If both buttons were pushed ==> Reverse direction
					//Exit Main loop:
					Main = 0;
					//Reverse direction:
					Reverse = 0;
					//Exit Pin change:
					Change = 1;
					Motor_Brake();
					return;
				}
			}
		}
		return;
	}
	if(!Reverse)
	{
		while(!Change)
		{
			switch (Case_Number)
			{
				case 0:
				if(!(PINC & (1<<Phase_B_LM)))
				{	//if B = 1
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 1:
				if((PINC & (1<<Phase_A_LM)))
				{	//if A = 0
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 2:
				if(!(PINC & (1<<Phase_C_LM)))
				{	//if C = 1
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 3:
				if((PINC & (1<<Phase_B_LM)))
				{	//if B = 0
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 4:
				if(!(PINC & (1<<Phase_A_LM)))
				{	//if A = 1
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
				case 5:
				if((PINC & (1<<Phase_C_LM)))
				{	//if C = 0
					t1 = Motor_Move(t2);
					t2 = t1;
					Change = 1;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
				}
				break;
			}
			//////////////Speed up & Speed down button change:(Or Reverse call)
			//Pushing one button will result in either speed up or speed down;
			//Pushing both buttons will result in reverse direction.
			if(!(PINC & (1<<SPEED_UP)))
			{
				if(Speed < Max_Speed)
				{
					Debounce_Counter ++;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
					//Change speed after DeBounce:
					if(Debounce_Counter > Button_Debounce_limit)
					{
						Speed  = Speed + Speed_Step;
						PWM_Duty(Speed);
						Debounce_Counter = 0;
					}
				}
				if(!(PINC & (1<<SPEED_DOWN)))
				{
					//If both buttons were pushed ==> Reverse direction
					//Exit Main loop: 
					Main = 0;
					//Reverse direction:
					Reverse = 1;
					//Exit Pin change:
					Change = 1;
					Motor_Brake();
					return;
				}
			}
			if(!(PINC & (1<<SPEED_DOWN)))
			{
				if(Speed > Min_Speed)
				{
					Debounce_Counter ++;
					//Motor has'nt stopped ==> Timer reset:
					Timer_Counter = 0;
					TCNT0 = 0;
					//Change speed after DeBounce:
					if(Debounce_Counter > Button_Debounce_limit)
					{
						Speed  = Speed - Speed_Step;
						PWM_Duty(Speed);
						Debounce_Counter = 0;
					}
				}
				if(!(PINC & (1<<SPEED_UP)))
				{
					//If both buttons were pushed ==> Reverse direction
					//Exit Main loop:
					Main = 0;
					//Reverse direction:
					Reverse = 1;
					//Exit Pin change:
					Change = 1;
					Motor_Brake();
					return;
				}
			}
		}
	}
}

//Based on the reverse status and the table in our report, we wrote the cases accordingly:
int Motor_Move(int Case_Number)
{
	if(Reverse)
	{
		switch(Case_Number)
		{
			case 0:
			PORTD &= ~0xAF;
			PORTD |=  0x50;
			return 1;
			break;
			case 1:
			PORTD &= ~0xCF;
			PORTD |=  0x30;
			return 2;
			break;
			case 2:
			PORTD &= ~0xD7;
			PORTD |=  0x28;
			return 3;
			break;
			case 3:
			PORTD &= ~0x77;
			PORTD |=  0x88;
			return 4;
			break;
			case 4:
			PORTD &= ~0x7B;
			PORTD |=  0x84;
			return 5;
			break;
			case 5:
			PORTD &= ~0xBB;
			PORTD |=  0x44;
			return 0;
			break;
			default:
			Main = 0;
			break;
		}
	}
	if(!Reverse)
	{
		switch(Case_Number)
		{
			case 0:
			PORTD &= ~0xAF;
			PORTD |=  0x50;
			return 5;
			break;
			case 1:
			PORTD &= ~0xCF;
			PORTD |=  0x30;
			return 0;
			break;
			case 2:
			PORTD &= ~0xD7;
			PORTD |=  0x28;
			return 1;
			break;
			case 3:
			PORTD &= ~0x77;
			PORTD |=  0x88;
			return 2;
			break;
			case 4:
			PORTD &= ~0x7B;
			PORTD |=  0x84;
			return 3;
			break;
			case 5:
			PORTD &= ~0xBB;
			PORTD |=  0x44;
			return 4;
			break;
			default:
			Main = 0;
			break;
		}
	}
}

void PWM_Duty(int duty)
{
	if(duty == Max_Speed)
	{
		duty = Max_Speed - 1;
	}
	if(duty == Min_Speed)
	{
		duty = Min_Speed + 1;
	}
	//Timer declaration:
	OCR2   = duty;  // set pin 11 PWM duty cycle
	//Timer 2 no preScaling (For PWM generation):
	TCCR2 |= (1<<COM21) | (0<<COM20) | (0<<WGM21) | (1<<WGM20) | (0<<CS22) | (0<<CS21) | (1<<CS20);
}

void Delay_ms(int Delay)
{
	for(int i=0; i < Delay; i++)
	{
		_delay_ms(1);
	}
}

void Timer1_Delay_ms(int Amount)
{
	TIMSK |= (1<<TOIE1);
	//Timer 1 no preScaling (For Timer1 Delay Function):
	TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (0 << WGM10);
	TCCR1B = 0x01;
	while(Amount > 0)
	{
		if(Amount == 1)
		{
			TIMSK |= (0<<TOIE1);
			Timer1_Counter = 0;
			TCNT1 = 57344;
			return;
		}
	}
}

void Motor_Stop()
{
	PORTD &= ~0xFF;
}

void Motor_Brake()
{
	for(int Brake_Index=0 ; Brake_Index > 300 ; Brake_Index ++)
	{
		int Timer1_Input = 20;
		PORTD &= ~0x1F;
		PORTD |=  0xE0;	
		Timer1_Delay_ms(Timer1_Input);
	}
}