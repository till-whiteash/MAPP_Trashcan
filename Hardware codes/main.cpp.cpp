// main.cpp
// Smart Recycling Bin - Complete System Integration
// STM32F103RB with LCD, Keypad, 7-Segment Display, Buzzer
// Mbed OS 6 Compatible

#undef __ARM_FP

#include "mbed.h"
#include "lcd.h"
#include "keypad.h"
#include "music.h"
#include "seg7.h"
#include "trashlevel.h"
#include "trashtype.h"

// Create trash level sensors for each bin
TrashLevel plasticBin(PA_1, PA_2);
TrashLevel metalBin(PA_3, PA_4);
TrashLevel paperBin(PA_5, PA_6);
TrashLevel glassBin(PA_7, PB_6);

// Material detection and sorting
TrashType sorter(PB_8, PA_9, PA_10);

// 7-Segment Display (for item counter)
// Using Port C for 7-segment data
BusOut seg7Data(PC_0, PC_1, PC_2, PC_3, PC_4, PC_5, PC_6);  // 7 segments (a-g)
BusOut seg7Digit(PC_8, PC_9, PC_10, PC_11);  // 4 digit selects

// PIR Motion Sensor
DigitalIn pirSensor(PB_0);

// DC Motor for door
PwmOut doorMotor(PA_8);
DigitalOut motorIN1(PB_10);
DigitalOut motorIN2(PB_11);

// Status LEDs
DigitalOut ledSystem(LED1);
DigitalOut ledBinFull(PC_13);

// Global variables
unsigned char itemCount[4] = {0, 0, 0, 0};  // Counter for 7-segment (0000-9999)
int totalItems = 0;
bool systemEnabled = true;
bool doorOpen = false;

// Menu state
enum MenuState {
    MENU_MAIN,
    MENU_STATS,
    MENU_SETTINGS,
    MENU_MANUAL
};

MenuState currentMenu = MENU_MAIN;

// Function prototypes
void displayMainMenu();
void displayStats();
void displaySettings();
void handleKeypad();
void updateSegmentDisplay();
void playSuccessSound();
void playErrorSound();
void playWelcomeSound();
void openDoor();
void closeDoor();
void processItem();
void checkBinLevels();

//=============== DOOR CONTROL FUNCTIONS ===============

void openDoor() {
    lcd_write_cmd(0x01);  // Clear display
    lcd_write_cmd(0x80);  // Line 1
    lcd_write_data('O');
    lcd_write_data('p');
    lcd_write_data('e');
    lcd_write_data('n');
    lcd_write_data('i');
    lcd_write_data('n');
    lcd_write_data('g');
    lcd_write_data('.');
    lcd_write_data('.');
    lcd_write_data('.');
    
    motorIN1 = 1;
    motorIN2 = 0;
    doorMotor.write(0.7f);
    
    thread_sleep_for(2000);  // 2 seconds to open
    
    motorIN1 = 0;
    motorIN2 = 0;
    doorMotor.write(0.0f);
    
    doorOpen = true;
    printf("Door opened\r\n");
}

void closeDoor() {
    lcd_write_cmd(0x01);  // Clear display
    lcd_write_cmd(0x80);  // Line 1
    lcd_write_data('C');
    lcd_write_data('l');
    lcd_write_data('o');
    lcd_write_data('s');
    lcd_write_data('i');
    lcd_write_data('n');
    lcd_write_data('g');
    lcd_write_data('.');
    lcd_write_data('.');
    lcd_write_data('.');
    
    motorIN1 = 0;
    motorIN2 = 1;
    doorMotor.write(0.7f);
    
    thread_sleep_for(2000);  // 2 seconds to close
    
    motorIN1 = 0;
    motorIN2 = 0;
    doorMotor.write(0.0f);
    
    doorOpen = false;
    printf("Door closed\r\n");
}

//=============== SOUND FUNCTIONS ===============

void playWelcomeSound() {
    tone(NOTE_C4, 8);
    tone(NOTE_E4, 8);
    tone(NOTE_G4, 4);
}

void playSuccessSound() {
    tone(NOTE_C5, 8);
    tone(NOTE_E5, 8);
    tone(NOTE_G5, 8);
    tone(NOTE_C6, 4);
}

void playErrorSound() {
    tone(NOTE_C4, 8);
    tone(NOTE_A3, 8);
    tone(NOTE_F3, 4);
}

void playBinFullSound() {
    for(int i = 0; i < 3; i++) {
        tone(NOTE_A4, 8);
        thread_sleep_for(100);
    }
}

//=============== 7-SEGMENT DISPLAY FUNCTIONS ===============

void updateSegmentDisplay() {
    // Update the item count display
    for(int digit = 0; digit < 4; digit++) {
        // Select digit
        seg7Digit = 0x00;  // Turn off all digits
        seg7Digit = (1 << digit);  // Turn on current digit
        
        // Display number
        seg7Data = convert(itemCount[digit]);
        
        thread_sleep_for(5);  // Small delay for multiplexing
    }
}

void incrementCounter() {
    itemCount[0]++;
    update(itemCount, 4);  // Handle carry-over
    totalItems++;
}

//=============== LCD DISPLAY FUNCTIONS ===============

void displayMainMenu() {
    lcd_write_cmd(0x01);  // Clear display
    
    // Line 1: "Smart Bin Ready"
    lcd_write_cmd(0x80);  // Line 1, position 0
    lcd_write_data('S');
    lcd_write_data('m');
    lcd_write_data('a');
    lcd_write_data('r');
    lcd_write_data('t');
    lcd_write_data(' ');
    lcd_write_data('B');
    lcd_write_data('i');
    lcd_write_data('n');
    
    // Line 2: "Items: XXXX"
    lcd_write_cmd(0xC0);  // Line 2, position 0
    lcd_write_data('I');
    lcd_write_data('t');
    lcd_write_data('e');
    lcd_write_data('m');
    lcd_write_data('s');
    lcd_write_data(':');
    lcd_write_data(' ');
    
    // Display item count
    char buffer[5];
    sprintf(buffer, "%04d", totalItems);
    for(int i = 0; i < 4; i++) {
        lcd_write_data(buffer[i]);
    }
}

void displayStats() {
    lcd_write_cmd(0x01);  // Clear display
    
    // Line 1: "Bin Levels"
    lcd_write_cmd(0x80);
    lcd_write_data('B');
    lcd_write_data('i');
    lcd_write_data('n');
    lcd_write_data(' ');
    lcd_write_data('L');
    lcd_write_data('e');
    lcd_write_data('v');
    lcd_write_data('e');
    lcd_write_data('l');
    lcd_write_data('s');
    
    // Line 2: "P:XX M:XX P:XX"
    lcd_write_cmd(0xC0);
    
    int plasticLevel = plasticBin.getCapacityPercent();
    int metalLevel = metalBin.getCapacityPercent();
    
    lcd_write_data('P');
    lcd_write_data(':');
    lcd_write_data('0' + (plasticLevel / 10));
    lcd_write_data('0' + (plasticLevel % 10));
    lcd_write_data(' ');
    
    lcd_write_data('M');
    lcd_write_data(':');
    lcd_write_data('0' + (metalLevel / 10));
    lcd_write_data('0' + (metalLevel % 10));
}

void displaySettings() {
    lcd_write_cmd(0x01);  // Clear display
    
    // Line 1: "Settings Menu"
    lcd_write_cmd(0x80);
    lcd_write_data('S');
    lcd_write_data('e');
    lcd_write_data('t');
    lcd_write_data('t');
    lcd_write_data('i');
    lcd_write_data('n');
    lcd_write_data('g');
    lcd_write_data('s');
    
    // Line 2: "A:Enable B:Back"
    lcd_write_cmd(0xC0);
    lcd_write_data('A');
    lcd_write_data(':');
    if(systemEnabled) {
        lcd_write_data('O');
        lcd_write_data('N');
    } else {
        lcd_write_data('O');
        lcd_write_data('F');
        lcd_write_data('F');
    }
}

void displayProcessing() {
    lcd_write_cmd(0x01);
    
    lcd_write_cmd(0x80);
    lcd_write_data('P');
    lcd_write_data('r');
    lcd_write_data('o');
    lcd_write_data('c');
    lcd_write_data('e');
    lcd_write_data('s');
    lcd_write_data('s');
    lcd_write_data('i');
    lcd_write_data('n');
    lcd_write_data('g');
    lcd_write_data('.');
    lcd_write_data('.');
    lcd_write_data('.');
}

//=============== BIN LEVEL CHECK ===============

void checkBinLevels() {
    if(plasticBin.isFull() || metalBin.isFull() || 
       paperBin.isFull() || glassBin.isFull()) {
        
        ledBinFull = 1;
        playBinFullSound();
        
        lcd_write_cmd(0x01);
        lcd_write_cmd(0x80);
        lcd_write_data('W');
        lcd_write_data('A');
        lcd_write_data('R');
        lcd_write_data('N');
        lcd_write_data('I');
        lcd_write_data('N');
        lcd_write_data('G');
        lcd_write_data('!');
        
        lcd_write_cmd(0xC0);
        lcd_write_data('B');
        lcd_write_data('i');
        lcd_write_data('n');
        lcd_write_data(' ');
        lcd_write_data('F');
        lcd_write_data('U');
        lcd_write_data('L');
        lcd_write_data('L');
        lcd_write_data('!');
        
        thread_sleep_for(3000);
    } else {
        ledBinFull = 0;
    }
}

//=============== ITEM PROCESSING ===============

void processItem() {
    if(!systemEnabled) {
        playErrorSound();
        return;
    }
    
    printf("\r\n=== Processing New Item ===\r\n");
    
    // Step 1: Open door
    openDoor();
    thread_sleep_for(3000);  // Wait for item placement
    closeDoor();
    
    // Step 2: Display processing
    displayProcessing();
    
    // Step 3: Detect material
    MaterialType material = sorter.detectMaterial();
    printf("Detected: %s\r\n", sorter.getMaterialName(material));
    
    // Step 4: Show result on LCD
    lcd_write_cmd(0x01);
    lcd_write_cmd(0x80);
    lcd_write_data('D');
    lcd_write_data('e');
    lcd_write_data('t');
    lcd_write_data('e');
    lcd_write_data('c');
    lcd_write_data('t');
    lcd_write_data('e');
    lcd_write_data('d');
    lcd_write_data(':');
    
    lcd_write_cmd(0xC0);
    const char* matName = sorter.getMaterialName(material);
    for(int i = 0; matName[i] != '\0' && i < 16; i++) {
        lcd_write_data(matName[i]);
    }
    
    thread_sleep_for(2000);
    
    // Step 5: Rotate to bin
    sorter.rotateToBin(material);
    
    // Step 6: Increment counter
    incrementCounter();
    
    // Step 7: Play success sound
    playSuccessSound();
    
    // Step 8: Check bin levels
    checkBinLevels();
    
    // Step 9: Return to main menu
    displayMainMenu();
    
    printf("=== Processing Complete ===\r\n\r\n");
}

//=============== KEYPAD HANDLING ===============

void handleKeypad() {
    char key = getkey();
    
    printf("Key pressed: %c\r\n", key);
    tone(NOTE_C4, 16);  // Short beep
    
    switch(currentMenu) {
        case MENU_MAIN:
            if(key == '1') {
                // Manual trigger
                processItem();
            }
            else if(key == '2') {
                currentMenu = MENU_STATS;
                displayStats();
            }
            else if(key == '3') {
                currentMenu = MENU_SETTINGS;
                displaySettings();
            }
            else if(key == 'A') {
                // Play music
                music();
            }
            break;
            
        case MENU_STATS:
            if(key == 'B' || key == '0') {
                currentMenu = MENU_MAIN;
                displayMainMenu();
            }
            break;
            
        case MENU_SETTINGS:
            if(key == 'A') {
                systemEnabled = !systemEnabled;
                displaySettings();
            }
            else if(key == 'B' || key == '0') {
                currentMenu = MENU_MAIN;
                displayMainMenu();
            }
            break;
    }
}

//=============== INITIALIZATION ===============

void initSystem() {
    printf("\r\n");
    printf("==========================================\r\n");
    printf("    Smart Recycling Bin System v2.0      \r\n");
    printf("    STM32F103RB with Full Integration    \r\n");
    printf("==========================================\r\n\r\n");
    
    // Initialize LCD
    printf("Initializing LCD...\r\n");
    lcd_init();
    lcd_Clear();
    
    // Initialize motor PWM
    doorMotor.period_ms(1);
    doorMotor.write(0.0f);
    
    // Initialize PIR sensor
    pirSensor.mode(PullDown);
    
    // Initialize LEDs
    ledSystem = 1;
    ledBinFull = 0;
    
    // Display welcome message
    lcd_write_cmd(0x80);
    lcd_write_data('S');
    lcd_write_data('m');
    lcd_write_data('a');
    lcd_write_data('r');
    lcd_write_data('t');
    lcd_write_data(' ');
    lcd_write_data('B');
    lcd_write_data('i');
    lcd_write_data('n');
    
    lcd_write_cmd(0xC0);
    lcd_write_data('I');
    lcd_write_data('n');
    lcd_write_data('i');
    lcd_write_data('t');
    lcd_write_data('i');
    lcd_write_data('a');
    lcd_write_data('l');
    lcd_write_data('i');
    lcd_write_data('z');
    lcd_write_data('i');
    lcd_write_data('n');
    lcd_write_data('g');
    lcd_write_data('.');
    lcd_write_data('.');
    lcd_write_data('.');
    
    // Play welcome sound
    playWelcomeSound();
    
    thread_sleep_for(2000);
    
    // Display main menu
    displayMainMenu();
    
    printf("System initialized successfully!\r\n");
    printf("Ready to sort trash!\r\n\r\n");
}

//=============== MAIN FUNCTION ===============

int main() {
    // Initialize system
    initSystem();
    
    // Main loop
    while(1) {
        // Check for PIR motion
        if(pirSensor == 1 && !doorOpen && systemEnabled) {
            printf("Motion detected!\r\n");
            playWelcomeSound();
            processItem();
            thread_sleep_for(3000);  // Cooldown
        }
        
        // Update 7-segment display
        updateSegmentDisplay();
        
        // Blink system LED
        ledSystem = !ledSystem;
        thread_sleep_for(500);
        
        // Note: Keypad is blocking, so it would be checked in interrupt
        // For non-blocking operation, you'd need to implement interrupt-driven keypad
    }
}