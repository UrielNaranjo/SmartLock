#include <avr/io.h>
#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"
#include "usart_ATmega1284.h"

// global variables
unsigned char lockdir = 0; // 0: counterclockwise 1: clockwise
unsigned char go = 0; // notifies stepper motor to turn the lock
unsigned char row; // rows to display on led matrix
unsigned char pattern; // patterns to display on led matrix
unsigned char data; // data received from app

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

// shift register functions
void rowTransmit(unsigned char data);
void patternTransmit(unsigned char data);

void StartSecPulse(unsigned portBASE_TYPE Priority){
	xTaskCreate(stepTask, (signed portCHAR *)"stepSecTask", configMINIMAL_STACK_SIZE, NULL, 3, NULL );
	xTaskCreate(LEDMatrixTask, (signed portCHAR *)"LEDMatrixSecTask", configMINIMAL_STACK_SIZE, NULL, 4, NULL );
	xTaskCreate(TransmitTask, (signed portCHAR *)"TransmitSecTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL );
   	xTaskCreate(ReceiveTask, (signed portCHAR *)"ReceiveSecTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL );
}

int main(){
	
	// intitialize USART
	initUSART(0);

	StartSecPulse(1);
	vTaskStartScheduler();

	return 0;
}


void stepInit(){
	stepperState = stepperInit;
}

void stepTick(){
	static unsigned char numPhases; // num of motor phases to lock/unlock
	static unsigned char output;
	// state transitions
	switch(stepperState){
		case stepperInit:
			stepperState = stepperWait;
			break;

		case stepperWait:
			if(go){
				go = 0;
				numPhases = 180;
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
			rowTransmit(~row);
			patternTransmit(pattern);
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

// send LED matrix data to shift register
void rowTransmit(unsigned char data) {
	// for each bit of data
	for(int i = 7; i >=0; --i){
		// Set SRCLR to 1 allowing data to be set
		// Also clear SRCLK in preparation of sending data
		PORTC = 0x08;
		// set SER = next bit of data to be sent.
		PORTC |= (data >>i) & 0x01;
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		// end for each bit of data
		PORTC |= 0x04;
	}
	
	// set RCLK = 1. Rising edge copies data from the "Shift" register to the "Storage" register
	PORTC |= 0x02;
	// clears all lines in preparation of a new transmission
	PORTC = 0x00;
}

// send LED matrix data to shift register
void patternTransmit(unsigned char data) {
	// for each bit of data
	for(int i = 7; i >=0; --i){
		// Set SRCLR to 1 allowing data to be set
		// Also clear SRCLK in preparation of sending data
		PORTC = 0x08;
		// set SER = next bit of data to be sent.
		PORTC |= (data >>i) & 0x01;
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		// end for each bit of data
		PORTC |= 0x04;
	}
	
	// set RCLK = 1. Rising edge copies data from the "Shift" register to the "Storage" register
	PORTC |= 0x02;
	// clears all lines in preparation of a new transmission
	PORTC = 0x00;
}