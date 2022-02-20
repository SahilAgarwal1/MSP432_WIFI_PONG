/*
 * Game.c
 *
 *  Created on: Apr 16, 2021
 *      Author: Kyle
 */

#include "Game.h"
#include "cc3100_usage.h"
#include <stdio.h>
#include <stdlib.h>
#include "G8RTOS_Lab5/G8RTOS_Scheduler.h"

#define ACK 1
#define START_GAME 2

GameState_t gamestate;
SpecificPlayerInfo_t clientInfo;
GeneralPlayerInfo_t clientGeneralInfo;

semaphore_t net_sem, gamestate_sem, LCD_SEM;

bool buttonTap;

int generate_random(int l, int r) { //this will generate random number in range l and r
      return (rand() % (r - l + 1)) + l;
}

void ButtonTap_port4(){ // set flag for a touch
    if(P4->IFG & BIT4){
        P4->IFG &= ~BIT4;
        buttonTap = true;
    }
}

/*********************************************** Host Threads *********************************************************************/

// Thread for the host to create a game
void CreateGame() {
    G8RTOS_AddThread(IdleThread, 255, "Idle");

    initCC3100(Host);

    // LED indicator
    initLEDs();
    turnLedOn(LED2);

    G8RTOS_InitSemaphore(&net_sem, 1);
    G8RTOS_InitSemaphore(&gamestate_sem, 1);
    G8RTOS_InitSemaphore(&LCD_SEM, 1);

    gamestate.players[1].currentCenter = 160;
    gamestate.players[0].currentCenter = 160;
    gamestate.players[0].color = LCD_BLUE;
    gamestate.players[1].color = LCD_RED;
    gamestate.players[0].position = BOTTOM;
    gamestate.players[1].position = TOP;

    gamestate.LEDScores[0] = 0;
    gamestate.LEDScores[1] = 0;

    // Handshake
    // Wait for client
    LCD_Text(MAX_SCREEN_X > 2, MAX_SCREEN_Y >> 2, "WAITING FOR CLIENT CONNECTION", LCD_WHITE);

//    int32_t retVal = NOTHING_RECEIVED;
//    while (retVal == NOTHING_RECEIVED){
//        G8RTOS_WaitSemaphore(&net_sem);
//        retVal = ReceiveData((uint8_t *)&clientInfo, sizeof(clientInfo));
//        G8RTOS_SignalSemaphore(&net_sem);
//        OS_Sleep(1);
//    }

    // Wait for client ACK
    uint8_t resp = 0;
    uint32_t retVal = 0;
    while (!retVal){
        // Send ACK
        resp = ACK;
        SendData((unsigned char *)&resp, HOST_IP_ADDR, 1);
        OS_Sleep(50);

//        resp = 0;
//        ReceiveData((uint8_t *)&resp, sizeof(resp));
//        OS_Sleep(1);
        retVal = ReceiveData((uint8_t *)&clientInfo, sizeof(clientInfo));
        OS_Sleep(50);
    }

    // move clientInfo to gamestate

    // Handshake complete
    turnLedOn(LED1);

    buttonTap = 0;

    LCD_Clear(LCD_BLACK);

    LCD_Text(MAX_SCREEN_X > 2, MAX_SCREEN_Y >> 2, "PRESS BUTTON TO START GAME", LCD_WHITE);
    while(!buttonTap);

    // Send START_GAME
    uint8_t start = START_GAME;
    SendData((unsigned char *)&start, HOST_IP_ADDR, 1);


    LCD_Clear(LCD_BLACK);

    for(int i = ARENA_MIN_Y; i <= ARENA_MAX_Y; i++){
        LCD_SetPoint(ARENA_MAX_X, i, LCD_GRAY);
        LCD_SetPoint(ARENA_MIN_X, i, LCD_GRAY);
    }

    // draw the paddles
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, ARENA_MIN_Y, ARENA_MIN_Y+ (PADDLE_WID), LCD_RED);
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, 235 , 239, LCD_BLUE);

    char str[2];
    sprintf(str, "%d", gamestate.overallScores[0]);
    LCD_Text(0, MAX_SCREEN_Y-15, (uint8_t *)str, LCD_BLUE);
    sprintf(str, "%d", gamestate.overallScores[1]);
    LCD_Text(310, 0, (uint8_t *)str, LCD_RED);


    G8RTOS_AddThread(&SendDataToClient, 10, "SendDataToClient");
    G8RTOS_AddThread(&ReceiveDataFromClient, 10, "ReceiveDataFromClient");
    G8RTOS_AddThread(&GenerateBall, 10, "GENERATE_BALL");
    G8RTOS_AddThread(&DrawObjects, 10, "DRAW_OBJECTS");
    G8RTOS_AddThread(&ReadJoystickHost, 10, "READ_JOYSTICK_HOST");
    G8RTOS_AddThread(&MoveLEDs, 10, "MoVE_LEDS");

    G8RTOS_KillSelf();
}


// Thread that sends game state to client
void SendDataToClient(){
    while(1){

        // Make packet
        G8RTOS_WaitSemaphore(&gamestate_sem);
        GameState_t packet = gamestate;
        G8RTOS_SignalSemaphore(&gamestate_sem);

        // Send packet
        G8RTOS_WaitSemaphore(&net_sem);
        SendData((unsigned char *)&packet, HOST_IP_ADDR, sizeof(packet));
        G8RTOS_SignalSemaphore(&net_sem);

        // Check if game is done
        if (gamestate.gameDone){
            G8RTOS_AddThread(&EndOfGameHost, 5, "EndOfGameHost");
        }

        OS_Sleep(5);
    }
}


// Thread that receives UDP packets from client
void ReceiveDataFromClient(){
    SpecificPlayerInfo_t packet;
    while(1){
        int32_t retVal = NOTHING_RECEIVED;

        // Wait for data
        while (retVal == NOTHING_RECEIVED){
            G8RTOS_WaitSemaphore(&net_sem);
            retVal = ReceiveData((uint8_t *)&packet, sizeof(packet));
            G8RTOS_SignalSemaphore(&net_sem);
            OS_Sleep(1);
        }

        // Update the player�s current center with the displacement received from the client
        G8RTOS_WaitSemaphore(&gamestate_sem);
        if(packet.displacement  >= 200 && (gamestate.players[1].currentCenter - PADDLE_LEN_D2 >= ARENA_MIN_X)){
            gamestate.players[1].currentCenter -= 3;
        }
        else if(packet.displacement <= -200 && (gamestate.players[1].currentCenter + PADDLE_LEN_D2 <= ARENA_MAX_X)){
            gamestate.players[1].currentCenter += 3;
        }
        G8RTOS_SignalSemaphore(&gamestate_sem);

        OS_Sleep(2);
    }
}


// Generate Ball thread
void GenerateBall(){
    while(1){
        // wait for gamestate semaphore;
        G8RTOS_WaitSemaphore(&gamestate_sem); // wait for the semaphore;

        if(gamestate.numberOfBalls < MAX_NUM_OF_BALLS){
            G8RTOS_AddThread(&MoveBall, 10, "BALL");
        }
        uint8_t balls = gamestate.numberOfBalls;
        G8RTOS_SignalSemaphore(&gamestate_sem); // release semaphore;

        OS_Sleep(1000 * (balls + 1));
    }
}


// Thread to read host's joystick
void ReadJoystickHost(){
    int16_t xbias, ybias;
    int16_t xpos, ypos;
    GetJoystickCoordinates(&xbias, &ybias);

    while(1){
        GetJoystickCoordinates(&xpos, &ypos);
        xpos = xpos - xbias;
        //ypos = ypos - ybias;

        OS_Sleep(10);

        G8RTOS_WaitSemaphore(&gamestate_sem);
        if(xpos >= 1000 && (gamestate.players[0].currentCenter - PADDLE_LEN_D2 >= ARENA_MIN_X)){
            gamestate.players[0].currentCenter -= 3;
        }
        else if(xpos <= -1000 && (gamestate.players[0].currentCenter + PADDLE_LEN_D2 <= ARENA_MAX_X)){
            gamestate.players[0].currentCenter += 3;
        }

        G8RTOS_SignalSemaphore(&gamestate_sem);

    }
}


// Thread to move a single ball
void MoveBall(){
    int i; // counter

    G8RTOS_WaitSemaphore(&gamestate_sem); // need semaphore to do all the initializatios

    int16_t velocity_x = generate_random(-4, 4); // randomize later
    int16_t velocity_y = generate_random(-8, 8); // randomize later

    if(velocity_x == 0) velocity_x = 1;
    if(velocity_y == 0) velocity_y = 1;




    for(i = 0; i < MAX_NUM_OF_BALLS; i++){
        if(!gamestate.balls[i].alive) break; // i is now the dead ball
    }

    // now we must initialize the ball;
    gamestate.balls[i].alive = true;
    gamestate.balls[i].currentCenterX = generate_random(ARENA_MIN_X + 30, ARENA_MAX_X - 30); // change to random center later
    gamestate.balls[i].currentCenterY = generate_random(ARENA_MIN_Y + 30, ARENA_MAX_Y - 30);
    gamestate.balls[i].color = LCD_WHITE;

    // create a prev ball object
    //PrevBall_t prevBall;
    //prevBall.CenterX = gamestate.balls[i].currentCenterX;
    //prevBall.CenterY = gamestate.balls[i].currentCenterY;

    gamestate.numberOfBalls++;

    G8RTOS_SignalSemaphore(&gamestate_sem); // release sem after initialization

    while(1){ // main loop for moving ball and checking for collisions
        G8RTOS_WaitSemaphore(&gamestate_sem);

        // update positions
        gamestate.balls[i].currentCenterX += velocity_x;
        gamestate.balls[i].currentCenterY += velocity_y;

        // check for contact with paddle


            if(((gamestate.balls[i].currentCenterY - BALL_SIZE_D2) <= (ARENA_MIN_Y + 4)) && (gamestate.balls[i].currentCenterX <=
                    gamestate.players[1].currentCenter + PADDLE_LEN_D2) && (gamestate.balls[i].currentCenterX >= gamestate.players[1].currentCenter - PADDLE_LEN_D2)){
                gamestate.balls[i].currentCenterY = ARENA_MIN_Y + PADDLE_WID + BALL_SIZE_D2 + 1;
                velocity_y *= -1;
                if(gamestate.balls[i].color == LCD_WHITE){
                    gamestate.balls[i].color = LCD_RED;
                }

                if(gamestate.balls[i].currentCenterX >= gamestate.players[1].currentCenter + 11){
                    velocity_x = abs(velocity_x);
                }
                else if(gamestate.balls[i].currentCenterX <= gamestate.players[1].currentCenter + 11){
                    velocity_x = abs(velocity_x) * -1;
                }

            }
            else if(((gamestate.balls[i].currentCenterY + BALL_SIZE_D2) >= (ARENA_MAX_Y - 4)) && (gamestate.balls[i].currentCenterX <=
                    gamestate.players[0].currentCenter + PADDLE_LEN_D2) && (gamestate.balls[i].currentCenterX >= gamestate.players[0].currentCenter - PADDLE_LEN_D2)){
                gamestate.balls[i].currentCenterY = ARENA_MAX_Y - PADDLE_WID - BALL_SIZE_D2 - 1;
                velocity_y *= -1;
                if(gamestate.balls[i].color == LCD_WHITE){
                    gamestate.balls[i].color = LCD_BLUE;
                }

                if(gamestate.balls[i].currentCenterX >= gamestate.players[0].currentCenter + 11){
                    velocity_x = abs(velocity_x);
                }
                else if(gamestate.balls[i].currentCenterX <= gamestate.players[0].currentCenter + 11){
                    velocity_x = abs(velocity_x) * -1;
                }

            }


        // check for collision between end and ball
            else if((gamestate.balls[i].currentCenterY - BALL_SIZE_D2) <= ARENA_MIN_Y - BALL_SIZE){
            //gamestate.balls[i].currentCenterY = ARENA_MIN_Y + BALL_SIZE_D2;
            //velocity_y *= -1;
                if(gamestate.balls[i].color == LCD_BLUE){
                    gamestate.LEDScores[0] += 1;
                }

                gamestate.balls[i].color = LCD_WHITE;

                // if one player wins, end game

                if (gamestate.LEDScores[0] == 16){
                    gamestate.LEDScores[1] = 0;
                    gamestate.overallScores[0]++;
                    gamestate.gameDone = 1;
                    gamestate.winner = 0;
//                    G8RTOS_AddThread(&EndOfGameHost, 1, "END_OF_GAME");
                }

                gamestate.numberOfBalls--;
                gamestate.balls[i].alive = false;
                G8RTOS_SignalSemaphore(&gamestate_sem);
                G8RTOS_KillSelf();
            }
            else if((gamestate.balls[i].currentCenterY + BALL_SIZE_D2) >= ARENA_MAX_Y + BALL_SIZE){
                //gamestate.balls[i].currentCenterY = ARENA_MAX_Y - BALL_SIZE_D2;
                //velocity_y *= -1;

                if(gamestate.balls[i].color == LCD_RED){
                    gamestate.LEDScores[1] += 1;
                }


                gamestate.balls[i].color = LCD_WHITE;

                // add host end game

                if (gamestate.LEDScores[1] == 16){
//                    G8RTOS_AddThread(&EndOfGameHost, 1, "END_OF_GAME");
                    gamestate.LEDScores[0] = 0;
                    gamestate.overallScores[1]++;
                    gamestate.gameDone = 1;
                    gamestate.winner = 1;
                }

                gamestate.numberOfBalls--;
                gamestate.balls[i].alive = false;
                G8RTOS_SignalSemaphore(&gamestate_sem);
                G8RTOS_KillSelf();
            }

        // check collision between x and ball

        if((gamestate.balls[i].currentCenterX - BALL_SIZE_D2) <= ARENA_MIN_X){
            gamestate.balls[i].currentCenterX = ARENA_MIN_X + BALL_SIZE_D2 + 1;
            velocity_x *= -1;

        }
        else if((gamestate.balls[i].currentCenterX + BALL_SIZE_D2) >= ARENA_MAX_X){
            gamestate.balls[i].currentCenterX = ARENA_MAX_X - BALL_SIZE_D2 - 1;
            velocity_x *= -1;
        }

        G8RTOS_SignalSemaphore(&gamestate_sem);

        OS_Sleep(30);

    }

}


// End of game for the host
void EndOfGameHost(){

    // Wait for all semaphores to be released
    G8RTOS_WaitSemaphore(&gamestate_sem);
    G8RTOS_WaitSemaphore(&net_sem);
    G8RTOS_WaitSemaphore(&LCD_SEM);

    // Kill all other threads
    G8RTOS_KillAllOtherThreads();
    G8RTOS_AddThread(IdleThread, 255, "Idle");

    // Re-initialize semaphores
    G8RTOS_InitSemaphore(&net_sem, 1);
    G8RTOS_InitSemaphore(&gamestate_sem, 1);
    G8RTOS_InitSemaphore(&LCD_SEM, 1);

    // Wait for button press
    buttonTap = 0;

    LCD_Clear(gamestate.players[gamestate.winner].color);

    LCD_Text(MAX_SCREEN_X > 2, MAX_SCREEN_Y >> 2, "PRESS BUTTON TO START GAME", LCD_WHITE);
    while(!buttonTap);

    // Send START_GAME
    uint8_t start = START_GAME;
    SendData((unsigned char *)&start, HOST_IP_ADDR, 1);

    gamestate.LEDScores[0] = 0;
    gamestate.LEDScores[1] = 0;
    gamestate.gameDone = 0;
    gamestate.numberOfBalls = 0;
    gamestate.players[0].currentCenter = 160;
    gamestate.players[1].currentCenter = 160;
    for (int i = 0; i < MAX_NUM_OF_BALLS; i++){
        gamestate.balls[i].color = LCD_WHITE;
        gamestate.balls[i].alive = false;
        gamestate.balls[i].currentCenterX = -10;
        gamestate.balls[i].currentCenterY = -10;
    }
    LCD_Clear(LCD_BLACK);

    for(int i = ARENA_MIN_Y; i <= ARENA_MAX_Y; i++){
        LCD_SetPoint(ARENA_MAX_X, i, LCD_GRAY);
        LCD_SetPoint(ARENA_MIN_X, i, LCD_GRAY);
    }

    // draw the paddles
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, ARENA_MIN_Y, ARENA_MIN_Y+ (PADDLE_WID), LCD_RED);
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, 235 , 239, LCD_BLUE);

    char str[2];
    sprintf(str, "%d", gamestate.overallScores[0]);
    LCD_Text(0, MAX_SCREEN_Y-15, (uint8_t *)str, LCD_BLUE);
    sprintf(str, "%d", gamestate.overallScores[1]);
    LCD_Text(310, 0, (uint8_t *)str, LCD_RED);

    // Add all threads back and restart game variables
    G8RTOS_AddThread(&SendDataToClient, 10, "SendDataToClient");
    G8RTOS_AddThread(&ReceiveDataFromClient, 10, "ReceiveDataFromClient");
    G8RTOS_AddThread(&GenerateBall, 10, "GENERATE_BALL");
    G8RTOS_AddThread(&DrawObjects, 10, "DRAW_OBJECTS");
    G8RTOS_AddThread(&ReadJoystickHost, 10, "READ_JOYSTICK_HOST");
    G8RTOS_AddThread(&MoveLEDs, 10, "MoVE_LEDS");

    G8RTOS_KillSelf();
}

/*********************************************** Host Threads *********************************************************************/


/*********************************************** Client Threads *********************************************************************/

// Thread for client to join game
void JoinGame(){
    initCC3100(Client);

    // LED indicator
    initLEDs();
    turnLedOn(LED2);

    G8RTOS_InitSemaphore(&net_sem, 1);
    G8RTOS_InitSemaphore(&gamestate_sem, 1);
    G8RTOS_InitSemaphore(&LCD_SEM, 1);


    gamestate.players[1].currentCenter = 160;
    gamestate.players[0].currentCenter = 160;
    gamestate.players[0].color = LCD_BLUE;
    gamestate.players[1].color = LCD_RED;
    gamestate.players[0].position = BOTTOM;
    gamestate.players[1].position = TOP;

    gamestate.LEDScores[0] = 0;
    gamestate.LEDScores[1] = 0;


    // Set initial SpecificPlayerInfo_t strict attributes (you can get the IP address by calling getLocalIP()
    clientInfo.IP_address = getLocalIP();
    char str[15];
    sprintf(str, "%d.%d.%d.%d", (clientInfo.IP_address & 0xFF000000) >> 24, (clientInfo.IP_address & 0xFF0000) >> 16, (clientInfo.IP_address & 0xFF00) >> 8, (clientInfo.IP_address & 0xFF));
    LCD_Text(0, 0, (uint8_t*)&str, LCD_WHITE);

    LCD_Text(MAX_SCREEN_X > 2, MAX_SCREEN_Y >> 2, "Waiting For Host Connection", LCD_WHITE);

    // Wait for ACK
    uint8_t resp = 0;
    while (resp != ACK){
        // Send clientSpecificInfo to host
        OS_Sleep(50);
        G8RTOS_WaitSemaphore(&net_sem);
        SendData((unsigned char *)&clientInfo, HOST_IP_ADDR, sizeof(clientInfo));
        G8RTOS_SignalSemaphore(&net_sem);
        OS_Sleep(50);

        G8RTOS_WaitSemaphore(&net_sem);
        ReceiveData((uint8_t *)&resp, 1);
        G8RTOS_SignalSemaphore(&net_sem);
        OS_Sleep(50);
    }

    // Send ACK
    G8RTOS_WaitSemaphore(&net_sem);
    SendData((unsigned char *)&resp, HOST_IP_ADDR, 1);
    G8RTOS_SignalSemaphore(&net_sem);

    // Handshake complete
    turnLedOn(LED1);

    // Wait for START_GAME

    resp = 0;
    while (resp != START_GAME){
//        G8RTOS_WaitSemaphore(&net_sem);
        ReceiveData((uint8_t *)&resp, 1);
//        G8RTOS_SignalSemaphore(&net_sem);
        OS_Sleep(20);
    }


    G8RTOS_WaitSemaphore(&LCD_SEM);

    LCD_Clear(LCD_BLACK);

    sprintf(str, "%d", gamestate.overallScores[0]);
    LCD_Text(0, MAX_SCREEN_Y-15, (uint8_t *)str, LCD_BLUE);
    sprintf(str, "%d", gamestate.overallScores[1]);
    LCD_Text(310, 0, (uint8_t *)str, LCD_RED);

    for(int i = ARENA_MIN_Y; i <= ARENA_MAX_Y; i++){
        LCD_SetPoint(ARENA_MAX_X, i, LCD_GRAY);
        LCD_SetPoint(ARENA_MIN_X, i, LCD_GRAY);
    }

    // draw the paddles
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, ARENA_MIN_Y, ARENA_MIN_Y+ (PADDLE_WID), LCD_RED);
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, 235 , 239, LCD_BLUE);
    G8RTOS_SignalSemaphore(&LCD_SEM);


    G8RTOS_AddThread(&IdleThread, 255, "Idle");
    G8RTOS_AddThread(&ReceiveDataFromHost, 25, "ReceiveDataFromHost");
    G8RTOS_AddThread(&SendDataToHost, 25, "SendDataToHost");
    G8RTOS_AddThread(&DrawObjects, 25, "DRAW_OBJECTS");
    G8RTOS_AddThread(&ReadJoystickClient, 25, "READ_JOYSTICK_Client");
    G8RTOS_AddThread(&MoveLEDs, 25, "MoVE_LEDS");

    G8RTOS_KillSelf();
}


// Thread that receives game state packets from host
void ReceiveDataFromHost(){
    while(1) {
        int32_t retVal = NOTHING_RECEIVED;
        GameState_t packet;

        // Wait for data
        while (retVal == NOTHING_RECEIVED){
            G8RTOS_WaitSemaphore(&net_sem);
            retVal = ReceiveData((uint8_t *)&packet, sizeof(GameState_t));
            G8RTOS_SignalSemaphore(&net_sem);
            OS_Sleep(1);
        }

        // Update gamestate
        G8RTOS_WaitSemaphore(&gamestate_sem);
        gamestate = packet;
        G8RTOS_SignalSemaphore(&gamestate_sem);

        // Check if game is done
        if (gamestate.gameDone){
            if(gamestate.LEDScores[0] == 16){
                LP3943_LedModeSet(BLUE, 0xFFFF);
            }
            else{
                LP3943_LedModeSet(RED,0xFFFF);
            }
            G8RTOS_AddThread(&EndOfGameClient, 25, "EndOfGameClient");
        }

        OS_Sleep(2);
    }
}


// Thread that sends UDP packets to host
void SendDataToHost(){
    SpecificPlayerInfo_t packet = clientInfo;
    while(1) {
        packet = clientInfo;
        G8RTOS_WaitSemaphore(&net_sem);
        SendData((unsigned char *)&packet, HOST_IP_ADDR, sizeof(packet));
        G8RTOS_SignalSemaphore(&net_sem);

        OS_Sleep(5);
    }
}

// Thread to read client's joystick

void ReadJoystickClient(){
    int16_t xbias, ybias;
    int16_t xpos, ypos;
    GetJoystickCoordinates(&xbias, &ybias);

    while(1){
        GetJoystickCoordinates(&xpos, &ypos);
        xpos = xpos - xbias;
        clientInfo.displacement = xpos;
        OS_Sleep(10);
    }
}

// End of game for the client
void EndOfGameClient(){


    // Wait for all semaphores to be released
    G8RTOS_WaitSemaphore(&gamestate_sem);
    G8RTOS_WaitSemaphore(&net_sem);
    G8RTOS_WaitSemaphore(&LCD_SEM);

    G8RTOS_KillAllOtherThreads();

    LCD_Clear(gamestate.players[gamestate.winner].color);

    // Re-initialize semaphores
    G8RTOS_InitSemaphore(&net_sem, 1);
    G8RTOS_InitSemaphore(&gamestate_sem, 1);
    G8RTOS_InitSemaphore(&LCD_SEM, 1);

    // Wait for restart
    uint8_t resp = 0;
    while (resp != START_GAME){
        ReceiveData((uint8_t *)&resp, 1);
        OS_Sleep(20);
    }

    resp = ACK;
        SendData((unsigned char *)&resp, HOST_IP_ADDR, 1);

    gamestate.LEDScores[0] = 0;
    gamestate.LEDScores[1] = 0;
    gamestate.gameDone = 0;
    gamestate.numberOfBalls = 0;
    gamestate.players[0].currentCenter = 160;
    gamestate.players[1].currentCenter = 160;
    for (int i = 0; i < MAX_NUM_OF_BALLS; i++){
        gamestate.balls[i].color = LCD_WHITE;
        gamestate.balls[i].alive = false;
        gamestate.balls[i].currentCenterX = -10;
        gamestate.balls[i].currentCenterY = -10;
    }

    LCD_Clear(LCD_BLACK);
    char str[2];
    sprintf(str, "%d", gamestate.overallScores[0]);
    LCD_Text(0, MAX_SCREEN_Y-15, (uint8_t *)str, LCD_BLUE);
    sprintf(str, "%d", gamestate.overallScores[1]);
    LCD_Text(310, 0, (uint8_t *)str, LCD_RED);


    for(int i = ARENA_MIN_Y; i <= ARENA_MAX_Y; i++){
        LCD_SetPoint(ARENA_MAX_X, i, LCD_GRAY);
        LCD_SetPoint(ARENA_MIN_X, i, LCD_GRAY);
    }

    // draw the paddles
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, ARENA_MIN_Y, ARENA_MIN_Y+ (PADDLE_WID), LCD_RED);
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, 235 , 239, LCD_BLUE);

    // Add all threads back and restart game variables
    G8RTOS_AddThread(&ReceiveDataFromHost, 25, "ReceiveDataFromHost");
    G8RTOS_AddThread(&SendDataToHost, 25, "SendDataToHost");
    G8RTOS_AddThread(&DrawObjects, 25, "DRAW_OBJECTS");
    G8RTOS_AddThread(&ReadJoystickClient, 25, "READ_JOYSTICK_Client");
    G8RTOS_AddThread(&MoveLEDs, 25, "MoVE_LEDS");
    G8RTOS_AddThread(&IdleThread, 255, "Idle");

    G8RTOS_KillSelf();
}

/*********************************************** Client Threads *********************************************************************/


/*********************************************** Common Threads *********************************************************************/

void DrawObjects(){
    PrevBall_t prevBalls[MAX_NUM_OF_BALLS];
    PrevPlayer_t prevPlayers[MAX_NUM_OF_PLAYERS];
    int i;
    prevPlayers[0].Center = 160;
    prevPlayers[1].Center = 160;
    while(1){

        G8RTOS_WaitSemaphore(&gamestate_sem);
        for(i = 0; i < MAX_NUM_OF_BALLS; i++){
            if((prevBalls[i].CenterY >= ARENA_MAX_Y + BALL_SIZE) || (prevBalls[i].CenterY <= ARENA_MIN_Y - BALL_SIZE)){
            UpdateBallOnScreen(&prevBalls[i], &gamestate.balls[i], LCD_WHITE);
          }
          else{
              UpdateBallOnScreen(&prevBalls[i], &gamestate.balls[i], gamestate.balls[i].color);
          }

          prevBalls[i].CenterX = gamestate.balls[i].currentCenterX;
          prevBalls[i].CenterY = gamestate.balls[i].currentCenterY;

        }

        for(i = 0; i < MAX_NUM_OF_PLAYERS; i++){
            UpdatePlayerOnScreen(&prevPlayers[i], &gamestate.players[i]);
            prevPlayers[i].Center = gamestate.players[i].currentCenter;
        }


        G8RTOS_SignalSemaphore(&gamestate_sem);

        OS_Sleep(20);

    }

}

void MoveLEDs(){
    uint16_t blue, red;
    int i;
    while(1){
        blue = 0;
        red = 0;
        G8RTOS_WaitSemaphore(&gamestate_sem);
        for(i = 0; i < gamestate.LEDScores[0]; i++){
            blue |= (1 << i);
        }

        for(i = 0; i < gamestate.LEDScores[1]; i++){
            red |= (BITF >> i);
        }

        G8RTOS_SignalSemaphore(&gamestate_sem);

        LP3943_LedModeSet(BLUE, 0);
        LP3943_LedModeSet(RED, 0);

        LP3943_LedModeSet(BLUE, blue);
        LP3943_LedModeSet(RED,red);

        OS_Sleep(20);
    }
}

void UpdateBallOnScreen(PrevBall_t * previousBall, Ball_t * currentBall, uint16_t outColor){
    // delete the prev ball
    G8RTOS_WaitSemaphore(&LCD_SEM);
    int16_t xStart, xEnd, yStart, yEnd;

    xStart = previousBall -> CenterX - BALL_SIZE_D2;
    xEnd = previousBall -> CenterX + BALL_SIZE_D2;
    yStart = previousBall -> CenterY - BALL_SIZE_D2;
    yEnd = previousBall -> CenterY + BALL_SIZE_D2;


    LCD_DrawRectangle(xStart, xEnd, yStart, yEnd, LCD_BLACK);

    // Draw the next BALL

    xStart = currentBall-> currentCenterX - BALL_SIZE_D2;
    xEnd = currentBall -> currentCenterX + BALL_SIZE_D2;
    yStart = currentBall -> currentCenterY -BALL_SIZE_D2;
    yEnd = currentBall -> currentCenterY + BALL_SIZE_D2;

    LCD_DrawRectangle(xStart, xEnd, yStart, yEnd, outColor);


    G8RTOS_SignalSemaphore(&LCD_SEM);
}

void UpdatePlayerOnScreen(PrevPlayer_t * prevPlayerIn, GeneralPlayerInfo_t * outPlayer){
    // delete the prev player
    G8RTOS_WaitSemaphore(&LCD_SEM);
    int16_t xStart, xEnd, yStart, yEnd;

    if((prevPlayerIn -> Center) > (outPlayer -> currentCenter)){
        xStart = outPlayer -> currentCenter + PADDLE_LEN_D2 ;
        xEnd = prevPlayerIn -> Center  +  PADDLE_LEN_D2;
    }
    else if((prevPlayerIn -> Center) < (outPlayer -> currentCenter)){
        xStart = prevPlayerIn -> Center - PADDLE_LEN_D2 ;
        xEnd = outPlayer -> currentCenter -  PADDLE_LEN_D2;
    }
    else{
        xStart = -10;
        xEnd = -10;
    }

    if(outPlayer->position == TOP){
        yStart = ARENA_MIN_Y;
        yEnd = ARENA_MIN_Y + 4;
    }
    else{
        yStart = ARENA_MAX_Y - 4;
        yEnd = ARENA_MAX_Y;

    }

    LCD_DrawRectangle(xStart, xEnd, yStart, yEnd, LCD_BLACK);

    // Draw the next player


    if((prevPlayerIn -> Center) > (outPlayer -> currentCenter)){
        xStart = outPlayer -> currentCenter - PADDLE_LEN_D2 ;
        xEnd = prevPlayerIn -> Center  -  PADDLE_LEN_D2;
    }
    else if((prevPlayerIn -> Center) < (outPlayer -> currentCenter)){
        xStart = prevPlayerIn -> Center + PADDLE_LEN_D2 ;
        xEnd = outPlayer -> currentCenter +  PADDLE_LEN_D2;
    }
    else{
        xStart = -10;
        xEnd = -10;
    }


    LCD_DrawRectangle(xStart, xEnd, yStart, yEnd, outPlayer->color);


    G8RTOS_SignalSemaphore(&LCD_SEM);
}


// Idle thread
void IdleThread(){
    while(1);
}

/*********************************************** Common Threads *********************************************************************/

