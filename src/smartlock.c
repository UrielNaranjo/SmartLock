#include <avr/io.h>
#include "scheduler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

enum temp{temp1, temp2, temp3}tempState;

void tempInit();
void tempTick();
void tempTask();

void StartSecPulse(unsigned portBASE_TYPE Priority){
	TaskCreate(tempTask, (signed portCHAR *)"tempSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(){

	StartSecPulse(1);
	vTaskStartScheduler();

	return 0;
}

void tempInit(){
	tempstate = temp1;
}

void tempTick(){
	switch(tempState){

	}
	switch(tempState){

	}
}

void tempTask(){
	tempInit();
	for(;;){
		tempTask();
		vTaskDelay(100);
	}
}
