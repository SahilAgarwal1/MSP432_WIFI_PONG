#include <G8RTOS_Lab5/G8RTOS.h>
#include "msp.h"
#include "Game.h"

/**
 * main.c
 */


void idle(){
    while(1);
}


void main(void)
{

    G8RTOS_Init(false);
    //port4_setUp();

    // generate touch interrupt on button press

    P4->DIR &= ~BIT4;//set direction
    P4->IFG &= ~BIT4; // clear flag
    P4->IE |= BIT4; // enable interrupt
    P4->IES |= BIT4; // high to low transition
    P4->REN |= BIT4; // PULL up resistor
    P4->OUT |= BIT4; // set res to pull up



   G8RTOS_InitSemaphore(&LCD_SEM, 1);
   G8RTOS_InitSemaphore(&gamestate_sem, 1);

   // add our normal threads
   G8RTOS_AddThread(&gameInit, 1, "game_init");
   G8RTOS_AddThread(&idle,              255,                   "idle");

   // add the aperiodic event
   G8RTOS_AddAPeriodicEvent(&ButtonTap_port4, 4, PORT4_IRQn);
   //G8RTOS_AddAPeriodicEvent(&ButtonTap_port5, 4, PORT5_IRQn);

    // and launch the OS!
    G8RTOS_Launch();



}




