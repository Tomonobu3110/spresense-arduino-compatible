/**
 * @file tiny_filer.ino
 * @author Tomonobu.Saito@gmail.com
 * @brief Tiny Filer to explore in Flash device.
 */

#include <Arduino.h>
#include <File.h>
#include <Flash.h>
#include <XModem.h>

// const
#define PROMPT "cmd('0':goto root, num:show, 'd'+num:delete, 'y'+num:Ymodem)>"
#define MOUNT_POINT "/mnt/spif/"
#define MAX_ENTRY_POINT 64
#define MAX_PATH_LENGTH 64

#define COMMAND_FLAG_SHOW ((uint32_t)(0x0000 << 16))
#define COMMAND_FLAG_DEL  ((uint32_t)(0x0001 << 16))
#define COMMAND_FLAG_DWLD ((uint32_t)(0x0010 << 16)) // Download w/ YModem
#define COMMAND_FLAG_NONE ((uint32_t)(0x0100 << 16))
#define COMMAND_FLAG_ERR  ((uint32_t)(0x1000 << 16))

// structure
typedef struct {
  bool isvalid;  // availability of this EntryPoint
  char path[MAX_PATH_LENGTH]; // path
  bool isdir;
  int  size;
} EntryPoint;

// global
EntryPoint entryPoints[MAX_ENTRY_POINT]; /* entry point list */
File current; /* current directory */

XModem ymodem(&Serial, ModeYModem); /* YModem */

// Print all file and directory names.
// like 'ls' command
void printDirectory(File dir) {
  memset(entryPoints, 0, sizeof(entryPoints)); // all reset
  int idx = 1;
  while (idx < MAX_ENTRY_POINT) {
    File entry = dir.openNextFile();
    // end check (and break)
    if (!entry) {
      dir.rewindDirectory();
      break;
    }
    // copy to entry point
    // Serial.println(entry.name());
    strncpy(entryPoints[idx].path, entry.name() + strlen(MOUNT_POINT), MAX_PATH_LENGTH);
    entryPoints[idx].isdir = entry.isDirectory();
    entryPoints[idx].size  = entry.size();
    entryPoints[idx].isvalid = true;
    entry.close();
    
    // print path
    Serial.print("  ");
    Serial.print(idx);
    Serial.print(" : ");
    Serial.print(entryPoints[idx].path);

    // print attribute
    if (entryPoints[idx].isdir) {
      Serial.println(" <DIR>");
    } else {
      Serial.print(" ");
      Serial.print(entryPoints[idx].size, DEC);
      Serial.println(" bytes");
    }
    
    idx++;
  } // end of while
}

// Print contents of text file.
// like 'cat' command
void printContents(File file) {
  Serial.println("-----");
  /* read file. */
  int len = 1; // dummy
  char buf[128 + 1]; // +1 for centinel(\0)
  while (0 < len) {
    memset(buf, 0, sizeof(buf));
    len = file.read(buf, sizeof(buf) - 1);
    Serial.print(buf);
  }
  Serial.println("[EOF]");
  Serial.println("-----");
}

// Queue command from serial
// upper 16 bit : flags
//   0x 0000 xxxx : show   (COMMAND_FLAG_SHOW)
//   0x 0001 xxxx : delete (COMMAND_FLAG_DEL)
//   0x 0100 xxxx : none   (COMMAND_FLAG_NONE)
//   0x 1000 xxxx : error  (COMMAND_FLAG_ERR)
// lower 16 bit : command No
uint32_t QueueCommand()
{
  static char buf[64];
  static int  idx = 0;

  // clear buffer
  if (0 == idx) { 
      memset(buf, 0, sizeof(buf));
  }

  // read buffer
  while (0 < Serial.available()) {
    // queue
    char c = (char)Serial.read();
    Serial.println(c);

    // if ENTER key then parse command.
    if ('\n' == c || '\r' == c) {
      buf[idx] = 0;
      uint32_t retval = ParseCommand(buf, idx);
      idx = 0;
      return retval;
    }

    // otherwise, just queue it.
    buf[idx++] = c;
  }
  
  return COMMAND_FLAG_NONE;
}

// Perse buffer to get command
// upper 16 bit : flags
//   0x 0000 xxxx : show   (COMMAND_FLAG_SHOW)
//   0x 0001 xxxx : delete (COMMAND_FLAG_DEL)
//   0x 0100 xxxx : none   (COMMAND_FLAG_NONE)
//   0x 1000 xxxx : error  (COMMAND_FLAG_ERR)
// lower 16 bit : command No
uint32_t ParseCommand(const char* buffer, int length)
{
  // error check (just in case)
  if (length <= 0) {
    return COMMAND_FLAG_ERR;
  }
  
  // get 1st character of command string
  char c = (char)buffer[0];
  if ('0' <= c && c <= '9') {
    return strtol(buffer, NULL, 10) | COMMAND_FLAG_SHOW;
  }
  else if ('d' == c || 'D' == c) {
    return strtol((const char*)(buffer + 1), NULL, 10) | COMMAND_FLAG_DEL;
  }
  else if ('y' == c || 'Y' == c) {
    return strtol((const char*)(buffer + 1), NULL, 10) | COMMAND_FLAG_DWLD;
  }
  return COMMAND_FLAG_ERR;
}

// setup
void setup() {
  /* Open serial communications and wait for port to open */
  Serial.begin(115200);
  while (!Serial) {
    ; /* wait for serial port to connect. Needed for native USB port only */
  }

  /* Show root directory of Flash */
  current = Flash.open("/");
  printDirectory(current);
  Serial.print(PROMPT);
}

// loop
void loop() {
  if (0 < Serial.available()) {
    // get commad number
    uint32_t cmd_no = QueueCommand();
    if (0 < (COMMAND_FLAG_NONE & cmd_no)) {
      return; // no command yet.
    }
    
    // execute command
    if (0 == cmd_no) {
      // goto root again.
      Serial.println("Back to root directory.");
      current = Flash.open("/");
      printDirectory(current);
    }
    else {
      // show file or enter directory
      if (cmd_no < MAX_ENTRY_POINT && entryPoints[cmd_no].isvalid) {
        if (entryPoints[cmd_no].isdir) {
          current.close();
          Serial.print("Move to ");
          Serial.println(entryPoints[cmd_no].path);
          current = Flash.open(entryPoints[cmd_no].path);
          printDirectory(current);
        } else {
          Serial.print("Show : ");
          Serial.println(entryPoints[cmd_no].path);
          File tmp = Flash.open(entryPoints[cmd_no].path);
          printContents(tmp);
          tmp.close();
          printDirectory(current);        
        }
      }
      // delete file
      else if (0 < (COMMAND_FLAG_DEL & cmd_no)) {
        uint32_t target = cmd_no & 0xffff;
        Serial.print("delete : ");
        Serial.println(target);
        if (target < MAX_ENTRY_POINT && entryPoints[target].isvalid){
          if (entryPoints[target].isdir) {
            // delete directory
            Flash.rmdir(entryPoints[target].path);
          }
          else {
            // delete file
            Flash.remove(entryPoints[target].path);
          }
          Serial.print("Remove : ");
          Serial.println(entryPoints[target].path);
          printDirectory(current);
        }
      }
      // Download file by YModem
      else if (0 < (COMMAND_FLAG_DWLD & cmd_no)) {
        uint32_t target = cmd_no & 0xffff;
        Serial.print("Download(YModem) : ");
        Serial.println(target);        
        if (target < MAX_ENTRY_POINT && entryPoints[target].isvalid)
        {
          if (entryPoints[target].isdir) {
            Serial.println("ERROR : this is directory.");
          }
          else {
            // download file...
            File f = Flash.open(entryPoints[target].path);
            if (f) {
              Serial.println("Download(YModel) start...");
              ymodem.sendFile(f, "tracker.ini");
              f.close();
            } 
            else {
              Serial.print("ERROR : Cannot open file : ");
              Serial.println(entryPoints[target].path);
            }
          }
        }
      }
      // error
      else {
        Serial.print("invalid cmd number : ");
        Serial.print(cmd_no);
        Serial.print(" / 0x");
        Serial.println(cmd_no, HEX);
      }
    }
    
    // show prompt for next input
    Serial.print(PROMPT);
  }
}
