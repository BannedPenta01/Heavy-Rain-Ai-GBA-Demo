// GBA Heavy Rain AI Simulator
// Created by Jan Michael with Cursor AI

#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <gba_dma.h>
#include <gba_types.h>
#include <string.h>
#include <stdlib.h>

// Display configuration
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
#define MODE3_WIDTH SCREEN_WIDTH
#define MODE3_HEIGHT SCREEN_HEIGHT

// Video memory configuration
#define VRAM_ADDR 0x6000000
static volatile u16* videoBuffer = (volatile u16*)VRAM_ADDR;

// Game states
typedef enum {
    STATE_MENU,
    STATE_RAIN
} GameState;

// Color definitions using 15-bit RGB
#define RGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))
#define COLOR_BLACK 0x0000
#define COLOR_WHITE RGB15(31,31,31)
#define COLOR_MENU_BG RGB15(4,4,4)    // Dark grey for menu
#define COLOR_TEXT_SHADOW RGB15(2,2,2)  // Darker shadow for text
#define COLOR_RAIN_LIGHT RGB15(24,26,28)  // More subtle bright rain
#define COLOR_RAIN_DARK RGB15(16,20,24)   // More subtle dark rain

// Rain configuration
#define MAX_RAINDROPS 150  // Reduced from 250 for better performance
#define MIN_LENGTH 2       // Shorter minimum length
#define MAX_LENGTH 5       // Shorter maximum length
#define MIN_RAIN_SPEED 2
#define MAX_RAIN_SPEED 8
#define DEFAULT_RAIN_SPEED 4
#define MAX_WIND 3
#define RAIN_LENGTH_MIN 2  // Shorter raindrops
#define RAIN_LENGTH_MAX 5  // Shorter max length

// Thunder effects
#define THUNDER_DURATION 30  // Increased duration
#define THUNDER_LIGHT RGB15(31,31,31)
#define THUNDER_SOFT 8     // Reduced from 12
#define THUNDER_HEAVY 16   // Reduced from 24
#define MENU_PULSE_RATE 60   // New constant for menu pulsing

// Background colors for mood
static const u16 bgColors[] = {
    RGB15(1,3,8),   // Deep blue
    RGB15(2,4,10),  // Medium blue
    RGB15(1,3,7),   // Dark blue
    RGB15(2,5,12)   // Light blue
};
#define NUM_BG_COLORS (sizeof(bgColors) / sizeof(bgColors[0]))

// Raindrop structure
typedef struct {
    s16 x, y;           // Position
    u8 length;          // Length of drop
    s8 speed;           // Vertical speed
    u8 alpha;           // Transparency
} Raindrop;

// Game state variables
static Raindrop raindrops[MAX_RAINDROPS] IWRAM_DATA;
static GameState gameState = STATE_MENU;
static s8 windForce = 0;
static u8 thunderTimer = 0;
static u8 thunderIntensity = 0;
static u8 rainSpeed = DEFAULT_RAIN_SPEED;
static u8 bgColorIndex = 0;
static u16 prevKeys = 0;
static u8 transitionTimer = 0;
static u8 menuAlpha = 31;
static u32 frameCounter = 0;  // Add frame counter

// Rain speed definitions
#define RAIN_SPEED_SLOW 2
#define RAIN_SPEED_MEDIUM 4
#define RAIN_SPEED_FAST 6

// Adjust rain appearance settings
#define DROP_HEAD_SIZE 2     // Size of raindrop head
#define TRAIL_LENGTH_MIN 6   // Minimum trail length
#define TRAIL_LENGTH_MAX 12  // Maximum trail length

// More realistic rain colors
#define COLOR_DROP_HEAD RGB15(28,30,31)  // Bright white-blue for drop head
#define COLOR_DROP_TRAIL RGB15(22,26,30) // Slightly darker for trail
#define COLOR_DROP_TAIL RGB15(18,22,28)  // Darkest for trail end

// Adjust rain colors for better line appearance
#define COLOR_RAIN_BRIGHT RGB15(28,30,31)  // Bright white-blue for rain
#define COLOR_RAIN_DIM RGB15(20,24,28)     // Dimmer for fade effect

// Function prototypes
static void InitSystem(void);
static void InitRain(void);
static void UpdateRain(void);
static void DrawRain(void);
static void DrawMenu(void);
static void HandleInput(void);
static void UpdateGame(void);
static void DrawGame(void);
static void TriggerThunder(u8 intensity);
static u16 BlendColors(u16 c1, u16 c2, u8 alpha);
static void DrawPixel(int x, int y, u16 color);
static void DrawChar(int x, int y, char c, u16 color, u8 alpha);
static void DrawText(int x, int y, const char* text, u16 color, u8 alpha);
static void ClearScreen(u16 color);
static bool ButtonPressed(u16 key);

// Simple bitmap font - each bit represents a pixel
static const unsigned char fontData[] = {
    // Space
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // A
    0x1C, 0x3E, 0x63, 0x63, 0x7F, 0x63, 0x63, 0x00,
    // B
    0x3E, 0x63, 0x63, 0x3E, 0x63, 0x63, 0x3E, 0x00,
    // C
    0x3E, 0x63, 0x60, 0x60, 0x60, 0x63, 0x3E, 0x00,
    // D
    0x7E, // 01111110
    0x63, // 01100011
    0x61, // 01100001
    0x61, // 01100001
    0x61, // 01100001
    0x63, // 01100011
    0x7E, // 01111110
    0x00, // 00000000
    // E
    0x7F, 0x60, 0x60, 0x7E, 0x60, 0x60, 0x7F, 0x00,
    // F
    0x7F, 0x60, 0x60, 0x7E, 0x60, 0x60, 0x60, 0x00,
    // G
    0x3E, 0x63, 0x60, 0x67, 0x63, 0x63, 0x3E, 0x00,
    // H
    0x63, 0x63, 0x63, 0x7F, 0x63, 0x63, 0x63, 0x00,
    // I
    0x3E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3E, 0x00,
    // J
    0x1F, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00,
    // K
    0x63, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x63, 0x00,
    // L
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7F, 0x00,
    // M
    0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00,
    // N
    0x63, 0x73, 0x7B, 0x6F, 0x67, 0x63, 0x63, 0x00,
    // O
    0x3E, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00,
    // P
    0x3E, 0x63, 0x63, 0x3E, 0x60, 0x60, 0x60, 0x00,
    // Q
    0x3E, 0x63, 0x63, 0x63, 0x6B, 0x66, 0x3D, 0x00,
    // R
    0x3E, 0x63, 0x63, 0x3E, 0x6C, 0x66, 0x63, 0x00,
    // S
    0x3E, 0x63, 0x60, 0x3E, 0x03, 0x63, 0x3E, 0x00,
    // T
    0x3E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x00,
    // U
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00,
    // V
    0x63, 0x63, 0x63, 0x36, 0x36, 0x1C, 0x08, 0x00,
    // W
    0x63, 0x63, 0x63, 0x6B, 0x7F, 0x36, 0x22, 0x00,
    // X
    0x63, 0x36, 0x1C, 0x08, 0x1C, 0x36, 0x63, 0x00,
    // Y
    0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x0C, 0x00,
    // Z
    0x7F, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7F, 0x00,
    // Numbers
    // 0
    0x3E, 0x63, 0x67, 0x6B, 0x73, 0x63, 0x3E, 0x00,
    // 1
    0x0C, 0x1C, 0x3C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00,
    // 2
    0x3E, 0x63, 0x03, 0x0E, 0x38, 0x60, 0x7F, 0x00,
    // 3
    0x3E, 0x63, 0x03, 0x1C, 0x03, 0x63, 0x3E, 0x00,
    // 4
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0x7F, 0x06, 0x00,
    // 5
    0x7F, 0x60, 0x7E, 0x03, 0x03, 0x63, 0x3E, 0x00,
    // 6
    0x1C, 0x30, 0x60, 0x7E, 0x63, 0x63, 0x3E, 0x00,
    // 7
    0x7F, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x00,
    // 8
    0x3E, 0x63, 0x63, 0x3E, 0x63, 0x63, 0x3E, 0x00,
    // 9
    0x3E, 0x63, 0x63, 0x3F, 0x03, 0x06, 0x3C, 0x00,
    // <
    0x0E, 0x1C, 0x38, 0x70, 0x38, 0x1C, 0x0E, 0x00,
    // -
    0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00,
    // >
    0x70, 0x38, 0x1C, 0x0E, 0x1C, 0x38, 0x70, 0x00,
    // .
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00
};

// Initialize the GBA system
static void InitSystem(void) {
    irqInit();
    irqEnable(IRQ_VBLANK);
    SetMode(MODE_3 | BG2_ENABLE);
}

// Initialize rain particles
static void InitRain(void) {
    rainSpeed = RAIN_SPEED_MEDIUM;
    
    // More scattered initialization
    for (int i = 0; i < MAX_RAINDROPS; i++) {
        raindrops[i].x = rand() % SCREEN_WIDTH;
        raindrops[i].y = -(rand() % 100);  // More varied starting heights
        raindrops[i].length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));
        raindrops[i].speed = rainSpeed + (rand() % 2);  // Slight speed variation
    }
    windForce = 0;
    thunderTimer = 0;
}

// Update rain particles
static void UpdateRain(void) {
    for (int i = 0; i < MAX_RAINDROPS; i++) {
        raindrops[i].y += raindrops[i].speed;
        raindrops[i].x += windForce;
        
        // Reset with more scattered positioning
        if (raindrops[i].y >= SCREEN_HEIGHT) {
            raindrops[i].y = -(rand() % 50);  // Varied spawn height
            raindrops[i].x = (rand() % (SCREEN_WIDTH + 40)) - 20;  // Allow spawning slightly off-screen
            raindrops[i].length = MIN_LENGTH + (rand() % (MAX_LENGTH - MIN_LENGTH + 1));
            raindrops[i].speed = rainSpeed + (rand() % 2);
        }
        
        // Wrap around with slight offset
        if (raindrops[i].x >= SCREEN_WIDTH + 10) raindrops[i].x = -10;
        else if (raindrops[i].x < -10) raindrops[i].x = SCREEN_WIDTH + 9;
    }
    
    if (thunderTimer > 0) thunderTimer--;
}

// Draw a single pixel with alpha blending
static void DrawPixel(int x, int y, u16 color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        videoBuffer[y * SCREEN_WIDTH + x] = color;
    }
}

// Draw the rain effect
static void DrawRain(void) {
    u16 bgColor = bgColors[bgColorIndex];
    
    if (thunderTimer > 0) {
        bgColor = BlendColors(bgColor, THUNDER_LIGHT, thunderIntensity >> 1);
    }
    
    ClearScreen(bgColor);
    
    // Draw each raindrop as a line
    for (int i = 0; i < MAX_RAINDROPS; i++) {
        int x = raindrops[i].x;
        int y = raindrops[i].y;
        int length = raindrops[i].length;
        
        if (y < 0 || y >= SCREEN_HEIGHT) continue;
        
        // Draw simple line with fade effect
        for (int j = 0; j < length; j++) {
            int py = y + j;
            if (py >= 0 && py < SCREEN_HEIGHT && x >= 0 && x < SCREEN_WIDTH) {
                u16 color;
                if (j == 0) {
                    color = COLOR_RAIN_BRIGHT;  // Bright top pixel
                } else {
                    // Fade out towards the bottom
                    u8 fade = ((length - j) * 31) / length;
                    color = BlendColors(COLOR_RAIN_DIM, bgColor, fade);
                }
                videoBuffer[py * SCREEN_WIDTH + x] = color;
            }
        }
    }
}

// Draw the menu screen
static void DrawMenu(void) {
    ClearScreen(COLOR_MENU_BG);
    
    // Calculate true center position
    int centerY = (SCREEN_HEIGHT / 2) - 50;  // Start 50 pixels above center
    
    // Draw menu text with consistent spacing
    DrawText((SCREEN_WIDTH - 9 * 8) / 2, centerY, "CLAUDE 3.7", COLOR_WHITE, 31);
    DrawText((SCREEN_WIDTH - 10 * 8) / 2, centerY + 20, "RAIN STORM", COLOR_WHITE, 31);
    DrawText((SCREEN_WIDTH - 11 * 8) / 2, centerY + 40, "JAN MICHAEL", COLOR_WHITE, 31);
    DrawText((SCREEN_WIDTH - 9 * 8) / 2, centerY + 55, "CURSOR AI", COLOR_WHITE, 31);
    DrawText((SCREEN_WIDTH - 7 * 8) / 2, centerY + 75, "PRESS A", COLOR_WHITE, menuAlpha);
    DrawText((SCREEN_WIDTH - 9 * 8) / 2, centerY + 90, "HOLD DPAD", COLOR_WHITE, 31);
}

// Update game state
static void UpdateGame(void) {
    frameCounter++;
    
    switch (gameState) {
        case STATE_MENU:
            // Only update menu alpha for "PRESS A" text
            menuAlpha = 20 + ((frameCounter % 60) * 11) / 60;
            break;
            
        case STATE_RAIN:
            UpdateRain();
            // Handle thunder effect independently without affecting state
            if (thunderTimer > 0) {
                thunderTimer--;
            }
            break;
    }
}

// Handle user input
static void HandleInput(void) {
    u16 keys = ~REG_KEYINPUT & 0x03FF;
    
    switch (gameState) {
        case STATE_MENU:
            if (ButtonPressed(KEY_A)) {
                gameState = STATE_RAIN;
                InitRain();
            }
            break;
            
        case STATE_RAIN:
            // Wind control
            if (keys & KEY_LEFT && windForce > -MAX_WIND) windForce--;
            if (keys & KEY_RIGHT && windForce < MAX_WIND) windForce++;
            
            // Three-speed cycle control
            if (ButtonPressed(KEY_UP)) {
                // Cycle up: SLOW -> MEDIUM -> FAST
                if (rainSpeed == RAIN_SPEED_SLOW) {
                    rainSpeed = RAIN_SPEED_MEDIUM;
                } else if (rainSpeed == RAIN_SPEED_MEDIUM) {
                    rainSpeed = RAIN_SPEED_FAST;
                }
                // Update all raindrop speeds
                for (int i = 0; i < MAX_RAINDROPS; i++) {
                    raindrops[i].speed = rainSpeed;
                }
            }
            if (ButtonPressed(KEY_DOWN)) {
                // Cycle down: FAST -> MEDIUM -> SLOW
                if (rainSpeed == RAIN_SPEED_FAST) {
                    rainSpeed = RAIN_SPEED_MEDIUM;
                } else if (rainSpeed == RAIN_SPEED_MEDIUM) {
                    rainSpeed = RAIN_SPEED_SLOW;
                }
                // Update all raindrop speeds
                for (int i = 0; i < MAX_RAINDROPS; i++) {
                    raindrops[i].speed = rainSpeed;
                }
            }
            
            // Thunder effects
            if (ButtonPressed(KEY_L)) TriggerThunder(THUNDER_SOFT);
            if (ButtonPressed(KEY_R)) TriggerThunder(THUNDER_HEAVY);
            
            // Background cycling
            if (ButtonPressed(KEY_SELECT)) {
                bgColorIndex = (bgColorIndex + 1) % NUM_BG_COLORS;
            }
            
            // Reset
            if (ButtonPressed(KEY_START)) {
                rainSpeed = DEFAULT_RAIN_SPEED;
                InitRain();
            }
            
            // Return to menu
            if (ButtonPressed(KEY_B)) {
                gameState = STATE_MENU;
            }
            break;
    }
    
    prevKeys = keys;
}

// Draw the current game state
static void DrawGame(void) {
    switch (gameState) {
        case STATE_MENU:
            DrawMenu();
            break;
            
        case STATE_RAIN:
            DrawRain();
            break;
    }
}

// Main game loop
int main(void) {
    // Initialize system
    irqInit();
    irqEnable(IRQ_VBLANK);
    SetMode(MODE_3 | BG2_ENABLE);
    
    // Initialize game
    InitRain();
    gameState = STATE_MENU;
    
    // Main loop
    while (1) {
        VBlankIntrWait();
        HandleInput();
        UpdateGame();
        DrawGame();
    }
    
    return 0;
}

// Utility functions
static bool ButtonPressed(u16 key) {
    return (~REG_KEYINPUT & key) && !(prevKeys & key);
}

static void TriggerThunder(u8 intensity) {
    thunderTimer = THUNDER_DURATION;
    thunderIntensity = intensity;
    // Don't affect game state
}

static u16 BlendColors(u16 c1, u16 c2, u8 alpha) {
    u8 r1 = (c1 >> 0) & 0x1F;
    u8 g1 = (c1 >> 5) & 0x1F;
    u8 b1 = (c1 >> 10) & 0x1F;
    
    u8 r2 = (c2 >> 0) & 0x1F;
    u8 g2 = (c2 >> 5) & 0x1F;
    u8 b2 = (c2 >> 10) & 0x1F;
    
    u8 r = (r1 * (31 - alpha) + r2 * alpha) / 31;
    u8 g = (g1 * (31 - alpha) + g2 * alpha) / 31;
    u8 b = (b1 * (31 - alpha) + b2 * alpha) / 31;
    
    return RGB15(r, g, b);
}

static void ClearScreen(u16 color) {
    u32 fillValue = color | (color << 16);
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT / 2; i++) {
        ((volatile u32*)videoBuffer)[i] = fillValue;
    }
}

// Draw a character with alpha blending and shadow
static void DrawChar(int x, int y, char c, u16 color, u8 alpha) {
    int index;
    if (c >= 'a') c &= ~0x20;  // Convert to uppercase
    
    if (c == ' ') index = 0;
    else if (c >= 'A' && c <= 'Z') index = c - 'A' + 1;
    else if (c >= '0' && c <= '9') index = (c - '0') + 27;
    else if (c == '<') index = 37;
    else if (c == '-') index = 38;
    else if (c == '>') index = 39;
    else if (c == '.') index = 40;
    else return;
    
    const unsigned char* charSprite = &fontData[index << 3];
    
    // Draw character with shadow
    for (int row = 0; row < 8; row++) {
        unsigned char line = charSprite[row];
        if (!line) continue;  // Skip empty rows
        
        int baseY = y + row;
        if (baseY >= 0 && baseY < SCREEN_HEIGHT) {
            for (int bit = 0; bit < 8; bit++) {
                if (line & (0x80 >> bit)) {
                    int baseX = x + bit;
                    if (baseX >= 0 && baseX < SCREEN_WIDTH) {
                        // Draw shadow first
                        if (baseX + 1 < SCREEN_WIDTH && baseY + 1 < SCREEN_HEIGHT) {
                            videoBuffer[(baseY + 1) * SCREEN_WIDTH + baseX + 1] = COLOR_TEXT_SHADOW;
                        }
                        // Draw main pixel with alpha blending if needed
                        if (alpha < 31) {
                            videoBuffer[baseY * SCREEN_WIDTH + baseX] = BlendColors(color, COLOR_MENU_BG, alpha);
                        } else {
                            videoBuffer[baseY * SCREEN_WIDTH + baseX] = color;
                        }
                    }
                }
            }
        }
    }
}

// Draw text string with alpha blending
static void DrawText(int x, int y, const char* text, u16 color, u8 alpha) {
    int cursor_x = x;
    while (*text) {
        DrawChar(cursor_x, y, *text++, color, alpha);
        cursor_x += 8;  // Fixed character width
    }
}