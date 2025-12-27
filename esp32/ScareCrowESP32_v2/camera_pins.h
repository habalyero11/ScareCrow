/*
 * Camera Pin Definitions for Various ESP32 Boards
 * 
 * This file contains pin mappings for different ESP32 camera boards.
 * Uncomment the section that matches your board.
 * 
 * Default: Freenove ESP32-S3 N16R8 CAM
 */

#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

// ==================== SELECT YOUR CAMERA MODULE ====================
// Uncomment ONE of the following options

#define CAMERA_MODEL_FREENOVE_S3_N16R8    // Freenove ESP32-S3 N16R8 CAM (Default)
// #define CAMERA_MODEL_ESP32S3_EYE       // ESP32-S3-EYE (Espressif)
// #define CAMERA_MODEL_AI_THINKER        // ESP32-CAM AI-Thinker
// #define CAMERA_MODEL_WROVER_KIT        // ESP-WROVER-KIT

// ==================== FREENOVE ESP32-S3 N16R8 CAM (Default) ====================
// Official pinout from Freenove documentation

#if defined(CAMERA_MODEL_FREENOVE_S3_N16R8)

#define PWDN_GPIO_NUM     -1      // Power down not used
#define RESET_GPIO_NUM    -1      // Reset not used

// I2C pins (SCCB interface)
#define SIOD_GPIO_NUM     4       // CAM_SIOD → GPIO4
#define SIOC_GPIO_NUM     5       // CAM_SIOC → GPIO5

// Sync pins
#define VSYNC_GPIO_NUM    6       // CAM_VSYNC → GPIO6
#define HREF_GPIO_NUM     7       // CAM_HREF → GPIO7

// Clock pins
#define XCLK_GPIO_NUM     15      // CAM_XCLK → GPIO15
#define PCLK_GPIO_NUM     13      // CAM_PCLK → GPIO13

// Data pins (Y2-Y9, directly from your pinout)
#define Y9_GPIO_NUM       16      // CAM_Y9 → GPIO16
#define Y8_GPIO_NUM       17      // CAM_Y8 → GPIO17
#define Y7_GPIO_NUM       18      // CAM_Y7 → GPIO18
#define Y6_GPIO_NUM       12      // CAM_Y6 → GPIO12
#define Y5_GPIO_NUM       10      // CAM_Y5 → GPIO10
#define Y4_GPIO_NUM       8       // CAM_Y4 → GPIO8
#define Y3_GPIO_NUM       9       // CAM_Y3 → GPIO9
#define Y2_GPIO_NUM       11      // CAM_Y2 → GPIO11

#endif

// ==================== ESP32-S3-EYE (Espressif) ====================

#if defined(CAMERA_MODEL_ESP32S3_EYE)

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#endif

// ==================== ESP32-CAM AI-THINKER ====================
// Most common ESP32-CAM board

#if defined(CAMERA_MODEL_AI_THINKER)

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#endif

// ==================== ESP-WROVER-KIT ====================

#if defined(CAMERA_MODEL_WROVER_KIT)

#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM       5
#define Y2_GPIO_NUM       4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

#endif

// ==================== VALIDATION ====================
#ifndef XCLK_GPIO_NUM
#error "Camera model not selected! Please uncomment one of the CAMERA_MODEL_* defines at the top of camera_pins.h"
#endif

#endif // CAMERA_PINS_H
