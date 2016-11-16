#include <avr/io.h>
#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"
#include "usart_ATmega1284.h"

// global variables
unsigned char button = 0;
unsigned char lockdir = 0; // 0: counterclockwise 1: clockwise
unsigned char go = 0; // notifies stepper motor to turn the lock
unsigned char row = 0xFF; // rows to display on led matrix
unsigned char red = 0xFF; // controls the red leds of the matrix
unsigned char green = 0xFF; // controls the green leds of the matrix
unsigned char blue = 0xFF; // controls the blue leds of the blue matrix
unsigned char data; // data received from app

// Smart Lock main control state machine
enum smartlock{lockInit, unlocked, locked}lockstate;
void smartlockInit();
void smartlockTick();
void smartlockTask();

// Stepper motor SM to control the turning of the lock
enum StepperMotor{stepperInit, stepperWait, A, AB, B, BC, C, CD, D, DA }stepperState;
void stepInit();
void stepTick();
void stepTask();

// LED Matrix SM to control which LEDs are displayed
enum LEDMatrix{ledInit, leddisplay}ledstate;
void LEDMatrixInit();
void LEDMatrixTick();
void LEDMatrixTask();

// USART Transmit SM
enum TransmitData{TranInit, TranWait, TranData} tranState;
void TransmitInit();
void TransmitTick();
void TransmitTask();

// USART Receive SM
enum ReceiveData{RecInit, RecWait, RecData}recState;
void ReceiveInit();
void ReceiveTick();
void ReceiveTask();

// Button SM
enum Button{BInit, buttonWait, buttonbuffer, buttonpress}buttonstate;
void ButtonInit();
void ButtonTick();
void ButtonTask();

// shift register function
void MatrixTransmit(unsigned char rowdata, unsigned char reddata, unsigned char greendata, unsigned char bluedata);

void StartSecPulse(unsigned portBASE_TYPE Priority){
	xTaskCreate(smartlockTask, (signed portCHAR *)"smartlockSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(stepTask, (signed portCHAR *)"stepSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(LEDMatrixTask, (signed portCHAR *)"LEDMatrixSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(TransmitTask, (signed portCHAR *)"TransmitSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
   	xTaskCreate(ReceiveTask, (signed portCHAR *)"ReceiveSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(ButtonTask, (signed portCHAR *)"ButtonSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(){
	// initialize ports to input or output
	DDRA = 0xFF; PORTA = 0x00; // A0-A4 stepper motor
	DDRB = 0xFF; PORTB = 0x00; // led matrix
	DDRC = 0x00; PORTC = 0xFF; // C0 button
	
	// intitialize USART
	initUSART(0);

	StartSecPulse(1);
	vTaskStartScheduler();

	return 0;
}

void smartlockInit(){
	lockstate = lockInit;
}

void smartlockTick(){
	// state transitions
	switch(lockstate){
		case lockInit:
			lockstate = unlocked;
			break;
	
		case unlocked:
			if(button){
				go = 1;
				lockdir = 0;
				button = 0;
				lockstate = locked;
			}
			else{
				lockstate = unlocked;
			}
			break;
	
		case locked: 
			if(button){
				lockdir = 1;
				go = 1;
				button = 0;
				lockstate = unlocked;
			}
			else{
				lockstate = locked;
			}
			break;
	
		default:
			lockstate = lockInit;
			break;
	}
	// state actions
	switch(lockstate){
		case lockInit:
			break;
		
		case unlocked:
			red = 0x00;
			blue = 0x00;
			green = 0xFF;
			break;
		
		case locked:
			red = 0xFF;
			blue = 0x00;
			green = 0x00;
			break;
		
		default:
			break;
	}
}

void smartlockTask(){
	smartlockInit();
	for(;;){
		smartlockTick();
		vTaskDelay(50);
	}
}

void ButtonInit(){
	buttonstate = BInit;
}

void ButtonTick(){
	static unsigned char count;
	unsigned char input = ~PINC & 0x01;
	// state transitions
	switch(buttonstate){
		case BInit:
			count = 0;
			buttonstate = buttonWait;
			break;
		
		case buttonWait:
			if(input){
				button = 1;
				buttonstate = buttonbuffer;
			}
			else{
				buttonstate = buttonWait;
			}
			break;
		
		case buttonbuffer:
			if(count >= 31){ // must wait for lock to finish turning
				buttonstate = buttonpress;
				count = 0;
			}
			else{
				buttonstate = buttonbuffer;
			}
			break;
		
		case buttonpress:
			if(!input){
				buttonstate = buttonWait;
			}
			else{
				buttonstate = buttonpress;
			}
			break;
		
		default:
			buttonstate = BInit;
			break;
	}
	// state actions
	switch(buttonstate){
		case BInit:
			break;
		
		case buttonWait:
			break;
				
		case buttonbuffer:
			count++;
			break;
		
		case buttonpress:
			break;
		
		default:
			break;
	}
}

void ButtonTask(){
	ButtonInit();
	for(;;){
		ButtonTick();
		vTaskDelay(100);
	}
}

void stepInit(){
	stepperState = stepperInit;
}

void stepTick(){
	static int numPhases; // num of motor phases to lock/unlock
	static unsigned char output;
	// state transitions
	switch(stepperState){
		case stepperInit:
			stepperState = stepperWait;
			break;

		case stepperWait:
			if(go){
				go = 0;
				numPhases = (90 / 5.625) * 64;
				stepperState = A;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case A:
			if(numPhases && lockdir){
				stepperState = AB;				
			}
			else if(numPhases && !lockdir){
				stepperState = DA;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case AB:
			if(numPhases && lockdir){
				stepperState = B;				
			}
			else if(numPhases && !lockdir){
				stepperState = A;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case B:
			if(numPhases && lockdir){
				stepperState = BC;				
			}
			else if(numPhases && !lockdir){
				stepperState = AB;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case BC:
			if(numPhases && lockdir){
				stepperState = C;
			}
			else if(numPhases && !lockdir){
				stepperState = B;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case C:
			if(numPhases && lockdir){
				stepperState = CD;
			}
			else if(numPhases && !lockdir){
				stepperState = BC;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case CD:
			if(numPhases && lockdir){
				stepperState = D;
			}
			else if(numPhases && !lockdir){
				stepperState = C;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case D:
			if(numPhases && lockdir){
				stepperState = DA;
			}
			else if(numPhases && !lockdir){
				stepperState = CD;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		case DA:
			if(numPhases && lockdir){
				stepperState = A;
			}
			else if(numPhases && !lockdir){
				stepperState = D;
			}
			else{
				stepperState = stepperWait;
			}
			break;

		default:
			stepperState = stepperInit;
			break;
	}

	// state actions 
	switch(stepperState){
		case stepperInit:
			break;

		case stepperWait:
			break;

		case A:
			numPhases--;
			output = 0x01;
			break;

		case AB:
			numPhases--;
			output = 0x03;
			break;

		case B:
			numPhases--;
			output = 0x02;
			break;

		case BC:
			numPhases--;
			output = 0x06;
			break;

		case C: 
			numPhases--;
			output = 0x04;
			break;

		case CD:
			numPhases--;
			output = 0x0C;
			break;

		case D:
			numPhases--;
			output = 0x08;
			break;

		case DA:
			numPhases--;
			output = 0x09;
			break;

		default:
			break;
	}
	PORTA = output;
}

void stepTask(){
	stepInit();
	for(;;){
		stepTick();
		vTaskDelay(3);
	}
}

void LEDMatrixInit(){
	ledstate = ledInit;
}

void LEDMatrixTick(){
	// state transitions
	switch(ledstate){
		case ledInit:
			ledstate = leddisplay;
			break;
		
		case leddisplay:
			ledstate = leddisplay;
			break;
		
		default:
			ledstate = ledInit;
			break;
	}	
	
	// state actions
	switch(ledstate){
		case ledInit:
			break;
		
		case leddisplay:
			MatrixTransmit(row, ~red, ~green, ~blue);
			//rowTransmit(row);
			//TransmitRed(red);
			//TransmitGreen(green);
			//TransmitBlue(blue);
			break;
		
		default:
			break;
	}
}	

void LEDMatrixTask(){
	LEDMatrixInit();
	for(;;){
		LEDMatrixTick();
		vTaskDelay(1);
	}
}

void ReceiveInit(){
    recState = RecInit;
}

void ReceiveTick(){
    // state transitions
    switch(recState){
        case RecInit:
            recState = RecWait;
            break;
           
        case RecWait:
            if( USART_HasReceived(0) ){
                recState = RecData;
            }
            else{
                recState = RecWait;
            }
            break;
           
        case RecData:
            USART_Flush(0);
            recState = RecWait;
            break;
            
        default:
            recState = RecInit;
            break;
    }
    // state actions
    switch(recState){
        case RecInit:
            USART_Flush(0);
            break;
           
        case RecWait:
            break;
           
        case RecData:
            data = USART_Receive(0);
            break;
    }
}

void ReceiveTask(){
    ReceiveInit();
    for(;;){
        ReceiveTick();
        vTaskDelay(50);
    }
}

void TransmitInit(){
	tranState = TranInit;
}

void TransmitTick(){
	// state transitions
	switch(tranState){
		case TranInit:
			tranState = TranWait;
			break;
		
		case TranWait:
			if(USART_IsSendReady(0)){
				tranState = TranData;
			}
			else{
				tranState = TranWait;
			}
			break;
		
		case TranData:
			if( !(USART_HasTransmitted(0)) ){
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
			USART_Send(0xFF, 0);
			break;
		
		default:
			break;
	}
}

void TransmitTask(){
	TransmitInit();
	for(;;){
		TransmitTick();
		vTaskDelay(1000);
	}
}

// send led matrix data to shift registers
void MatrixTransmit(unsigned char rowdata, unsigned char reddata, unsigned char greendata, unsigned char bluedata) {
	unsigned char rojo, verde, azul, temprow;
	// for each bit of data
	for(int i = 7; i >=0; --i){
		// Set SRCLR to 1 allowing data to be set
		// Also clear SRCLK in preparation of sending data
		PORTB = 0x28;
		// set SER = next bit of data to be sent.
		rojo = (((reddata >> i) & 0x01) << 6); // ser location of red led
		verde = ((greendata >> i) & 0x01); // ser location of green led
		azul = (((bluedata >> i) & 0x01) << 7); // ser location of blue led
		temprow = (((rowdata >> i) & 0x01) << 4);
		PORTB |= (temprow | rojo | verde | azul);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		// end for each bit of data
		PORTB |= 0x04;
	}
	
	// set RCLK = 1. Rising edge copies data from the "Shift" register to the "Storage" register
	PORTB |= 0x02;
	// clears all lines in preparation of a new transmission
	PORTB = 0x00;
}
