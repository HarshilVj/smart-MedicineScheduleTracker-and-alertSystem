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

int main(void) {
    // Set clock to 40 MHz
    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
    DelayMs(100);

    // Initialize medicines
    int i;
    for (i = 0; i < MAX_MEDICINES; i++) {
        medicines[i].active = false;
    }

    // Initialize peripherals
    I2C_LCD_Init();
    LCD_Init();
    LCD_Clear();
    Keypad_Init();
    Timer_Init();

    // Welcome display
    UpdateDisplay();

    while (1) {
        // Handle keypad input
        char key = Keypad_GetKey();
        if (key) {
            ProcessKeyInput(key);
            while (Keypad_GetKey()) DelayMs(10);
            DelayMs(100);
            UpdateDisplay();
        }

        // Handle display updates
        if (displayNeedsUpdate) {
            UpdateDisplay();
            displayNeedsUpdate = false;
        }

        // Alert timeout
        if (currentState == STATE_ALERT) {
            DelayMs(2000);
            currentState = STATE_TRACKING;
            displayNeedsUpdate = true;
        }
    }
}

void ProcessKeyInput(char key) {
    switch (currentState) {
        case STATE_WELCOME:
            if (key == '#') {
                currentState = STATE_GET_MED_NAME;
                currentMedicineIndex = 0;
                inputLength = 0;
                memset(inputBuffer, 0, sizeof(inputBuffer));
            }
            break;

        case STATE_GET_MED_NAME:
            if (key == 'C') {
                if (inputLength) inputBuffer[--inputLength] = '\0';
            } else if (key == 'D') {
                if (inputLength) {
                    strncpy(medicines[currentMedicineIndex].name, inputBuffer, MAX_NAME_LENGTH);
                    medicines[currentMedicineIndex].name[MAX_NAME_LENGTH] = '\0';
                    currentState = STATE_GET_MED_TIME;
                    inputLength = 0;
                    memset(inputBuffer, 0, sizeof(inputBuffer));
                }
            } else if (inputLength < MAX_NAME_LENGTH) {
                inputBuffer[inputLength++] = key;
                inputBuffer[inputLength] = '\0';
            }
            break;

        case STATE_GET_MED_TIME:
            if (key >= '1' && key <= '9') {
                medicines[currentMedicineIndex].timer = key - '0';
                medicines[currentMedicineIndex].timeRemaining = medicines[currentMedicineIndex].timer * 60;
                medicines[currentMedicineIndex].active = true;

                currentMedicineIndex++;
                currentState = (currentMedicineIndex < MAX_MEDICINES)
                    ? STATE_GET_MED_NAME : STATE_TRACKING;
            }
            break;

        case STATE_TRACKING:
            if (key == '#') {
                int j;
                for (j = 0; j < MAX_MEDICINES; j++) medicines[j].active = false;
                currentMedicineIndex = 0;
                currentState = STATE_WELCOME;
            }
            break;

        case STATE_ALERT:{
            int k;
            for (k = 0; k < MAX_MEDICINES; k++) {
                if (medicines[k].active && medicines[k].timeRemaining == 0) {
                    medicines[k].timeRemaining = medicines[k].timer * 60;
                    break;
                }
            }
        }
            currentState = STATE_TRACKING;
            break;
    }
}

void UpdateDisplay(void) {
    LCD_Clear();
    switch (currentState) {
        case STATE_WELCOME:
            LCD_Print("Medicine Tracker");
            LCD_Command(0xC0);
            LCD_Print("Press # to start");
            break;

        case STATE_GET_MED_NAME:
            LCD_Print("Name "); LCD_Char('1' + currentMedicineIndex);
            LCD_Command(0xC0);
            LCD_Print(inputBuffer);
            break;

        case STATE_GET_MED_TIME:
            LCD_Print("Time "); LCD_Char('1' + currentMedicineIndex);
            LCD_Print(" (1-9)");
            break;

        case STATE_TRACKING: {
            int next = -1; uint32_t best = UINT32_MAX;
            int i;
            for (i = 0; i < MAX_MEDICINES; i++) {
                if (medicines[i].active && medicines[i].timeRemaining < best) {
                    best = medicines[i].timeRemaining;
                    next = i;
                }
            }
            if (next >= 0) {
                uint8_t m = best / 60, s = best % 60;
                LCD_Print("Next:"); LCD_Print(medicines[next].name);
                LCD_Command(0xC0);
                char buf[6];
                snprintf(buf, sizeof(buf), "%02u:%02u", m, s);
                LCD_Print(buf);
            } else {
                LCD_Print("No active meds");
            }
            break;
        }

        case STATE_ALERT:
            LCD_Print("TAKE MEDICINE:");
            LCD_Command(0xC0);
            int n;
            for (n = 0; n < MAX_MEDICINES; n++) {
                if (medicines[n].active && medicines[n].timeRemaining == 0) {
                    LCD_Print(medicines[n].name);
                    break;
                }
            }
            break;
    }
}

void TimerIntHandler(void) {
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    if (currentState == STATE_TRACKING) {
        bool ready = false;
        int t;
        for (t = 0; t < MAX_MEDICINES; t++) {
            if (medicines[t].active && medicines[t].timeRemaining--) {
                if (medicines[t].timeRemaining == 0) ready = true;
            }
        }
        static uint8_t cnt = 0;
        if (ready) {
            currentState = STATE_ALERT;
            displayNeedsUpdate = true;
        } else if (++cnt >= 5) {
            cnt = 0;
            displayNeedsUpdate = true;
        }
    }
}

void I2C_LCD_Init(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C0));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB));
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);
    I2CMasterInitExpClk(I2C_BASE, SysCtlClockGet(), false);
}

void LCD_Send(uint8_t data, uint8_t mode) {
    uint8_t high = data & 0xF0;
    uint8_t low  = (data << 4) & 0xF0;
    uint8_t arr[4] = {
        high | BACKLIGHT | mode | 0x04,
        high | BACKLIGHT | mode,
        low  | BACKLIGHT | mode | 0x04,
        low  | BACKLIGHT | mode
    };
    int j;
    for (j = 0; j < 4; j++) {
        I2CMasterSlaveAddrSet(I2C_BASE, LCD_ADDR, false);
        I2CMasterDataPut(I2C_BASE, arr[j]);
        I2CMasterControl(I2C_BASE, I2C_MASTER_CMD_SINGLE_SEND);
        while (I2CMasterBusy(I2C_BASE));
        DelayMs(1);
    }
    DelayMs(2);
}

void LCD_Init(void) {
    DelayMs(50);
    LCD_Send(0x33, 0);
    LCD_Send(0x32, 0);
    LCD_Command(LCD_FUNCTION);
    LCD_Command(LCD_DISPLAY_ON);
    LCD_Command(LCD_CLEAR);
    LCD_Command(LCD_ENTRY_MODE);
    DelayMs(50);
}

void LCD_Clear(void) {
    LCD_Command(LCD_CLEAR);
    DelayMs(5);
}

void LCD_Command(uint8_t cmd) {
    LCD_Send(cmd, 0);
}

void LCD_Char(char c) {
    LCD_Send((uint8_t)c, 1);
}

void LCD_Print(const char *s) {
    while (*s) {
        LCD_Char(*s++);
    }
}

void DelayMs(uint32_t ms) {
    SysCtlDelay((SysCtlClockGet() / 3000) * ms);
}

void Keypad_Init(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD));
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE));
    GPIOPinTypeGPIOOutput(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS);
    GPIOPadConfigSet(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    GPIOPinWrite(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS, KEYPAD_ROW_PINS);
    GPIOPinTypeGPIOInput(KEYPAD_COL_PORT, KEYPAD_COL_PINS);
    GPIOPadConfigSet(KEYPAD_COL_PORT, KEYPAD_COL_PINS, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
}

char Keypad_GetKey(void) {
    const char keymap[4][4] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
    uint8_t row;
    for (row = 0; row < 4; row++) {
        GPIOPinWrite(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS, KEYPAD_ROW_PINS & ~(1 << row));
        DelayMs(1);
        uint8_t cols = ~GPIOPinRead(KEYPAD_COL_PORT, KEYPAD_COL_PINS) & KEYPAD_COL_PINS;
        if (cols) {
            uint8_t col;
            for (col = 0; col < 4; col++) {
                if (cols & (1 << col)) {
                    GPIOPinWrite(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS, KEYPAD_ROW_PINS);
                    return keymap[row][col];
                }
            }
        }
    }
    GPIOPinWrite(KEYPAD_ROW_PORT, KEYPAD_ROW_PINS, KEYPAD_ROW_PINS);
    return 0;
}

void Timer_Init(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0));
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet() - 1);
    IntMasterEnable();
    TimerIntRegister(TIMER0_BASE, TIMER_A, TimerIntHandler);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_A);
}
