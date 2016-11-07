#include <avr/io.h>
#include "scheduler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

// global variables
unsigned char lockdir = 0; // 0: counterclockwise 1: clockwise
unsigned char go = 0; // notifies stepper motor to turn the lock

// Stepper motor SM to control the turning of the lock
enum StepperMotor{stepperInit, stepperWait, A, AB, B, BC, C, CD, D, DA }stepperState;
void stepperInit();
void stepperTick();
void stepperTask();

void StartSecPulse(unsigned portBASE_TYPE Priority){
	TaskCreate(stepperTask, (signed portCHAR *)"stepperSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(){

	StartSecPulse(1);
	vTaskStartScheduler();

	return 0;
}


void stepperInit(){
	stepperState = stepperInit;
}

void stepperTick(){
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

void stepperTask(){
	stepperInit();
	for(;;){
		stepperTask();
		vTaskDelay(3);
	}
}
