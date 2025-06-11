#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_timer.h"
#include "inc/hw_ints.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/i2c.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"

// LCD I2C
#define I2C_BASE    I2C0_BASE
#define LCD_ADDR    0x27
#define BACKLIGHT   0x08

// LCD Commands
#define LCD_CLEAR       0x01
#define LCD_HOME        0x02
#define LCD_ENTRY_MODE  0x06
#define LCD_DISPLAY_ON  0x0C
#define LCD_FUNCTION    0x28  // 4-bit, 2 lines, 5x8 dots

// Keypad configuration
#define KEYPAD_ROW_PORT GPIO_PORTD_BASE
#define KEYPAD_ROW_PINS (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)
#define KEYPAD_COL_PORT GPIO_PORTE_BASE
#define KEYPAD_COL_PINS (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

// Maximum number of medicines to track
#define MAX_MEDICINES    3
#define MAX_NAME_LENGTH 10

// System states
typedef enum {
    STATE_WELCOME,
    STATE_GET_MED_NAME,
    STATE_GET_MED_TIME,
    STATE_TRACKING,
    STATE_ALERT
} SystemState;

// Medicine structure
typedef struct {
    char     name[MAX_NAME_LENGTH + 1];
    uint8_t  timer;           // time in minutes
    bool     active;          // if medicine is scheduled
    uint32_t timeRemaining;   // time remaining in seconds
} Medicine;

// Globals
volatile SystemState currentState = STATE_WELCOME;
Medicine medicines[MAX_MEDICINES];
uint8_t currentMedicineIndex = 0;
char inputBuffer[MAX_NAME_LENGTH + 1];
uint8_t inputLength = 0;
bool displayNeedsUpdate = false;

// Function prototypes
void I2C_LCD_Init(void);
void LCD_Send(uint8_t data, uint8_t mode);
void LCD_Init(void);
void LCD_Clear(void);
void LCD_Command(uint8_t cmd);
void LCD_Char(char c);
void LCD_Print(const char *s);
void DelayMs(uint32_t ms);
void Keypad_Init(void);
char Keypad_GetKey(void);
void Timer_Init(void);
void ProcessKeyInput(char key);
void UpdateDisplay(void);
void TimerIntHandler(void);

// Main function
int main(void) {
    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
    DelayMs(100);
    int i;
    for (i = 0; i < MAX_MEDICINES; i++) medicines[i].active = false;
    I2C_LCD_Init();
    LCD_Init();
    LCD_Clear();
    Keypad_Init();
    Timer_Init();
    UpdateDisplay();

    while (1) {
        char key = Keypad_GetKey();
        if (key) {
            ProcessKeyInput(key);
            while (Keypad_GetKey()) DelayMs(10);
            DelayMs(100);
            UpdateDisplay();
        }
        if (displayNeedsUpdate) {
            UpdateDisplay();
            displayNeedsUpdate = false;
        }
        if (currentState == STATE_ALERT) {
            DelayMs(2000);
            currentState = STATE_TRACKING;
            displayNeedsUpdate = true;
        }
    }
}
