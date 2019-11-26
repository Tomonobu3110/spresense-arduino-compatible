/*
 *  gnss_tracker.ino - GNSS tracker example application
 *  Copyright 2018 Sony Semiconductor Solutions Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file gnss_tracker.ino
 * @author Sony Semiconductor Solutions Corporation
 * @brief GNSS tracker example application
 * @details The gnss_tracker is a sample sketch that performs GPS positioning by
 *          intermittent operation.
 */

#include "gnss_dumper.h"
#include "gnss_nmea.h"
#include "gnss_file.h"

/* Directory on flash */
#define DATA_DIR_NAME    "gnss_tracker/"

/* Config file */
#define CONFIG_FILE_NAME    DATA_DIR_NAME "tracker.ini"  /**< Config file name */
#define CONFIG_FILE_SIZE    4096           /**< Config file size */

/* Index file */
#define INDEX_FILE_NAME    DATA_DIR_NAME "index.ini"     /**< Index file name */
#define INDEX_FILE_SIZE    16              /**< Index file size */

/* NMEA file */
#define NMEA_FILE_NAME    DATA_DIR_NAME "%08d.txt"       /**< NMEA file name */

#define STRING_BUFFER_SIZE  128            /**< %String buffer size */
#define NMEA_BUFFER_SIZE    128            /**< NMEA buffer size */
#define OUTPUT_FILENAME_LEN 32             /**< Output file name length */

#define SERIAL_BAUDRATE     115200         /**< Serial baud rate */

#define SEPARATOR           0x0A           /**< Separator */

char FilenameTxt[OUTPUT_FILENAME_LEN];  /**< Output file name */
char FilenameBin[OUTPUT_FILENAME_LEN];  /**< Output binary file name */

/**
 * @brief Turn on / off the LED0 for CPU active notification.
 */
static void Led_isActive(void)
{
  static int state = 1;
  if (state == 1)
  {
    ledOn(PIN_LED0);
    state = 0;
  }
  else
  {
    ledOff(PIN_LED0);
    state = 1;
  }
}

/**
 * @brief Turn on / off the LED1 for positioning state notification.
 * 
 * @param [in] state Positioning state
 */
static void Led_isPosfix(bool state)
{
  if (state == 1)
  {
    ledOn(PIN_LED1);
  }
  else
  {
    ledOff(PIN_LED1);
  }
}

/**
 * @brief Turn on / off the LED2 for file SD access notification.
 * 
 * @param [in] state SD access state
 */
static void Led_isSdAccess(bool state)
{
  if (state == 1)
  {
    ledOn(PIN_LED2);
  }
  else
  {
    ledOff(PIN_LED2);
  }
}

/**
 * @brief Turn on / off the LED3 for error notification.
 * 
 * @param [in] state Error state
 */
static void Led_isError(bool state)
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
 * @brief Get file number and output to serial
 */
int PrintFileNumber(void)
{
  char IndexData[INDEX_FILE_SIZE];
  int ReadSize = 0;

  APP_PRINT("index number --> ");
  /* Open index file. */
  memset(IndexData, 0, INDEX_FILE_SIZE);
  ReadSize = ReadChar(IndexData, INDEX_FILE_SIZE, INDEX_FILE_NAME, FILE_READ);
  if (ReadSize != 0)
  {
    APP_PRINT(IndexData);
    APP_PRINT("\n");
    return strtoul(IndexData, NULL, 10);
  }
  else {
    APP_PRINT("ERRROR: Cannot read\n");
    return (-1);
  }
}

void DumpNmeaFile(int index)
{
  /* Create file name. */
  memset(FilenameTxt, 0, sizeof(FilenameTxt));
  snprintf(FilenameTxt, sizeof(FilenameTxt), NMEA_FILE_NAME, index);
  APP_PRINT(FilenameTxt);
  APP_PRINT("\n");

  int read_result = 0;
  File myFile;
  
  /* Open file. */
  if (Flash.exists(FilenameTxt) == false) {
    APP_PRINT(FilenameTxt);
    APP_PRINT(" Not exist.\n");
    return;
  }
  myFile = Flash.open(FilenameTxt);
  if (myFile == NULL)
  {
    /* if the file didn't open, print an error. */
    APP_PRINT(FilenameTxt);
    APP_PRINT(" Open error.\n");
    return;
  }
  else
  {
    /* read file. */
    int len = 1; // dummy
    char buf[128 + 1]; // +1 for centinel(\0)
    memset(buf, 0, sizeof(buf));
    while (0 < len) {
      len = myFile.read(buf, sizeof(buf) - 1);
      APP_PRINT(buf);
    }
    
    /* Close file. */
    myFile.close();
  }
}

/**
 * @brief Activate GNSS device and setup positioning
 */
void setup()
{
  /* Initialize the serial first for debug messages. */
  /* Set serial baudeate. */
  Serial.begin(SERIAL_BAUDRATE);
  APP_PRINT("GNSS DUMPER version 1.00\n");
  APP_PRINT("Initializing...\n");

  /* Wait mode select. */
  sleep(3);

  /* Turn on all LED:Setup start. */
  ledOn(PIN_LED0);
  ledOn(PIN_LED1);
  ledOn(PIN_LED2);
  ledOn(PIN_LED3);

  /* Mount SD card. */
  BeginSDCard();

  /* Print file number */
  int num = 0;
  num = PrintFileNumber();

  int i = 0;
  for (i = 0; i < num; ++i) {
    DumpNmeaFile(i);
  }

  /* Turn off all LED:Setup done. */
  ledOff(PIN_LED0);
  ledOff(PIN_LED1);
  ledOff(PIN_LED2);
  ledOff(PIN_LED3);
  
  APP_PRINT("Done...\n");
}

/**
 * @brief dummy loop
 */
void loop() {
}
