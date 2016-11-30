#include <avr/io.h>
#include "scheduler.h"
#include "usart_ATmega1284.h"
#include "lcd.h"

unsigned char data = 0x00;
unsigned char prevdata = 0x00;
unsigned char islocked, attempts, currlock, lockout, resetting, go;
unsigned char text[] = "currlock:  /3   attempts:  /3";
unsigned char text2[] = "Too many failed attempts.";
unsigned char text3[] = "Lock Status:    Unlocked";
unsigned char text4[] = "Resetting       Lock  /3";

enum LCD_Display{LCDInit, LCDWait, LCDLocked, LCDUnlocked, LCDLockout, LCDReset};
enum TransmitData{TranInit, TranWait, TranData};
enum ReceiveData{RecInit, RecWait, RecData};

int LCDDisplayTick(int lcdstate);
int TransmitTick(int tranState);
int ReceiveTick(int recState);

int main(void)
{
	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	
	tasksNum = 3;
	task tsk[3];
	tasks = tsk;
	
	unsigned char i = 0;
	tasks[i].state = LCDInit;
	tasks[i].period = 100;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &LCDDisplayTick;
	++i;
	tasks[i].state = RecInit;
	tasks[i].period = 50;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &ReceiveTick;
	++i;
	tasks[i].state = TranInit;
	tasks[i].period = 1000;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TransmitTick;
	
	// intitialize USART
	initUSART(1);
	
	// initialize LCD
	LCD_init();
	LCD_ClearScreen();
	
	TimerSet(50);
	TimerOn();
	
	while(1){
		// Timer ISR
	}
}

int LCDDisplayTick(int lcdstate){
	// state transitions
	switch(lcdstate){
		case LCDInit:
			lcdstate = LCDWait;
			break;
		
		case LCDWait:
			if(go && islocked && !lockout && !resetting ){
				go = 0;
				lcdstate = LCDLocked;
			}
			else if(go && !islocked && !lockout && !resetting){
				go = 0;
				lcdstate = LCDUnlocked;
			}
			else if(go && resetting && !lockout){
				go = 0;
				lcdstate = LCDReset;
			}
			else if(go && lockout){
				go = 0;
				lcdstate = LCDLockout;
			}
			else{
				lcdstate = LCDWait;
			}
			break;
		
		case LCDLocked:
			lcdstate = LCDWait;
			break;
			
		case  LCDUnlocked:
			lcdstate = LCDWait;
			break;
			
		case LCDLockout:
			lcdstate = LCDWait;
			break;
			
		case LCDReset:
			lcdstate = LCDWait;
			break;
		
		default:
			lcdstate = LCDInit;
			break;
	}
	// state actions
	switch(lcdstate){
		case LCDInit:
			break;
			
		case LCDWait:
			break;
		
		case LCDLocked:
			LCD_ClearScreen();
			LCD_DisplayString(1, text);
			LCD_Cursor(11);
			LCD_WriteData(currlock + '0');
			LCD_Cursor(27);
			LCD_WriteData(attempts + '0');
			LCD_Cursor(32);
			break;
		
		case LCDUnlocked:
			LCD_ClearScreen();
			LCD_DisplayString(1, text3);
			LCD_Cursor(32);
			break;
		
		case LCDLockout:
			LCD_ClearScreen();
			LCD_DisplayString(1, text2);
			LCD_Cursor(32);
			break;
			
		case LCDReset:
			LCD_ClearScreen();
			LCD_DisplayString(1, text4);
			LCD_Cursor(22);
			LCD_WriteData(resetting + '0');
			LCD_Cursor(32);
			break;
		
		default:
			break;
	}
	PORTB = resetting;
	return lcdstate;
}

int ReceiveTick(int recState){
	// state transitions
	switch(recState){
		case RecInit:
			recState = RecWait;
			break;
		
		case RecWait:
			if( USART_HasReceived(1) ){
				recState = RecData;
			}
			else{
				recState = RecWait;
			}
			break;
		
		case RecData:
			USART_Flush(1);
			recState = RecWait;
			break;
		
		default:
			recState = RecInit;
			break;
	}
	// state actions
	switch(recState){
		case RecInit:
			USART_Flush(1);
			break;
		
		case RecWait:
			break;
		
		case RecData:
			data = USART_Receive(1);
			islocked = data & 0x03;
			currlock = ( (data & 0x0C) >> 2) + 1;
			attempts = ( (data & 0x30) >> 4) ;
			lockout = (data & 0x02) >> 1;
			resetting = ((data & 0xC0) >> 6);
			if(data != prevdata){
				go = 1;
				prevdata = data;
			}
			break;
	}
	return recState;
}

int TransmitTick(int tranState){
	// state transitions
	switch(tranState){
		case TranInit:
		tranState = TranWait;
		break;
		
		case TranWait:
		if(USART_IsSendReady(1)){
			tranState = TranData;
		}
		else{
			tranState = TranWait;
		}
		break;
		
		case TranData:
		if( !(USART_HasTransmitted(1)) ){
			tranState = TranData;
		}
		else{
			tranState = TranWait;
		}
		break;
		
		default:
		tranState = TranInit;
		break;
	}
	// state actions
	switch(tranState){
		case TranInit:
		break;
		
		case TranWait:
		break;
		
		case TranData:
		USART_Send(0x48, 1);
		break;
		
		default:
		break;
	}
	return tranState;
}

