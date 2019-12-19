/*
    gnss_tracker.ino - GNSS tracker example application
    Copyright 2018 Sony Semiconductor Solutions Corporation

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
   @file gnss_tracker.ino
   @author Sony Semiconductor Solutions Corporation
   @brief GNSS tracker example application
   @details The gnss_tracker is a sample sketch that performs GPS positioning by
            intermittent operation.
*/

#include <GNSS.h>
#include <GNSSPositionData.h>
#include "gnss_logger.h"
#include "gnss_nmea.h"
#include "gnss_file.h"

/* Index file */
#define INDEX_FILE_NAME    DATA_DIR_NAME "index.ini"     /**< Index file name */
#define INDEX_FILE_SIZE    16              /**< Index file size */

/* NMEA file */
#define NMEA_FILE_NAME    DATA_DIR_NAME "%08d.txt"       /**< NMEA file name */

#define STRING_BUFFER_SIZE  128            /**< %String buffer size */
#define NMEA_BUFFER_SIZE    128            /**< NMEA buffer size */
#define OUTPUT_FILENAME_LEN 32             /**< Output file name length */

/* parameter. */
#define POSITIONING_INTERVAL  5 /**< positioning interval in seconds */
#define DATA_WRITE_INTERVAL  60 /**< data write interval in seconds */
#define NUMBER_OF_CACHE (DATA_WRITE_INTERVAL / POSITIONING_INTERVAL)

// NMEA data buffer size
// 12 = 60sec (write interval) / 5sec (positioning interval)
#define MEMORY_BUFFER_SIZE (NMEA_BUFFER_SIZE * NUMBER_OF_CACHE)

#define SERIAL_BAUDRATE     115200         /**< Serial baud rate */

#define SEPARATOR           0x0A           /**< Separator */

/**
   @enum LoopState
   @brief State of loop
*/
enum LoopState {
  eStateActive  /**< Loop is activated */
};

SpGnss Gnss;                            /**< SpGnss object */
char FilenameTxt[OUTPUT_FILENAME_LEN];  /**< Output file name */
char FilenameBin[OUTPUT_FILENAME_LEN];  /**< Output binary file name */
AppPrintLevel AppDebugPrintLevel;       /**< Print level */

/**
   @brief Turn on / off the LED1 for positioning state notification.

   @param [in] state Positioning state
*/
static void Led_isPosfix(bool state)
{
  if (state == 1)
  {
    ledOn(PIN_LED0);
  }
  else
  {
    ledOff(PIN_LED0);
  }
}

/**
   @brief Turn on / off the LED2 for file SD access notification.

   @param [in] state SD access state
*/
static void Led_isStorageAccess(bool state)
{
  if (state == 1)
  {
    ledOn(PIN_LED3);
  }
  else
  {
    ledOff(PIN_LED3);
  }
}

/**
   @brief Turn on / off the LED3 for error notification.

   @param [in] state Error state
*/
static void Led_isError(bool state)
{
  /*
  if (state == 1)
  {
    ledOn(PIN_LED3);
  }
  else
  {
    ledOff(PIN_LED3);
  }
  */
}

/**
   @brief Get file number.

   @return File count
*/
unsigned long GetFileNumber(void)
{
  int FileCount;
  char IndexData[INDEX_FILE_SIZE];
  int ReadSize = 0;

  /* Open index file. */
  ReadSize = ReadChar(IndexData, INDEX_FILE_SIZE, INDEX_FILE_NAME, FILE_READ);
  if (ReadSize != 0)
  {
    /* Use index data. */
    FileCount = strtoul(IndexData, NULL, 10);
    FileCount++;

    Remove(INDEX_FILE_NAME);
  }
  else
  {
    /* Init file count. */
    FileCount = 1;
  }

  /* Update index.txt */
  snprintf(IndexData, sizeof(IndexData), "%08d", FileCount);
  WriteChar(IndexData, INDEX_FILE_NAME, FILE_WRITE);

  return FileCount;
}

/**
   @brief Setup positioning.

   @return 0 if success, 1 if failure
*/
static int SetupPositioning(void)
{
  int error_flag = 0;
  
  /* Mount SD card. */
  if (BeginSDCard() != true)
  {
    /* Error case.*/
    APP_PRINT_E("SD begin error!!\n");
    error_flag = 1;
  }

  /* Set Gnss debug mode. */
  Gnss.setDebugMode(PrintNone);
  AppDebugPrintLevel = AppPrintInfo;

  if (Gnss.begin(Serial) != 0)
  {
    /* Error case. */
    APP_PRINT_E("Gnss begin error!!\n");
    error_flag = 1;
  }
  else
  {
    APP_PRINT_I("Gnss begin OK.\n");

    // GPS + QZSS(L1C/A) + QZAA(L1S)
    Gnss.select(GPS);
    Gnss.select(QZ_L1CA);
    Gnss.select(QZ_L1S);
    Gnss.setInterval(POSITIONING_INTERVAL);

    if (Gnss.start(HOT_START) != OK)
    {
      /* Error case. */
      APP_PRINT_E("Gnss start error!!\n");
      error_flag = 1;
    }
  }

  /* Create output file name. */
  FilenameTxt[0] = 0;
  int FileCount = GetFileNumber();
  snprintf(FilenameTxt, sizeof(FilenameTxt), NMEA_FILE_NAME, FileCount);

  return error_flag;
}

/**
   @brief Activate GNSS device and setup positioning
*/
void setup()
{
  int error_flag = 0;

  /* Open serial communications and wait for port to open */
  Serial.begin(115200);
  while (!Serial) {
    ; /* wait for serial port to connect. Needed for native USB port only */
  }

  /* Turn on all LED:Setup start. */
  ledOn(PIN_LED0);
  ledOn(PIN_LED1);
  ledOn(PIN_LED2);
  ledOn(PIN_LED3);

  error_flag = SetupPositioning();

  /* Turn off all LED:Setup done. */
  ledOff(PIN_LED0);
  ledOff(PIN_LED1);
  ledOff(PIN_LED2);
  ledOff(PIN_LED3);

  /* Set error LED. */
  if (error_flag == 1)
  {
    Led_isError(true);
  }
}

/**
   @brief GNSS tracker loop

   @details Positioning is performed for the first 300 seconds after setup.
            After that, in each loop processing, it sleeps for SleepSec
            seconds and performs positioning ActiveSec seconds.
            The gnss_tracker use SatelliteSystem sattelites for positioning.\n\n

            Positioning result is notificated in every IntervalSec second.
            The result formatted to NMEA will be saved on SD card if the
            parameter NmeaOutFile is TRUE, or/and output to UART if the
            parameter NmeaOutUart is TRUE. NMEA is buffered for each
            notification. Write at once when ActiveSec completes. If SleepSec
            is set to 0, positioning is performed continuously.
*/
void loop() {
  // static
  static bool PosFixflag = false;
  static char *pNmeaBuff = NULL;
  static int WriteCounter = NUMBER_OF_CACHE;

  /* Check update. */
  if (Gnss.waitUpdate(POSITIONING_INTERVAL * 1000))
  {
    /* Get NavData. */
    SpNavData NavData;
    Gnss.getNavData(&NavData);

    /* Position Fixed?? */
    bool LedSet = ((NavData.posDataExist) && (NavData.posFixMode != 0));
    if (PosFixflag != LedSet)
    {
      Led_isPosfix(LedSet);
      PosFixflag = LedSet;
    }
    if (PosFixflag) {
        // GPS, QZ_L1CA --> LED1
        if (0 != (NavData.posSatelliteType & QZ_L1CA) ||
            0 != (NavData.posSatelliteType & GPS)) {
          ledOn(PIN_LED1);
        } else {
          ledOff(PIN_LED1);  
        }
        // QZ_L1S --> LED2
        if (0 != (NavData.posSatelliteType & QZ_L1S)) {
          ledOn(PIN_LED2);
        } else {
          ledOff(PIN_LED2);  
        }
    }
    
    /* Convert Nav-Data to Nmea-String. */
    String NmeaString = getNmeaGga(&NavData);
    if (strlen(NmeaString.c_str()) == 0)
    {
      /* Error case. */
      APP_PRINT_E("getNmea error\n");
      Led_isError(true);
    }
    else
    {
      // Alloc buffer.
      if (pNmeaBuff == NULL) {
        pNmeaBuff = (char*)malloc(MEMORY_BUFFER_SIZE);
        if (pNmeaBuff != NULL) {
          memset(pNmeaBuff, 0x00, MEMORY_BUFFER_SIZE); // Clear Buffer
        }
      }

      // Store Nmea Data to buffer.
      if (pNmeaBuff != NULL && strlen(pNmeaBuff) + NmeaString.length() < MEMORY_BUFFER_SIZE)  {
        strncat(pNmeaBuff, NmeaString.c_str(), MEMORY_BUFFER_SIZE);
        WriteCounter -= 1;
      } else {
        // ERROR : no capacity to write
        APP_PRINT_E("error: no capacity to write\n");
        Led_isError(true);
        WriteCounter = 0; // emergency write to Flash.
      }
    }

    // log
    APP_PRINT_I("Write Counter : ");
    APP_PRINT_I(WriteCounter);
    APP_PRINT_I(" / Sat Type : ");
    APP_PRINT_I(NavData.posSatelliteType);
    APP_PRINT_I("\n");

    /* Write NMEA data to Flash */
    if (WriteCounter <= 0 && NULL != pNmeaBuff) {
      /* Write Nmea Data. */
      Led_isStorageAccess(true);
      unsigned long WriteSize = WriteChar(pNmeaBuff, FilenameTxt, (FILE_WRITE | O_APPEND));
      Led_isStorageAccess(false);

      /* Check result. */
      if (WriteSize != strlen(pNmeaBuff)) {
        APP_PRINT_E("error: wrong size for flash write");
        Led_isError(true);
      }

      /* Reset */
      memset(pNmeaBuff, 0x00, MEMORY_BUFFER_SIZE); // Clear Buffer
      WriteCounter = NUMBER_OF_CACHE;
    }
  }
}
