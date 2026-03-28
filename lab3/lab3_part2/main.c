// Include FreeRTOS Libraries
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Include xilinx Libraries
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"

// Other miscellaneous libraries
#include <stdlib.h>
#include <sys/_intsup.h>
#include <time.h>
#include <stdio.h>
#include <xil_types.h>
#include "pmodkypd.h"
#include "sleep.h"
#include "PmodOLED.h"
#include "OLEDControllerCustom.h"


#define BTN_DEVICE_ID  XPAR_GPIO_INPUTS_BASEADDR
#define KYPD_DEVICE_ID XPAR_GPIO_KEYPAD_BASEADDR
#define KYPD_BASE_ADDR XPAR_GPIO_KEYPAD_BASEADDR
#define BTN_CHANNEL    1
#define SSD_DEVICE_ID       XPAR_GPIO_SSD_BASEADDR


#define FRAME_DELAY 50000

// keypad key table
#define DEFAULT_KEYTABLE 	"0FED789C456B123A"

// Declaring the devices
XGpio btnInst;
PmodOLED oledDevice;
PmodKYPD 	KYPDInst;
XGpio       SSDInst;

// Function prototypes
void InitializeKeypad();
void initializeScreen();
static void keypadTask( void *pvParameters );
static void oledTask( void *pvParameters );
static void buttonTask( void *pvParameters );
static void vDisplayTask(); 
int grphClampXco(int xco);
int grphClampYco(int yco);
int grphAbs(int foo);
void OLED_DrawLineTo(PmodOLED *InstancePtr, int xco, int yco);
void OLED_getPos(PmodOLED *InstancePtr, int *pxco, int *pyco);
void drawPlayer(u8 playerX, u8 playerY);
u32 SSD_decode(u8 key_value, u8 cathode);

const u8 orientation = 0x1; // Set up for Normal PmodOLED(false) vs normal
                            // Onboard OLED(true)
const u8 invert = 0x1; // true = whitebackground/black letters
                       // false = black background /white letters
u8 keypad_val = 'x';
u8 playerX, playerY, playerWidth, playerLength;
int score, lives, win, goalTime;

typedef struct {
    u8 x, y;
    u8 length, width;
}obstacle;

obstacle ob1, ob2, ob3, ob4;

int main()
{
	int status = 0;
	// Initialize Devices
	InitializeKeypad();

	 // orientation: 0 is usually normal, invert: 0 = normal colors
    OLED_Begin(&oledDevice,
               XPAR_GPIO_OLED_BASEADDR,
               XPAR_SPI_OLED_BASEADDR,
               orientation,
               invert);

	// Buttons
	status = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for buttons failed.\r\n");
		return XST_FAILURE;
	}

    // SSD
    status = XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization for SSD has failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&SSDInst, 1, 0x00);


	xil_printf("Initialization Complete, System Ready!\n");

    xTaskCreate( keypadTask					/* The function that implements the task. */
			   , "keypad task"				/* Text name for the task, provided to assist debugging only. */
			   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
			   , NULL						/* The task parameter is not used, so set to NULL. */
			   , tskIDLE_PRIORITY			/* The task runs at the idle priority. */
			   , NULL
			   );

	xTaskCreate( oledTask					/* The function that implements the task. */
			   , "screen task"				/* Text name for the task, provided to assist debugging only. */
			   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
			   , NULL						/* The task parameter is not used, so set to NULL. */
			   , tskIDLE_PRIORITY			/* The task runs at the idle priority. */
			   , NULL
			   );

	xTaskCreate( buttonTask
			   , "button task"
			   , configMINIMAL_STACK_SIZE
			   , NULL
			   , tskIDLE_PRIORITY
			   , NULL
			   );

    xTaskCreate(vDisplayTask                    /* The function that implements the task. */
                , "display task"			       /* Text name for the task, provided to assist debugging only. */
                , configMINIMAL_STACK_SIZE 	  /* The stack allocated to the task. */
                , NULL 						  /* The task parameter is not used, so set to NULL. */
                , tskIDLE_PRIORITY			    /* The task runs at the idle priority. */
                , NULL
                ); 

	vTaskStartScheduler();


   while(1);

   return 0;
}


void InitializeKeypad()
{
   KYPD_begin(&KYPDInst, KYPD_BASE_ADDR);
   KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
}

void initializePlayer() {
    playerX = 30;
    playerY = 5;
    playerWidth = 5, playerLength = 5;
}

void drawPlayer(u8 targetx, u8 targety)
{
	u8 width = playerWidth;
    u8 length = playerLength;
    
	OLED_MoveTo(&oledDevice, targetx, targety);
	OLED_RectangleTo(&oledDevice, targetx + length, targety + width);
}

void updatePlayer() {
    if ((keypad_val >= 48 && keypad_val <= 57) || (keypad_val >= 65 && keypad_val <= 70)) {
        playerY--;
    } else {
        playerY++;
    }

    if (playerY < 1) {
        playerY = 1;
    }
    if (playerY + playerLength > OledRowMax - 1) {
        playerY = OledRowMax - playerLength - 1;
    }
}

void initializeObstacles() {
    ob1.x = 34, ob1.y = 22;
    ob1.length = 5, ob1.width = 5;

    ob2.x = 67, ob2.y = 15;
    ob2.length = 5, ob2.width = 5;

    ob3.x = 99, ob3.y = 2;
    ob3.length = 5, ob3.width = 5;

    ob4.x = 11, ob4.y = 17;
    ob4.length = 5, ob4.width = 5;
}

void drawObstacles() {
    u8 x = ob1.x;
    u8 y = ob1.y;
    u8 width = ob1.width;
    u8 length = ob1.length;
	OLED_MoveTo(&oledDevice, x, y);
    OLED_RectangleTo(&oledDevice, x + length, y + width);

    x = ob2.x;
    y = ob2.y;
    width = ob2.width;
    length = ob2.length;
	OLED_MoveTo(&oledDevice, x, y);
    OLED_RectangleTo(&oledDevice, x + length, y + width);

    x = ob3.x;
    y = ob3.y;
    width = ob3.width;
    length = ob3.length;
	OLED_MoveTo(&oledDevice, x, y);
    OLED_RectangleTo(&oledDevice, x + length, y + width);

    x = ob4.x;
    y = ob4.y;
    width = ob4.width;
    length = ob4.length;
	OLED_MoveTo(&oledDevice, x, y);
    OLED_RectangleTo(&oledDevice, x + length, y + width);
}

void updateObstacles() {
    ob1.x -= 2;
    ob2.x -= 2;
    ob3.x -= 2;
    ob4.x -= 2;

    if (ob1.x > OledColMax-1) {
        ob1.x = OledColMax-1;
        ob1.y = rand() % (OledRowMax - ob1.length);
    }

    if (ob2.x > OledColMax-1) {
        ob2.x = OledColMax-1;
        ob2.y = rand() % (OledRowMax - ob2.length);
    }

    if (ob3.x > OledColMax-1) {
        ob3.x = OledColMax-1;
        ob3.y = rand() % (OledRowMax - ob3.length);
    }

    if (ob4.x > OledColMax-1) {
        ob4.x = OledColMax-1;
        ob4.y = rand() % (OledRowMax - ob4.length);
    }
}

void startGame() {
    initializePlayer();
    initializeObstacles();
    score = 30;
    lives = 1;
    win = 0;
    goalTime = xTaskGetTickCount() + score * 100;
}

void updateScore() {
    if (lives < 1) { return; }
    else if (score <= 0) { 
        win = 1;
        return;
    }
    u8 goalTimeS = goalTime / 100;
    u8 currentTimeS = xTaskGetTickCount() / 100;
    score = goalTimeS - currentTimeS;
}

int checkHitboxes(u8 x, u8 y, u8 length, u8 width) {
    int overlapX = x + width >= playerX && playerX + playerWidth >= x;
    int overlapY = y + length >= playerY && playerY + playerLength >= y;
    return overlapX & overlapY;
}

void checkCollisions() {
    u8 hit = 0;
    hit += checkHitboxes(ob1.x, ob1.y, ob1.length, ob1.width);
    hit += checkHitboxes(ob2.x, ob2.y, ob2.length, ob2.width);
    hit += checkHitboxes(ob3.x, ob3.y, ob3.length, ob3.width);
    hit += checkHitboxes(ob4.x, ob4.y, ob4.length, ob4.width);
    if (hit) {
        lives = 0;
    }
}

static void keypadTask( void *pvParameters )
{
   u16 keystate;
   XStatus status, last_status = KYPD_NO_KEY;
   u8 new_key = 'x';

   const TickType_t xDelay = 25 / portTICK_RATE_MS;

   xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");
   while (1) {
	  // Capture state of the keypad
	  keystate = KYPD_getKeyStates(&KYPDInst);

	  // Determine which single key is pressed, if any
	  // if a key is pressed, store the value of the new key in new_key
	  status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);

	  // Print key detect if a new key is pressed or if status has changed
	  if (status == KYPD_SINGLE_KEY){
	  } else if (status == KYPD_MULTI_KEY && status != last_status){
		 xil_printf("Error: Multiple keys pressed\r\n");
	  } else if (status == KYPD_NO_KEY && last_status == KYPD_NO_KEY ){
		  new_key = 'x';
	  }

	  last_status = status;
	  keypad_val = new_key;
	  
	  vTaskDelay(xDelay); // Scanning Delay
   }
}

// This function is hard coded to translate key value codes to their binary representation
u32 SSD_decode(u8 key_value, u8 cathode)
{
    u32 result;

	// key_value represents the code of the pressed key
	switch(key_value){ // Handles the coding of the 7-seg display
		case 48: result = 0b00111111; break; // 0
        case 49: result = 0b00110000; break; // 1
        case 50: result = 0b01011011; break; // 2
        case 51: result = 0b01111001; break; // 3
        case 52: result = 0b01110100; break; // 4
        case 53: result = 0b01101101; break; // 5
        case 54: result = 0b01101111; break; // 6
        case 55: result = 0b00111000; break; // 7
        case 56: result = 0b01111111; break; // 8
        case 57: result = 0b01111100; break; // 9
        default: result = 0b00000000; break; // default case - all seven segments are OFF
    }

	// cathode handles which display is active (left or right)
	// by setting the MSB to 1 or 0
    if(cathode==0){
            return result;
    } else {
            return result | 0b10000000;
	}
}

static void vDisplayTask() 
{
    u32 ssd_value=0;
    u8 ones_digit = 'x';
    u8 tens_digit = 'x';
    TickType_t xDelay = 1;

    while(1){
        ones_digit = (score % 10) + 0x30;
        tens_digit = ((score % 100) / 10) + 0x30;

        ssd_value = SSD_decode(ones_digit, 1);
        XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);
        vTaskDelay(xDelay);
        ssd_value = SSD_decode(tens_digit, 0);
        XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);
        vTaskDelay(xDelay);
    }
}

static void oledTask( void *pvParameters )
{
	u8 buttonVal = 0;
	char temp[10];
	xil_printf("UART and SPI opened for PmodOLED Demo\n");
	OLED_SetDrawMode(&oledDevice, 0);
	// Turn automatic updating off
	OLED_SetCharUpdate(&oledDevice, 0);
    
    startGame();

	while(1){

        updatePlayer();
        updateObstacles();
        updateScore();
        checkCollisions();

		buttonVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);      
        if (lives == 0){
			OLED_ClearBuffer(&oledDevice);
			OLED_SetCursor(&oledDevice, 0, 1);
			OLED_PutString(&oledDevice, "Game Over");
			OLED_Update(&oledDevice);
        } else if (win == 1) {
            OLED_ClearBuffer(&oledDevice);
			OLED_SetCursor(&oledDevice, 0, 1);
			OLED_PutString(&oledDevice, "You win!");
			OLED_Update(&oledDevice);
        } else {
            if (buttonVal == 4){
				OLED_ClearBuffer(&oledDevice);
				OLED_SetCursor(&oledDevice, 0, 1);
				u32 ticks = xTaskGetTickCount();
				ticks = ticks / 100;
				sprintf(temp, "time: %lu", ticks);
				OLED_PutString(&oledDevice, temp);
				OLED_Update(&oledDevice);
			} else {
			    drawPlayer(playerX, playerY);
                drawObstacles();
				OLED_Update(&oledDevice);
				usleep(FRAME_DELAY);
				OLED_ClearBuffer(&oledDevice);
			}
		}
	}
}


static void buttonTask( void *pvParameters )
{
	u8 buttonVal = 0;
	while(1){
		buttonVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);
		if (buttonVal == 8){
			xil_printf("reset\n");
			startGame();
		}
		vTaskDelay(10);
	}
}
