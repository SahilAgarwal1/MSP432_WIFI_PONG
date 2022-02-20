#include <G8RTOS_Lab5/G8RTOS.h>
#include <Game_temp_1.h>
#include "msp.h"
#include "board.h"

/**
 * main.c
 */

#define IS_HOST 0


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




   // add our normal threads
    if(IS_HOST)
        G8RTOS_AddThread(&CreateGame, 1, "CreateGame");
    else
        G8RTOS_AddThread(&JoinGame, 1, "JoinGame");

   // add the aperiodic event
   //G8RTOS_AddThread(&IdleThread, 255, "Idle");
   G8RTOS_AddAPeriodicEvent(&ButtonTap_port4, 4, PORT4_IRQn);
   //G8RTOS_AddAPeriodicEvent(&ButtonTap_port5, 4, PORT5_IRQn);

    // and launch the OS!
    G8RTOS_Launch();





//    G8RTOS_Init(true);
//    initCC3100(Client);
//    char data = '1';
//    while(1){
//        Delay(500);
//        SendData((unsigned char *)&data, HOST_IP_ADDR, sizeof(char));
//    }
}



