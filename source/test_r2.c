#include "utils.h"
#include "3140_concur.h"
#include "realtime.h"

/*--------------------------*/
/* Parameters for test case */
/*--------------------------*/



/* Stack space for processes */
#define NRT_STACK 70
#define RT_STACK  70



/*--------------------------------------*/
/* Time structs for real-time processes */
/*--------------------------------------*/

/* Constants used for 'work' and 'deadline's */
realtime_t t_1msec = {0, 1};


/* Process start time */
realtime_t t_pRT1 = {0, 0};


/*------------------*/
/* Helper functions */
/*------------------*/
void shortDelay(){delay();}
void mediumDelay() {delay(); delay();}



/*----------------------------------------------------
 * Non real-time process
 *   Blinks red LED 10 times.
 *   Should be blocked by real-time process at first.
 *----------------------------------------------------*/

void pNRT1(void) {
	int i;
	for (i=0; i<20;i++){
	LEDRed_On();
	shortDelay();
	LEDRed_Toggle();
	shortDelay();
	}

}
//Toggles green LED 10 times
void pNRT2(void) {
	int i;
	for (i=0; i<20;i++){
	LEDGreen_On();
	shortDelay();
	LEDGreen_Toggle();
	shortDelay();
	}

}

/*-------------------
 * Real-time process
 *-------------------*/

void pRT1(void) {
	LEDGreen_On();
	mediumDelay();
	mediumDelay();
	LEDGreen_Toggle();
	mediumDelay();
}


/*--------------------------------------------*/
/* Main function - start concurrent execution */
/*--------------------------------------------*/
int main(void) {

	LED_Initialize();

    /* Create processes */
    if (process_create(pNRT1, NRT_STACK) < 0) { return -1; }
    if (process_create(pNRT2, NRT_STACK) < 0) { return -1; }
    if (process_rt_create(pRT1, RT_STACK, &t_pRT1, &t_1msec) < 0) { return -1; }

    /* Launch concurrent execution */
	process_start();

  LED_Off();
  while(process_deadline_miss>0) {
		LEDRed_On();
		LEDGreen_On();
		shortDelay();
		LED_Off();
		shortDelay();
		process_deadline_miss--;
	}

	/* Hang out in infinite loop (so we can inspect variables if we want) */
	while (1);
	return 0;
}
