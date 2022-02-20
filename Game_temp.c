/*
 * Game.c
 *
 *  Created on: Apr 23, 2021
 *      Author: sahil
 */

#include <G8RTOS_Lab5/G8RTOS.h>
#include "Game.h"
#include <stdlib.h>




GameState_t gamestate;

SpecificPlayerInfo_t clientInfo;

bool buttonTap = false;



int generate_random(int l, int r) { //this will generate random number in range l and r
      return (rand() % (r - l + 1)) + l;
}

// random function

//int generate_random(int l, int r) { //this will generate random number in range l and r
//      return (rand() % (r - l + 1)) + l;
//}


// NOTE: WILL NEED TO ADD GAMESTATE MUTEX EVERYTIME A GAMESTATE VAR IS USED

// Generate Ball checks how many active balls there are, then generates a ball;


void ButtonTap_port4(){ // set flag for a touch
    if(P4->IFG & BIT4){
        P4->IFG &= ~BIT4;
        buttonTap = true;
    }
}


void gameInit(){
    int i;

    //
    // start LCD
    G8RTOS_WaitSemaphore(&LCD_SEM);

    LCD_Clear(LCD_BLACK);

    LCD_Text(MAX_SCREEN_X > 2, MAX_SCREEN_Y >> 2, "PRESS BUTTON TO START GAME", LCD_WHITE);
    while(!buttonTap);


    LCD_Clear(LCD_BLACK);

    LCD_Text(0, MAX_SCREEN_Y >> 2, "0", LCD_BLUE);
    LCD_Text(300, MAX_SCREEN_Y >> 2, "0", LCD_RED);




    for(i = ARENA_MIN_Y; i <= ARENA_MAX_Y; i++){
        LCD_SetPoint(ARENA_MAX_X, i, LCD_GRAY);
        LCD_SetPoint(ARENA_MIN_X, i, LCD_GRAY);
    }


    // draw the paddles

    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, ARENA_MIN_Y, ARENA_MIN_Y+ (PADDLE_WID), LCD_RED);
    LCD_DrawRectangle(160 - PADDLE_LEN_D2, 160 + PADDLE_LEN_D2, 235 , 239, LCD_BLUE);

    gamestate.players[1].currentCenter = 160;
    gamestate.players[0].currentCenter = 160;
    gamestate.players[0].color = LCD_BLUE;
    gamestate.players[1].color = LCD_RED;
    gamestate.players[0].position = BOTTOM;
    gamestate.players[1].position = TOP;

    gamestate.LEDScores[0] = 0;
    gamestate.LEDScores[1] = 0;



    G8RTOS_SignalSemaphore(&LCD_SEM);


    G8RTOS_AddThread(&GenerateBall, 25, "GENERATE_BALL");
    G8RTOS_AddThread(&DrawObjects, 10, "DRAW_OBJECTS");
    G8RTOS_AddThread(&ReadJoystickHost, 10, "READ_JOYSTICK_HOST");
    G8RTOS_AddThread(&MoveLEDs, 10, "MoVE_LEDS");
    G8RTOS_KillSelf();

}


void GenerateBall(){
    while(1){
        // wait for gamestate semaphore;
        G8RTOS_WaitSemaphore(&gamestate_sem); // wait for the semaphore;

        if(gamestate.numberOfBalls < MAX_NUM_OF_BALLS){
            G8RTOS_AddThread(&MoveBall, 20, "BALL");
        }

        G8RTOS_SignalSemaphore(&gamestate_sem); // release semaphore;

        OS_Sleep(1000 * (gamestate.numberOfBalls + 1));
    }
}

void MoveBall(){
    int i; // counter

    G8RTOS_WaitSemaphore(&gamestate_sem); // need semaphore to do all the initializatios

    int16_t velocity_x = generate_random(-10, 10); // randomize later
    int16_t velocity_y = generate_random(-10, 10); // randomize later

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

                // if one player wins, end game

                if (gamestate.LEDScores[0] == 16){
                    gamestate.LEDScores[1] = 0;
                   // G8RTOS_AddThread(&EndOfGameHost, 1, "END_OF_GAME");
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

                // add host end game

                if (gamestate.LEDScores[1] == 16){
                   // G8RTOS_AddThread(&EndOfGameHost, 1, "END_OF_GAME");
                    gamestate.LEDScores[0] = 0;
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

        OS_Sleep(35);

    }

}



void DrawObjects(){
    PrevBall_t prevBalls[MAX_NUM_OF_BALLS];
    PrevPlayer_t prevPlayers[MAX_NUM_OF_PLAYERS];
    int i;
    prevPlayers[0].Center = 160;
    prevPlayers[1].Center = 160;
    while(1){

        G8RTOS_WaitSemaphore(&gamestate_sem);
        for(i = 0; i < MAX_NUM_OF_BALLS; i++){
          if((prevBalls[i].CenterY >= ARENA_MAX_Y) || (prevBalls[i].CenterY <= ARENA_MIN_Y)){
            UpdateBallOnScreen(&prevBalls[i], &gamestate.balls[i], LCD_WHITE);
            gamestate.balls[i].color = LCD_WHITE;
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
        if(xpos >= 200 && (gamestate.players[0].currentCenter - PADDLE_LEN_D2 >= ARENA_MIN_X)){
            gamestate.players[0].currentCenter -= 1;
        }
        else if(xpos <= -200 && (gamestate.players[0].currentCenter + PADDLE_LEN_D2 <= ARENA_MAX_X)){
            gamestate.players[0].currentCenter += 1;
        }

        G8RTOS_SignalSemaphore(&gamestate_sem);

    }


}

void ReadJoystickClient(){
    int16_t xbias, ybias;
    int16_t xpos, ypos;
    GetJoystickCoordinates(&xbias, &ybias);

    while(1){
        GetJoystickCoordinates(&xpos, &ypos);
        xpos = xpos - xbias;
        clientInfo.displacement = xpos;
    }
}

