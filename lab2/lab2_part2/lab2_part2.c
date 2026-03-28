/*
 * Lab 1, Part 2 - Seven-Segment Display & Keypad
 *
 * ECE-315 WINTER 2025 - COMPUTER INTERFACING
 * Created on: February 5, 2021
 * Modified on: July 26, 2023
 * Modified on: January 20, 2025
 * Author(s):  Shyama Gandhi, Antonio Andara Lara
 *
 * Summary:
 * 1) Declare & initialize the 7-seg display (SSD).
 * 2) Use xDelay to alternate between two digits fast enough to prevent flicker.
 * 3) Output pressed keypad digits on both SSD digits: current_key on right, previous_key on left.
 * 4) Print status changes and experiment with xDelay to find minimum flicker-free frequency.
 *
 * Deliverables:
 * - Demonstrate correct display of current and previous keys with no flicker.
 * - Print to the SDK terminal every time that theh variable `status` changes.
 */


// Include FreeRTOS Libraries
#include <FreeRTOS.h>
#include <portmacro.h>
#include <stdint.h>
#include <task.h>
#include <queue.h>

// Include xilinx Libraries
#include <xil_types.h>
#include <xparameters.h>
#include <xgpio.h>
#include <xscugic.h>
#include <xil_exception.h>
#include <xil_printf.h>
#include <sleep.h>
#include <xil_cache.h>
#include <xstatus.h>
#include "xuartps.h"

// Other miscellaneous libraries
#include "pmodkypd.h"
#include "rgb_led.h"
#include <portmacro.h>
#include <projdefs.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <xil_io.h>
#include <xil_printf.h>
#include <xuartps_hw.h>


// Device ID declarations
#define KYPD_DEVICE_ID   	XPAR_GPIO_KYPD_BASEADDR
/*************************** Enter your code here ****************************/
// TODO: Define the seven-segment display (SSD) base address.
#define SSD_DEVICE_ID       XPAR_GPIO_SSD_BASEADDR
// #define LED_DEVICE_ID       XPAR_GPIO_LEDS_BASEADDR
#define BUTTON_DEVICE_ID    XPAR_GPIO_INPUTS_BASEADDR
/*****************************************************************************/

// keypad key table
#define DEFAULT_KEYTABLE 	"0FED789C456B123A"

// ======================================================
// Configuration
// ======================================================
#define UART_BASEADDR 	XPAR_UART1_BASEADDR
#define RX_QUEUE_LEN     512
#define CMD_QUEUE_LEN     16
#define TX_QUEUE_LEN     512

#define POLL_DELAY_MS     20

// Declaring the devices
PmodKYPD 	KYPDInst;

/*************************** Enter your code here ****************************/
// TODO: Declare the seven-segment display peripheral here.
XGpio       SSDInst;
XGpio       rgbLedInst;
XGpio       pushButtonInst;
/*****************************************************************************/

// Function prototypes
void InitializeKeypad();
static void vKeypadTask( void *pvParameters );
static void vRgbTask( void *pvParameters );
static void vButtonsTask();
static void vDisplayTask(); 
static void UART_RX_Task(void *pvParameters);
static void UART_TX_Task(void *pvParameters);
static void CLI_Task(void *pvParameters);
u32 SSD_decode(u8 key_value, u8 cathode);

QueueHandle_t keypadToSSDQueue;
QueueHandle_t buttonsToLEDQueue;
QueueHandle_t uartToSSDQueue;
QueueHandle_t uartToLEDIncQueue;  
QueueHandle_t uartToLEDClrQueue;

// ======================================================
// FreeRTOS objects
// ======================================================
static QueueHandle_t q_rx_byte = NULL;   // uint8_t
static QueueHandle_t q_tx      = NULL;   // char

// ======================================================
// UART instance
// ======================================================
static XUartPs UartPs;

typedef enum {
  CMD_NONE,
  CMD_SSD_1 = '1',
  CMD_SSD_2 = '2',
  CMD_SSD_3 = '3',
  CMD_SSD_4 = '4',
  CMD_SSD_5 = '5',
  CMD_SSD_6 = '6',
  CMD_SSD_7 = '7',
  CMD_SSD_8 = '8',
  CMD_SSD_9 = '9',
  CMD_SSD_0 = '0',
  CMD_SSD_A = 'A',
  CMD_SSD_B = 'B',
  CMD_SSD_C = 'C',
  CMD_SSD_D = 'D',
  CMD_SSD_E = 'E',
  CMD_SSD_F = 'F',
  CMD_RGB_HIGH = 'h',
  CMD_RGB_LOW = 'l',
  CMD_RGB_RED = 'r',
  CMD_RGB_GREEN = 'g',
  CMD_RGB_BLUE = 'b',
  CMD_RGB_YELLOW = 'y',
  CMD_RGB_CYAN = 'c',
  CMD_RGB_MAGENTA = 'm',
  CMD_RGB_WHITE = 'w'
} command_type_t;

typedef enum { 
    UARTCMD_SSD, 
    UARTCMD_NONE, 
    UARTCMD_BRIGHT, 
    UARTCMD_COLOR
}uartcmd_type_t; 

typedef struct { 
    // uartcmd_type_t type; 
    uint8_t cmd; 
}uart_cmd_t;


// ======================================================
// UART helpers
// ======================================================
uint8_t receive_byte(uint8_t *out_byte);
// // void receive_string(char *buf, size_t buf_len);
static void uart_init(void);
static int uart_poll_rx(uint8_t *b);
static void uart_tx_byte(uint8_t b);


// ======================================================
// Custom UART functions
// ======================================================
void print_string(const char *str);
void flush_uart(void);

int main(void)
{
	int status;

	// Initialize keypad
	InitializeKeypad();

    // Initialize UART
    uart_init();

/*************************** Enter your code here ****************************/
	// TODO: Initialize SSD and set the GPIO direction to output.
    status = XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization for SSD has failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&SSDInst, 1, 0x00);

    status = XGpio_Initialize(&rgbLedInst, RGB_LED_BASEADDR);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization for RGB LED has failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&rgbLedInst, 1, 0x00);

    status = XGpio_Initialize(&pushButtonInst, BUTTON_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("GPIO Initialization for PUSH BUTTON has failed.\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&pushButtonInst, 1, 0x01);
/********************************************   *********************************/

	xil_printf("\n\n\nInitialization Complete, System Ready!\n");

	xTaskCreate(vKeypadTask,					/* The function that implements the task. */
				"main task", 				/* Text name for the task, provided to assist debugging only. */
				configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
				NULL, 						/* The task parameter is not used, so set to NULL. */
				tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
				NULL);

    xTaskCreate(vRgbTask,					/* The function that implements the task. */
                "rgb task", 				/* Text name for the task, provided to assist debugging only. */
                configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
                NULL, 						/* The task parameter is not used, so set to NULL. */
                tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
                NULL);

    xTaskCreate(vButtonsTask,					/* The function that implements the task. */
                "buttons task", 				/* Text name for the task, provided to assist debugging only. */
                configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
                NULL, 						/* The task parameter is not used, so set to NULL. */
                tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
                NULL);

    xTaskCreate(vDisplayTask,					/* The function that implements the task. */
                "display task", 				/* Text name for the task, provided to assist debugging only. */
                configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
                NULL, 						/* The task parameter is not used, so set to NULL. */
                tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
                NULL); 
    
    xTaskCreate(UART_RX_Task,					/* The function that implements the task. */
                "UART task", 				/* Text name for the task, provided to assist debugging only. */
                configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
                NULL, 						/* The task parameter is not used, so set to NULL. */
                tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
                NULL);
    
    xTaskCreate(UART_TX_Task, 
                "UART_TX", 
                 configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
                NULL, 						/* The task parameter is not used, so set to NULL. */
                tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
                NULL);

    xTaskCreate(CLI_Task, 
                "CLI",     
                 configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
                NULL, 						/* The task parameter is not used, so set to NULL. */
                tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
                NULL);

    keypadToSSDQueue = xQueueCreate(1, sizeof(u8));
    buttonsToLEDQueue = xQueueCreate(1, sizeof(TickType_t)); 
    uartToSSDQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(uart_cmd_t)); 
    uartToLEDIncQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(uart_cmd_t));
    uartToLEDClrQueue = xQueueCreate(CMD_QUEUE_LEN, sizeof(uart_cmd_t));
    q_rx_byte = xQueueCreate(RX_QUEUE_LEN, sizeof(uint8_t));
    q_tx = xQueueCreate(TX_QUEUE_LEN, sizeof(char));

	vTaskStartScheduler();
	while(1);
	return 0;
}


static void vKeypadTask( void *pvParameters )
{
	u16 keystate;
	XStatus status, previous_status = KYPD_NO_KEY;
	u8 new_key = 'x';
    u32 button_debounce = 20; 

    xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");
	while (1){
		// Capture state of the keypad
		keystate = KYPD_getKeyStates(&KYPDInst);

		// Determine which single key is pressed, if any
		// if a key is pressed, store the value of the new key in new_key
		status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);
		// Print key detect if a new key is pressed or if status has changed
		if (status == KYPD_SINGLE_KEY && previous_status == KYPD_NO_KEY){
			xil_printf("Key Pressed: %c\r\n", (char) new_key);

            xQueueOverwrite(keypadToSSDQueue, &new_key);
            

		} else if (status == KYPD_MULTI_KEY && status != previous_status){
			xil_printf("Error: Multiple keys pressed\r\n");
		}
		

        if (status != previous_status) {
            xil_printf("Status changed: %i\r\n", (int32_t) status);
            vTaskDelay(button_debounce);
        }

		previous_status = status;
	}
}

static void vDisplayTask() 
{
    u32 ssd_value=0;
    u8 new_key = 'x';
    u8 current_key = 'x';
    u8 previous_key = 'x';
    TickType_t xDelay = 10;

    while(1){
        if (xQueueReceive(keypadToSSDQueue, &new_key, 1) || xQueueReceive(uartToSSDQueue, &new_key, 1)) {
            previous_key = current_key;
            current_key = new_key;
        }

        ssd_value = SSD_decode(current_key, 1);
        XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);
        vTaskDelay(xDelay);
        ssd_value = SSD_decode(previous_key, 0);
        XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);
        vTaskDelay(xDelay);
    }
}


void InitializeKeypad()
{
	KYPD_begin(&KYPDInst, KYPD_DEVICE_ID);
	KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
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
        case 65: result = 0b01111110; break; // A
        case 66: result = 0b01100111; break; // B
        case 67: result = 0b00001111; break; // C
        case 68: result = 0b01110011; break; // D
        case 69: result = 0b01001111; break; // E
        case 70: result = 0b01001110; break; // F
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

#define PUSH_NO_STATE           0
#define PUSH_DECREMENT_STATE    1
#define PUSH_INCREMENT_STATE    8

static void vButtonsTask() 
{
    
    XStatus state, previous_state = PUSH_NO_STATE;
    TickType_t inputDelay = 300;
    u16 increment = 1;
    uint8_t uartCmd;
    const TickType_t xPeriod = 20;
    TickType_t xOnDelay = 10;
    
    while(1){
        state = XGpio_DiscreteRead(&pushButtonInst, 1); 
        
        if (state != previous_state){
            if (state == PUSH_DECREMENT_STATE && xOnDelay > 0) {
                xOnDelay -= increment; 
                vTaskDelay(inputDelay);
            } else if (state == PUSH_INCREMENT_STATE && (xPeriod - xOnDelay) > 0) {
                xOnDelay += increment;
                vTaskDelay(inputDelay);
            } 
            xQueueOverwrite(buttonsToLEDQueue, &xOnDelay);
        }

        else if (xQueueReceive(uartToLEDIncQueue, &uartCmd, 1)){
            if (uartCmd == PUSH_DECREMENT_STATE && xOnDelay > 0) {
                xOnDelay -= increment; 
                vTaskDelay(inputDelay);
            } else if (uartCmd == PUSH_INCREMENT_STATE && (xPeriod - xOnDelay) > 0) {
                xOnDelay += increment;
                vTaskDelay(inputDelay);
            } 
            xQueueOverwrite(buttonsToLEDQueue, &xOnDelay);
        }
    }
    
}

static void vRgbTask(void *pvParameters)
{
    uint8_t color = RGB_CYAN;
    const TickType_t xPeriod = 20;
    TickType_t xOnDelay = 10;  
    TickType_t xOffDelay = xPeriod - xOnDelay;  

    // xil_printf("\nxPeriod: %d\n", xPeriod);   

    while (1){  
        
        if (xQueueReceive(uartToLEDClrQueue, &color, 1)) {
            xil_printf("\nColor changed");
        }
        
        if (xQueueReceive(buttonsToLEDQueue,&xOnDelay,1)){
            xil_printf("\nBrightness: %d", (xOnDelay * 100) / xPeriod);
        }      

        xOffDelay = xPeriod - xOnDelay;

        if (xOnDelay != 0) {
            XGpio_DiscreteWrite(&rgbLedInst, RGB_CHANNEL, color);
            vTaskDelay(xOnDelay); 
        }
       
        XGpio_DiscreteWrite(&rgbLedInst, RGB_CHANNEL, 0);
        vTaskDelay((xOffDelay)); 
    } 

    
} 

static void UART_RX_Task(void *pvParameters) {  
  uint8_t byte;

  for (;;){
    if (uart_poll_rx(&byte)){
      xQueueSend(q_rx_byte, &byte, 0); 
    } 
    vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
  }
}

// ======================================================
// UART TX Task
// ======================================================

static void UART_TX_Task(void *pvParameters)
{

  char c;

  for (;;){
    // modified the statement(s) below to replace the blocking operation
    if (xQueueReceive(q_tx, &c, 0) == pdTRUE){
      uart_tx_byte((uint8_t)c);
    } 
    vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
  }
}


// ======================================================
// CLI Task
// ======================================================

static void CLI_Task(void *pvParameters)
{
    command_type_t op = CMD_NONE;
    uart_cmd_t cmd;

    vTaskDelay(pdMS_TO_TICKS(20));

    print_string((const char *)pvParameters);
    // print_string("\n*******************************************\n");
    print_string("CLI Menu:\n\tSSD Commands: 1; 2; 3; 4; 5; 6; 7; 8; 9; 0; A; B; C; D; E; F\n");
    print_string("\tRGB Commands: (h)igh; (l)ow; (r)ed; (g)reen; (b)lue; (y)ellow; (c)yan; (m)agenta; (w)hite\n");

    for (;;){

        print_string("\nEnter your option: ");
        receive_byte((uint8_t *)&op);

        // print_string("\n*******************************************\n");

        switch (op){
            case CMD_SSD_0: case CMD_SSD_1: case CMD_SSD_2: case CMD_SSD_3: case CMD_SSD_4: case CMD_SSD_5: case CMD_SSD_6: case CMD_SSD_7:
            case CMD_SSD_8: case CMD_SSD_9: case CMD_SSD_A: case CMD_SSD_B: case CMD_SSD_C: case CMD_SSD_D: case CMD_SSD_E: case CMD_SSD_F:
                cmd.cmd = op;
                xQueueSend(uartToSSDQueue, &cmd, 0);
                break;

            case CMD_RGB_HIGH:
                cmd.cmd = PUSH_INCREMENT_STATE;
                xQueueSend(uartToLEDIncQueue, &cmd, 0);
                break;

            case CMD_RGB_LOW:
                cmd.cmd = PUSH_DECREMENT_STATE;
                xQueueSend(uartToLEDIncQueue, &cmd, 0);
                break;

            case CMD_RGB_RED:
                cmd.cmd = RGB_RED;
                xQueueSend(uartToLEDClrQueue, &cmd, 0);
                break;

            case CMD_RGB_GREEN:
                cmd.cmd = RGB_GREEN;
                xQueueSend(uartToLEDClrQueue, &cmd, 0);
                break;

            case CMD_RGB_BLUE:
                cmd.cmd = RGB_BLUE;
                xQueueSend(uartToLEDClrQueue, &cmd, 0);
                break;

            case CMD_RGB_YELLOW:
                cmd.cmd = RGB_YELLOW;
                xQueueSend(uartToLEDClrQueue, &cmd, 0);
                break;

            case CMD_RGB_CYAN:
                cmd.cmd = RGB_CYAN;
                xQueueSend(uartToLEDClrQueue, &cmd, 0);
                break;

            case CMD_RGB_MAGENTA:
                cmd.cmd = RGB_MAGENTA;
                xQueueSend(uartToLEDClrQueue, &cmd, 0);
                break;

            case CMD_RGB_WHITE:
                cmd.cmd = RGB_WHITE;
                xQueueSend(uartToLEDClrQueue, &cmd, 0);
                break;

            default:
                print_string("\nOption not recognized");
                break;
        }
		
        vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
        flush_uart();
    }
}
  


static void uart_init(void)
{
  XUartPs_Config *cfg;

  cfg = XUartPs_LookupConfig(UART_BASEADDR);
  if (!cfg){
    while (1) {}
  }

  if (XUartPs_CfgInitialize(&UartPs, cfg, cfg->BaseAddress) != XST_SUCCESS){
    while (1) {}
  }

  XUartPs_SetBaudRate(&UartPs, 115200);
}

static int uart_poll_rx(uint8_t *b)
{
  if (XUartPs_IsReceiveData(UartPs.Config.BaseAddress)){
    *b = XUartPs_ReadReg(UartPs.Config.BaseAddress, XUARTPS_FIFO_OFFSET);
    return 1;
  }
  return 0;
}

static void uart_tx_byte(uint8_t b)
{
  while (XUartPs_IsTransmitFull(UartPs.Config.BaseAddress)){  
      
  }
  
  XUartPs_WriteReg(UartPs.Config.BaseAddress, XUARTPS_FIFO_OFFSET, b);
}

void flush_uart(void)
{
    uint8_t dummy;    
    while (xQueueReceive(q_rx_byte, &dummy, 0) == pdTRUE);
}


void print_string(const char *str)
{
    // reimplement new print function
    xil_printf(str);
    // while (*str) {
    //     uint8_t c = *str++;
    //     xQueueSend(q_tx, &c, pdMS_TO_TICKS(POLL_DELAY_MS));
    // }
}

uint8_t receive_byte(uint8_t *out_byte)
{
    while(1){
        if (xQueueReceive(q_rx_byte, out_byte, 0)!=pdTRUE){            
            vTaskDelay(pdMS_TO_TICKS(POLL_DELAY_MS));
        } else {
            return *out_byte;
        }
    }
}