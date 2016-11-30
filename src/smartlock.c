/*	Name: Uriel Naranjo
 *	Lab Section: 022
 *	Assignment: Final Project 
 *	Exercise Description: Bluetooth driven smartlock
 *	
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */

#include <avr/io.h>
#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"
#include "usart_ATmega1284.h"

// global variables
unsigned char sound = 0; // set to number of sound to play
unsigned char lockres = 0; // resetting lock when set
unsigned char Data2Transmit = 0x00; // data transmited to servant
unsigned char valid = 0; // if code is valid advance to next state
unsigned char islocked = 0; // locked = 1 unlocked = 0
unsigned char lockout = 0; // high when too many failed attempts
unsigned char lockdir = 0; // 0: counterclockwise 1: clockwise
unsigned char go = 0; // notifies stepper motor to turn the lock
unsigned char row = 0x01; // rows to display on led matrix
unsigned char data; // data received from usart
unsigned char currlock = 0; // number of current lock 
unsigned char attempts = 0; // number of failed attempts
unsigned char locks[3] = {0x12, 0x34, 0x56}; // multiple locks for more security

// array of led patterns to light when locked
unsigned char lockedred[8] = {0xFF, 0xFF, 0xC1, 0xB1, 0xB1, 0xC1, 0xFF, 0xFF};
unsigned char lockedgreen[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char lockedblue[8] = {0x00, 0x00, 0x3E, 0x4E, 0x4E, 0x3E, 0x00, 0x00};

// array of led patterns to light when unlocked
unsigned char unlockedred[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
unsigned char unlockedgreen[8] = {0x00, 0x00, 0xE0, 0xE4, 0xE4, 0xE3, 0x00, 0x00};
unsigned char unlockedblue[8] = {0xFF, 0xFF, 0xF1, 0xB1, 0xB1, 0xC1, 0xFF, 0xFF};

// array of led patterns to light when locked out due to failed attempts
unsigned char lockedoutred[8] = {0xFE, 0xFD, 0x8B, 0xFB, 0xFB, 0x8B, 0xFD, 0xFE};
unsigned char lockedoutgreen[8] = {0x10, 0x20, 0x47, 0x40, 0x40, 0x47, 0x20, 0x10};
unsigned char lockedoutblue[8] = {0x01, 0x02, 0x74, 0x04, 0x04, 0x74, 0x02, 0x01};

// array of led patterns to light when resetting lock pins
unsigned char resetlockred[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char resetunlockred[8] = {0x00, 0x00, 0x0E, 0x4E, 0x4E, 0x3E, 0x00, 0x00};

// Musical Notes frequency for pwm
#define C4 261.63
#define D4 293.66
#define E4 329.63
#define F4 249.23
#define G4 392.00
#define A4 440.00
#define B4 493.88
#define C5 523.25
#define zero 0.00
const double lockpwm[8] = {C5, B4, A4, G4, F4, E4, D4, C4};
const double unlockpwm[8] = {C4, D4, E4, F4, G4, A4, B4, C5};
const double lockoutpwm[8] = {C4, C4, C4, C4, C4, C4, C4, C4};
const double correctpwm[8] = {C4, C4, E4, E4, A4, A4, C5, C5};
double failpwm[8] = {C5, C5, C5, C5, C4, C4, C4, C4};

// Smart Lock main control state machine
enum smartlock{lockInit, unlocked, locked, secondlock, thirdlock, lockedout}lockstate;
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
enum ReceiveData{RecInit, RecWait, RecProcess, RecData, RecIncorrect, RecUnlock, RecResWait, RecReset}recState;
void ReceiveInit();
void ReceiveTick();
void ReceiveTask();

// Speaker SM
enum SpeakerSM{SpeakInit, SpeakOff, SpeakLock, SpeakUnlock, SpeakFail, SpeakLockout, SpeakCorrect} speakstate;
void SpeakerInit();
void SpeakerTick();
void SpeakerTask();

// PWM Functions
void set_PWM(double frequency);
void PWM_on();
void PWM_off();

// shift register function
void MatrixTransmit(unsigned char rowdata, unsigned char reddata, unsigned char greendata, unsigned char bluedata);

void StartSecPulse(unsigned portBASE_TYPE Priority){
	xTaskCreate(smartlockTask, (signed portCHAR *)"smartlockSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	//xTaskCreate(stepTask, (signed portCHAR *)"stepSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(LEDMatrixTask, (signed portCHAR *)"LEDMatrixSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(TransmitTask, (signed portCHAR *)"TransmitSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
   	xTaskCreate(ReceiveTask, (signed portCHAR *)"ReceiveSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(SpeakerTask, (signed portCHAR *)"SpeakerSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(){
	// initialize ports to input or output
	DDRA = 0xFF; PORTA = 0x00; // A0-A4 stepper motor
	DDRB = 0xFF; PORTB = 0x00; // led matrix B0-B5, PWM Speaker: B6, B7: LED Matrix
	DDRC = 0xFF; PORTC = 0x00; // C0 button
	
	// intitialize USART
	initUSART(0);
	initUSART(1);

	// intiliaze pwm
	PWM_on();
	set_PWM(zero);

	StartSecPulse(1);
	vTaskStartScheduler();

	return 0;
}

void smartlockInit(){
	lockstate = lockInit;
}

void smartlockTick(){
	static unsigned char lockwait;
	// state transitions
	switch(lockstate){
		case lockInit:
			lockwait = 0;
			lockstate = locked;
			break;
	
		case unlocked:
			if(valid){
				go = 1;
				attempts = 0;
				lockdir = 0;
				valid = 0;
				currlock = 0;
				lockedgreen[2] = 0x00;
				lockedgreen[3] = 0x00;
				lockedgreen[4] = 0x00;
				lockedgreen[5] = 0x00;
				lockedred[3] = 0xB1;
				lockedred[4] = 0xB1;
				lockedred[5] = 0xC1;
				lockstate = locked;
			}
			else{
				lockstate = unlocked;
			}
			break;
	
		case locked: 
			if(valid && (attempts < 3) ){
				valid = 0;
				lockstate = secondlock;
			}
			else if(attempts >= 3){
				lockedred[3] = 0xB1;
				lockedred[4] = 0xB1;
				lockedred[5] = 0xC1;
				attempts = 0;
				lockout = 1;
				lockstate = lockedout; // replace with timed lockout
			}
			else{
				lockstate = locked;
			}
			break;
		
		case secondlock:
			if(valid && (attempts < 3) ){
				valid = 0;
				lockstate = thirdlock;
			}
			else if(attempts >= 3){
				lockedgreen[2] = 0x00;
				lockedgreen[3] = 0x00;
				lockedgreen[4] = 0x00;
				lockedgreen[5] = 0x00;
				lockedred[3] = 0xB1;
				lockedred[4] = 0xB1;
				lockedred[5] = 0xC1;
				attempts = 0;
				lockout = 1;
				lockstate = lockedout; 
			}
			else{
				lockstate = secondlock;
			}
			break;
			
		case thirdlock:
			if(valid && attempts < 3){
				lockdir = 1;
				go = 1;
				valid = 0;
				currlock = 0;
				lockstate = unlocked;
			}
			else if(attempts >= 3){
				lockedgreen[2] = 0x00;
				lockedgreen[3] = 0x00;
				lockedgreen[4] = 0x00;
				lockedgreen[5] = 0x00;
				lockedred[3] = 0xB1;
				lockedred[4] = 0xB1;
				lockedred[5] = 0xC1;
				attempts = 0;
				lockout = 1;
				lockstate = lockedout; 
			}
			else{
				lockstate = thirdlock;
			}
			break;
			
		case lockedout:
			if(lockwait < 200){
				lockstate = lockedout;
			}
			else{
				attempts = 0;
				lockwait = 0;
				lockout = 0;
				currlock = 0;
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
			islocked = 0;
			break;
		
		case locked:
			islocked = 1;
			if(attempts == 1){
				lockedred[3] |= 0xF0;
				lockedred[4] |= 0xF0;
			}
			else if(attempts == 2){
				lockedred[5] |= 0x30;
			}
			break;
	
		case secondlock:
			lockedgreen[2] = 0x20;
			lockedgreen[3] = 0x20;
			lockedgreen[4] = 0x20;
			lockedgreen[5] = 0x20;
			if(attempts == 1){
				lockedred[3] |= 0xF0;
				lockedred[4] |= 0xF0;
			}
			else if(attempts == 2){
				lockedred[5] |= 0x30;
			}
			islocked = 1;
			break;
		
		case thirdlock:
			lockedgreen[2] = 0x60;
			lockedgreen[3] = 0x60;
			lockedgreen[4] = 0x60;
			lockedgreen[5] = 0x60;
			if(attempts == 1){
				lockedred[3] |= 0xF0;
				lockedred[4] |= 0xF0;
			}
			else if(attempts == 2){
				lockedred[5] |= 0x30;
			}
			islocked = 1;
			break;
		
		case lockedout:
			lockwait++;
			break;
		
		default:
			break;
	}
	PORTA = islocked;
}

void smartlockTask(){
	smartlockInit();
	for(;;){
		smartlockTick();
		vTaskDelay(50);
	}
}

void SpeakerInit(){
	speakstate = SpeakInit;
}

void SpeakerTick(){
	static unsigned char i;
	// state transitions
	switch(speakstate){
		case SpeakInit:
			i = 0;
			speakstate = SpeakOff;
			break;

		case SpeakOff:
			if(sound == 1){
				sound = 0;
				speakstate = SpeakLock;
			}
			else if(sound == 2){
				sound = 0;
				speakstate = SpeakUnlock;
			}
			else if(sound == 3){
				sound = 0;
				speakstate = SpeakFail;
			}
			else if(sound == 4){
				sound = 0;
				speakstate = SpeakLockout;
			}
			else if(sound == 5){
				sound = 0;
				speakstate = SpeakCorrect;
			}
			else{
				speakstate = SpeakOff;
			}
			break;

		case SpeakLock:
			if(i < 8){
				speakstate = SpeakLock;
			}
			else{
				i = 0;
				speakstate = SpeakOff;
			}
			break;
		
		case SpeakUnlock:
			if(i < 8){
				speakstate = SpeakUnlock;
			}
			else{
				i = 0;
				speakstate = SpeakOff;
			}
			break;
		
		case SpeakFail:
			if(i < 8){
				speakstate = SpeakFail;
			}
			else{
				i = 0;
				speakstate = SpeakOff;
			}
			break;

		case SpeakCorrect:
			if(i < 8){
				speakstate = SpeakCorrect;
			}
			else{
				i = 0;
				speakstate = SpeakOff;
			}
			break;	
		
		case SpeakLockout:
			if(lockout){
				speakstate = SpeakLockout;
			}
			else{
				i = 0;
				speakstate = SpeakOff;
			}
			break;
		
		default:
			speakstate = SpeakInit;
			break;
	}

	// state actions
	switch(speakstate){
		case SpeakInit:
			break;
		
		case SpeakOff:
			set_PWM(0);
			break;
		
		case SpeakLock:
			set_PWM(lockpwm[i]);
			i++;
			break;	
		
		case SpeakUnlock:
			set_PWM(unlockpwm[i]);
			i++;
			break;

		case SpeakFail:
			set_PWM(failpwm[i]);
			i++;
			break;

		case SpeakLockout:
			set_PWM(C4);
			i++;
			break;

		case SpeakCorrect:
			set_PWM(correctpwm[i]);
			i++;
			break;
		
		default: 
			break;
	}
}

void SpeakerTask(){
	SpeakerInit();
	for(;;){
		SpeakerTick();
		vTaskDelay(50);
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
	static unsigned char j;
	unsigned char red, green, blue;
	// state transitions
	switch(ledstate){
		case ledInit:
			j = 0;
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
			if(j >= 8){
				j = 0;
				row = 0x01;
			}
			if(islocked && !lockout){
				if(!lockres){
					red = lockedred[j];
				}
				else{
					red = resetlockred[j];
				}
				green = lockedgreen[j];
				blue = lockedblue[j];
			}
			else if(!islocked && !lockout){
				blue = unlockedblue[j];
				green = unlockedgreen[j];
				if(!lockres){
					red = unlockedred[j];
				}
				else{
					red = resetunlockred[j];
				}
			}
			else{
				red = lockedoutred[j];
				green = lockedoutgreen[j];
				blue = lockedoutblue[j];
			}
			MatrixTransmit(row, ~red, ~green, ~blue);
			j++;
			row = row << 1;
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
			lockres = 0;
            recState = RecWait;
            break;
           
        case RecWait:
            if( USART_HasReceived(0) && !lockout ){
				data = USART_Receive(0);
                recState = RecProcess;
            }
            else{
                recState = RecWait;
            }
            break;
			
		case RecProcess:
			if(data == 0xFF && !islocked){
				recState = RecUnlock;
			}
			else if(data == locks[currlock]){
				recState = RecData;
			}
			else if(data == 0x00 & !currlock){
				lockres = 1;
				recState = RecResWait;
			}
			else{
				recState = RecIncorrect;
			}
			break;				
           
        case RecData:
            USART_Flush(0);
            recState = RecWait;
            break;
            
		case RecUnlock:
			USART_Flush(0);
			recState = RecWait;
			break;
		
		case RecIncorrect:
			USART_Flush(0);
			recState = RecWait;
			break;
		
		case RecResWait:
			if( USART_HasReceived(0) ){
				data = USART_Receive(0);
				if(data == 0x00 || data == 0xFF){
					USART_Flush(0);
					recState = RecResWait;
				}
				else{
					recState = RecReset;
				}
			}
			else{
				recState = RecResWait;
			}
			break;
		
		case RecReset:
			USART_Flush(0);
			if(lockres >= 4){
				lockres = 0;
				recState = RecWait;
			}
			else{
				recState = RecResWait;
			}
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
			// discard data received while lockedout
			if(lockout){
				USART_Flush(0); 	
			}
            break;
          
		case RecProcess:
			break;
		
        case RecData:
            // data = USART_Receive(0);				
			currlock++;
			valid = 1;
			if(currlock == 3){
				sound = 2;
			}
			else{
				sound = 5;
			}						
            break;
		
		case RecIncorrect:
			attempts++; // number of failed attempts
			if(attempts >= 3){
				sound = 4;
			}
			else{
				sound = 3;
			}
			break;
		
		case RecUnlock:
			sound = 1;
			valid = 1;
			break;
		
		case RecResWait:
			break;
		
		case RecReset:
			locks[lockres-1] = data;
			lockres++;
			break;
		
		default:
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
			// (lockout) | (attempts) | (currlock) | (locked?)
			Data2Transmit = (lockres << 6) | (attempts << 4) | (currlock << 2) | (lockout << 1) |(islocked); 
			USART_Send(Data2Transmit, 1);
			break;
		
		default:
			break;
	}
}

void TransmitTask(){
	TransmitInit();
	for(;;){
		TransmitTick();
		vTaskDelay(100);
	}
}

// 0.954 hz is lowest frequency possible with this function,
// based on settings in PWM_on()
// Passing in 0 as the frequency will stop the speaker from generating sound
void set_PWM(double frequency) {
	static double current_frequency; // Keeps track of the currently set frequency
	// Will only update the registers when the frequency changes, otherwise allows
	// music to play uninterrupted.
	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; } //stops timer/counter
		else { TCCR3B |= 0x03; } // resumes/continues timer/counter
		
		// prevents OCR0A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR3A = 0xFFFF; }
		
		// prevents OCR0A from underflowing, using prescaler 64					// 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR3A = 0x0000; }
		
		// set OCR0A based on desired frequency
		else { OCR3A = (short)(8000000 / (128 * frequency)) - 1; }


		TCNT3 = 0; // resets counter
		current_frequency = frequency; // Updates the current frequency
	}
}

void PWM_on() {
	TCCR3A = (1 << COM3A0);
	// COM0A0: Toggle PB3 on compare match between counter and OCR0A
	TCCR3B = (1 << WGM02) | (1 << CS01) | (1 << CS00);
	// WGM02: When counter (TCNT0) matches OCR0A, reset counter
	// CS01 & CS00: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}

// send led matrix data to four shift registers
void MatrixTransmit(unsigned char rowdata, unsigned char reddata, unsigned char greendata, unsigned char bluedata) {
	unsigned char r, g, b, temprow;
	// for each bit of data
	for(int i = 7; i >=0; --i){
		// Set SRCLR to 1 allowing data to be set
		// Also clear SRCLK in preparation of sending data
		PORTB = 0x08;
		PORTA |= 0x10;
		// set SER = next bit of data to be sent for each led color and row.
		r = (( (reddata >> i) & 0x01) << 5); // ser location of red led
		g = ( (greendata >> i) & 0x01); // ser location of green led
		b = (( (bluedata >> i) & 0x01) << 7); // ser location of blue led
		temprow = (((rowdata >> i) & 0x01) << 4); // ser location of rows
		PORTB |= (temprow | r | g | b);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		// end for each bit of data
		PORTB |= 0x04;
	}
	
	// set RCLK = 1. Rising edge copies data from the "Shift" register to the "Storage" register
	PORTB |= 0x02;
	// clears all lines in preparation of a new transmission
	PORTB &= 0x40;
	PORTA &= 0xEF;
}