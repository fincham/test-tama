#include <SmartResponseXE.h>
#include "RadioFunctions.h"
#include <EEPROM.h>

#define screen_wide 32
#define scrollback_lines 100

unsigned long beacon_interval = 10000;
unsigned long last_beacon = 0;

bool redraw_scrollback = true;
bool redraw_input = true;

bool sleeping = false;

char *parrot_sounds[] = {
  "SKRAAAAAAAK!",
  "SQUUUAAA!",
  "WAAK!",
  "TWOO!",
  "WAAAAK!",
  "AAARRK!"
};

size_t parrot_counter = 0;

size_t pkt_good = 0;
size_t pkt_bad = 0;
size_t beacons_sent = 0;

uint8_t scroll_up = 0;

uint8_t tail = 0;
bool move_tail = false;

char pkt_buffer[256];
char lcd_buffer[100];
char radio_buffer[128];
char typing_buffer[128];
char log_buffer[32];

char seen_id[30];
char seen_handle[30][9];
unsigned long seen_last[30];

char scrollback[scrollback_lines][screen_wide + 1];

char handle[16];
char handle_copy[16];

uint8_t pkt_buffer_head = 0;
uint8_t typing_buffer_head = 0;
int scrollback_head = 0;
bool has_been_full = false;

uint8_t unit_id = 0;

void clear_lcd() {
  for (int m = 0; m < 8; m++) {
    SRXEWriteString(0, 16 * m, "                                ", FONT_MEDIUM, 3, 0);
  }
  SRXERectangle(0, 119, 127, 16, 0, 1); /* clear input line */
}

void reboot(){
   WDTCSR |= _BV(WDCE) | _BV(WDE);
   WDTCSR = _BV(WDP2) | _BV(WDP1) | _BV(WDP0) | _BV(WDE);
   for(;;){}
}

void setup() {
  MCUSR &= ~(1 << WDRF);
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR = 0;
  delay(200);
  memset(handle, 0, 16);
  SRXEInit(0xe7, 0xd6, 0xa2);
  sprintf(lcd_buffer, "Booting...");
  SRXEWriteString(0, 0, lcd_buffer, FONT_MEDIUM, 3, 0);

  if (EEPROM.read(1) != 0xaa || EEPROM.read(2) != 0x55) { /* not yet provisioned at all */
    sprintf(lcd_buffer, "OK, time to enter my unit ID: ");
    SRXEWriteString(0, 0, lcd_buffer, FONT_MEDIUM, 3, 0);

    while (true) {
      unsigned char key = 0;
      key = SRXEGetKey();

      if (key != 0) {
        if (key == 0x08 ) { /* backspace */
          if (typing_buffer_head > 0) {
            typing_buffer[typing_buffer_head--] = 0;
            redraw_input = true;
          }
        } else if (key >= 48 && key <= 57) { /* number */
          if (typing_buffer_head < 2) {
            typing_buffer[typing_buffer_head++] = key;
            typing_buffer[typing_buffer_head] = 0;
          }
        } else if (key == 0xf9) { /* "send" */
          break;
        }

      }
      sprintf(lcd_buffer, "%s     ", typing_buffer);
      SRXEWriteString(0, 16, lcd_buffer, FONT_MEDIUM, 3, 0);
    }
    sscanf(typing_buffer, "%d", &unit_id);
    sprintf(lcd_buffer, "OK I'm unit %i", unit_id);
    SRXEWriteString(0, 16, lcd_buffer, FONT_MEDIUM, 3, 0);
    typing_buffer_head = 0;
    EEPROM.write(0, unit_id);
    EEPROM.write(1, 0xaa);
    EEPROM.write(2, 0x55);
    EEPROM.write(3, 0); /* has handle */
    EEPROM.write(4, 0); /* kaka or human? */
    delay(2000);
    SRXESleep();

  } else {

    unit_id = EEPROM.read(0);
  }

  if (EEPROM.read(3) == 0) {
    SRXESleep();
  }

  while (EEPROM.read(3) == 0) { /* need to set up handle */
    /*SRXERectangle(0, 0, 127, 135, 3, 0);*/
    SRXEWriteString(0, 0,  "Aaaah! You made it to Kawaiicon!", FONT_MEDIUM, 3, 0);
    SRXEWriteString(0, 16, "We are super glad you're here :D", FONT_MEDIUM, 3, 0);
    SRXEWriteString(0, 48, "Say... if you were a mischievous", FONT_MEDIUM, 3, 0);
    SRXEWriteString(0, 64, "native parrot, what would you be", FONT_MEDIUM, 3, 0);
    SRXEWriteString(0, 80, "called?", FONT_MEDIUM, 3, 0);

    while (true) {
      unsigned char key = 0;
      key = SRXEGetKey();

      if (key != 0) {
        if (key == 0x08 ) { /* backspace */
          if (typing_buffer_head > 0) {
            typing_buffer[typing_buffer_head--] = 0;
            redraw_input = true;
          }
        } else if (key == 0xf9) { /* "send" */
          break;
        } else if ((key >= 97 && key <= 122) || (key >= 48 && key <= 57) || (key >= 65 && key <= 90)) { /* a typable character */
          if (typing_buffer_head < 15) {
            typing_buffer[typing_buffer_head++] = key;
            typing_buffer[typing_buffer_head] = 0;
            redraw_input = true;
          }
        }
      }

      if (redraw_input) {
        sprintf(lcd_buffer, "      %s", typing_buffer);
        SRXEWriteString(0, 119, lcd_buffer, FONT_MEDIUM, 3, 0);
        SRXERectangle(typing_buffer_head * 4 + 24, 119, 4, 16, 2, 1); /* draw cursor */
        SRXERectangle(typing_buffer_head * 4 + 28, 119, 4, 16, 0, 1); /* clear righthand side */

        sprintf(lcd_buffer, "Ok");
        SRXEWriteString(336 + 12, 119, lcd_buffer, FONT_MEDIUM, 0, 3);
        redraw_input = false;
      }
    }
    redraw_input = true;
    clear_lcd();

    SRXEWriteString(0, 0,  "Ok! So if you were a parrot we'd", FONT_MEDIUM, 3, 0);
    SRXEWriteString(0, 16, "totally just call you...", FONT_MEDIUM, 3, 0);
    SRXEWriteString(0, 80, "           ...right?", FONT_MEDIUM, 3, 0);

    SRXEWriteString(192 - typing_buffer_head * 6, 48, typing_buffer, FONT_MEDIUM, 3, 0);

    SRXEWriteString(336, 119, "Yes!", FONT_MEDIUM, 0, 3);
    SRXEWriteString(336 + 9, 90, "No!", FONT_MEDIUM, 0, 3);
    while (true) {
      unsigned char key = 0;
      key = SRXEGetKey();
      if (key != 0) {
        if (key == 0xf9) { /* "ok" */
          sprintf(handle, "%s", typing_buffer);
          typing_buffer_head = 0;
          typing_buffer[0] = 0;
          EEPROM.write(3, 1);
          for (int g = 0; g < 15; g++) {
            EEPROM.write(5 + g, handle[g]);
          }
          break;
        } else if (key == 0xf8) { /* "no" */
          break;
        }
      }
    }
    clear_lcd();
    reboot();
  }
  for (int g = 0; g < 15; g++) {
    handle[g] = EEPROM.read(5 + g);
  }
  handle[16] = 0;
  rfBegin(11);
  sprintf(typing_buffer, "");
  memset(seen_id, 0, 30);
  /*sprintf(scrollback[0], "-- There is no more. SKRAAAK. --");*/
  sprintf(lcd_buffer, "<!> %s has entered the chat...", handle);
  SRXEWriteString(0, 0, lcd_buffer, FONT_MEDIUM, 3, 0);

}

void radio_log(char* log_message) {
  sprintf(radio_buffer, "\x01\x03%c\xff%c%s\x04", unit_id, strlen(log_message), log_message); /* start - packet type 3 (log) - sender - receiver (all) - length - log str - end */
  rfPrint(radio_buffer);
}

void radio_chat(char* log_message) {
  sprintf(radio_buffer, "\x01\x09%c\xff%c%s\x04", unit_id, strlen(log_message), log_message); /* start - packet type 3 (log) - sender - receiver (all) - length - log str - end */
  rfPrint(radio_buffer);
}

void add_scrollback(char* message) {
  if (strlen(handle) + 3 + strlen(message) < screen_wide) {
    sprintf(scrollback[scrollback_head++], "<%s> %s", handle, message);
    if (scrollback_head >= scrollback_lines) {
      scrollback_head = 0;
      move_tail = true;
    }
    if (move_tail) {
      tail = scrollback_head + 1;
    }
  } else {
    bool first_line = true;
    uint8_t indent = strlen(handle) + 3;
    for (int m = 0; m < strlen(message); m = m + (screen_wide - indent)) {
      memset(scrollback[scrollback_head], 0, 32);
      if (first_line) {
        sprintf(scrollback[scrollback_head], "<%s>  ", handle);
        first_line = false;
      } else {
        indent = 2;
        sprintf(scrollback[scrollback_head], "   ");
      }
      size_t max_to_copy = strlen(message + m);
      memcpy(scrollback[scrollback_head] + indent, message + m, min(max_to_copy, screen_wide - indent));
      scrollback[scrollback_head][32] = 0;
      scrollback_head++;
      if (scrollback_head >= scrollback_lines) {
        scrollback_head = 0;
        move_tail = true;
      }
      if (move_tail) {
        tail = scrollback_head + 1;
      }
    }
  }

  if (scrollback_head >= 7) {
    has_been_full = true;
  }

}

void loop() {
  static size_t radio_bytes_waiting = 0;
  unsigned char key = 0;
  key = SRXEGetKey();

  /* THE CHAT STUFF */
  if (key != 0) {
    if (sleeping) {
      SRXEPowerUp();
      redraw_input = true;
      redraw_scrollback = true;
      sleeping = false;
    } else {
      if (key == 0x08 ) { /* backspace */
        if (typing_buffer_head > 0) {
          typing_buffer[typing_buffer_head--] = 0;
          redraw_input = true;
          if (scroll_up > 0) {
            scroll_up = 0;
            redraw_scrollback = true;
          }
        }
      } else if (key == 0x1) { /* "menu" */
        sprintf(handle_copy, "%s", handle);
        sprintf(handle, "!");
        if (EEPROM.read(4) == 0) {
          add_scrollback("Blorp! You've become a Human Operator.");
          EEPROM.write(4, 1);
        } else {
          add_scrollback("Glarp! You've become a Kaka Operator.");
          EEPROM.write(4, 0);
        }
        uint8_t friends = 0;
        for (int j = 0; j < 30; j++) {
          if (seen_id[j] != 0) { /* a known friend! */
            friends++;
          }
        }
        if (friends > 1 || friends == 0) {
          sprintf(lcd_buffer, "BTW, so far you've met %i new friends.", friends);
        } else {
          sprintf(lcd_buffer, "BTW, so far you've met %i new friend.", friends);
        }
        add_scrollback(lcd_buffer);
        sprintf(handle, "%s", handle_copy);
        redraw_scrollback = true;
      } else if (key == 0xf0) { /* "sleep" */
        SRXEPowerDown();
        sleeping = true;
      } else if (key == 0xf1) { /* "friend list" */
        sprintf(handle_copy, "%s", handle);
        sprintf(handle, "!");
        add_scrollback("Friends out of earshot:");
        for (int j = 0; j < 30; j++) {
          if (seen_id[j] != 0 && seen_last[j] == 0) { /* a known friend! */
            sprintf(lcd_buffer, " - %s", seen_handle[j]);
            add_scrollback(lcd_buffer);
          }
        }
        add_scrollback("And here are your nearby    friends:");
        for (int j = 0; j < 30; j++) {
          if (seen_id[j] != 0 && seen_last[j] != 0) { /* a known friend! */
            sprintf(lcd_buffer, " - %s", seen_handle[j]);
            add_scrollback(lcd_buffer);
          }
        }
        sprintf(handle, "%s", handle_copy);
        redraw_scrollback = true;

      } else if (key == 0xf5) { /* "scroll up" */
        if (((move_tail && scroll_up < (scrollback_lines - 7)) || (!move_tail && scroll_up < scrollback_head - 7)) && has_been_full) {
          scroll_up++;
          redraw_scrollback = true;
        }
      } else if (key == 0xf8) { /* "scroll down" */
        if (scroll_up > 0) {
          scroll_up--;
          redraw_scrollback = true;
        }
      } else if (key == 0xf9) { /* "send" */
        if (typing_buffer_head > 0) {
          for (int m = 1; m < typing_buffer_head; m++) { /* clear any trailing spaces */
            if (typing_buffer[typing_buffer_head - m] == 32) {
              typing_buffer[typing_buffer_head - m] = 0;
            } else {
              break;
            }
          }
          radio_chat(typing_buffer);
          add_scrollback(typing_buffer);
          typing_buffer_head = 0;
          typing_buffer[0] = 0;
          typing_buffer[1] = 0;
          SRXERectangle(0, 119, 127, 16, 0, 1); /* clear input line */
          redraw_input = true;
          redraw_scrollback = true;
        }
      } else if (EEPROM.read(4) == 1 && (key >= 32 && key <= 126)) { /* a typable character */
        if (scroll_up > 0) {
          scroll_up = 0;
          redraw_scrollback = true;
        }
        if (typing_buffer_head < 100) {
          typing_buffer[typing_buffer_head++] = key;
          typing_buffer[typing_buffer_head] = 0;
          redraw_input = true;
        }
      } else if (EEPROM.read(4) == 0 && key == 'y') { /* SQUARK */
        sprintf(typing_buffer, "%s", parrot_sounds[parrot_counter]);
        typing_buffer_head = strlen(parrot_sounds[parrot_counter]);
        parrot_counter++;
        if (parrot_counter > 5) {
          parrot_counter = 0;
        }
        redraw_input = true;
        SRXEWriteString(0, 119, "                    ", FONT_MEDIUM, 3, 0);
      } else if (EEPROM.read(4) == 0 && (key >= 32 && key <= 126)) { /* no SQUARK :( */
        SRXERectangle(16, 16, 127 - 32, 70, 0, 1); /* draw dialog */
        SRXERectangle(16, 16, 127 - 32, 70, 3, 0); /* draw dialog */
        SRXEWriteString(81, 32, "Press Y to SKRAAAK.", FONT_MEDIUM, 3, 0);
        delay(4000);
        redraw_scrollback = true;
      }

    }
  }




  /* draw input line */
  if (redraw_input) {
    unsigned int input_offset = 0;
    if (typing_buffer_head > 20) {
      input_offset = typing_buffer_head - 20;
    }

    sprintf(lcd_buffer, "[#kc] %s", typing_buffer + input_offset);

    SRXERectangle(0, 114, 127, 3, 2, 1); /* divider */
    SRXEWriteString(0, 119, lcd_buffer, FONT_MEDIUM, 3, 0);
    SRXERectangle(min(20, typing_buffer_head) * 4 + 24, 119, 4, 16, 2, 1); /* draw cursor */
    SRXERectangle(min(20, typing_buffer_head) * 4 + 28, 119, 4, 16, 0, 1); /* clear righthand side */

    SRXERectangle(111, 116, 16, 19, 2, 1);
    sprintf(lcd_buffer, "Send");
    SRXEWriteString(336, 119, lcd_buffer, FONT_MEDIUM, 0, 2);

    redraw_input = false;
  }



  /* draw scrollback */
  if (redraw_scrollback) {
    int window_top = 0;
    if (scrollback_head >= 7 && has_been_full) {
      window_top = scrollback_head - 7 - scroll_up;
    }
    if (scrollback_head < 7 && has_been_full) {
      window_top = scrollback_lines - 7 + scrollback_head - scroll_up;
    }
    size_t window_height = 7;
    if (scroll_up > 0) {
      window_height = 6;
    }
    for (int i = 0; i < window_height; i++) {
      sprintf(lcd_buffer, "%s", scrollback[(window_top + i) % (scrollback_lines)]);
      SRXEWriteString(0, 16 * i, "                                ", FONT_MEDIUM, 3, 0); /* less buggy line clear */
      SRXEWriteString(0, 16 * i, lcd_buffer, FONT_MEDIUM, 3, 0);
    }
    if (scroll_up > 0) {
      SRXEWriteString(0, 16 * 6, "                                ", FONT_MEDIUM, 3, 0); /* less buggy line clear */
      SRXEWriteString(108, 16 * 6, "  -- more --   ", FONT_MEDIUM, 2, 0);
    }


    redraw_scrollback = false;
  }


  /* THE RADIO STUFF */

  /* scoop any waiting radio bytes in to the packet buffer */
  radio_bytes_waiting = rfAvailable();
  while (radio_bytes_waiting) {
    pkt_buffer[pkt_buffer_head++] = rfRead();
    radio_bytes_waiting--;
  }


  /* look for any Start Of Header marks indicating a packet */
  for (uint8_t i = 0; i < 255; i++) {
    if (pkt_buffer[i] == 0x01) { /* 0x01 is "start of header" */
      size_t payload_length = pkt_buffer[i + 4];
      if (pkt_buffer[i + payload_length + 5] == 0x04) { /* 0x04 is "end of transmission" */
        pkt_good++;

        /* decide what to do based on incoming radio message */
        if (pkt_buffer[i + 1] == 0x01) { /* 0x01 = "hello" */
          int found_offset = 64;
          for (int j = 0; j < 30; j++) {
            if (seen_id[j] == pkt_buffer[i + 2]) { /* a known friend! */
              found_offset = j;
              break;
            }
          }
          if (found_offset == 64) { /* not a known friend */
            for (int j = 0; j < 30; j++) {
              if (seen_id[j] == 0) { /* empty slot */
                found_offset = j;
                seen_id[j] = pkt_buffer[i + 2];
                seen_last[j] = millis();
                pkt_buffer[i + 5 + payload_length] = 0;
                sprintf(seen_handle[j], "%s", pkt_buffer + i + 5);
                sprintf(handle_copy, "%s", handle);
                sprintf(handle, "+");
                sprintf(log_buffer, "Made new friend %s!", seen_handle[j]);
                add_scrollback(log_buffer);
                redraw_scrollback = true;
                sprintf(handle, "%s", handle_copy);
                break;
              }
            }
          } else { /* check if they're coming back */
            if (seen_last[found_offset] == 0) {
              sprintf(handle_copy, "%s", handle);
              sprintf(handle, "+");
              sprintf(log_buffer, "%s came back :3", seen_handle[found_offset]);
              add_scrollback(log_buffer);
              redraw_scrollback = true;
              sprintf(handle, "%s", handle_copy);
            }
            seen_last[found_offset] = millis();
          }
        } else if (pkt_buffer[i + 1] == 0x09) { /* 0x09 = "chat" */
          sprintf(handle_copy, "%s", handle);
          sprintf(handle, "???");
          for (int j = 0; j < 30; j++) {
            if (seen_id[j] == pkt_buffer[i + 2]) { /* a known friend! */
              sprintf(handle, "%s", seen_handle[j]);
              break;
            }
          }

          pkt_buffer[i + 5 + payload_length] = 0;
          sprintf(radio_buffer, "%s", pkt_buffer + i + 5);
          add_scrollback(radio_buffer);
          redraw_scrollback = true;
          sprintf(handle, "%s", handle_copy);
        }

        pkt_buffer[i + payload_length + 5] = 0; /* kill the start and end markers so this packet won't be parsed again */
        pkt_buffer[i] = 0;
        pkt_buffer_head = 0; /* move back to the start of the buffer... XXX think about this harder */
      }
    }
  }



  /* scan for any friends who have left */
  for (int j = 0; j < 30; j++) {
    if (seen_id[j] != 0) { /* a known friend! */
      if (seen_last[j] != 0 && millis() - seen_last[j] > 20000) {
        seen_last[j] = 0;
        sprintf(handle_copy, "%s", handle);
        sprintf(handle, "-");
        sprintf(log_buffer, "%s left the room :(", seen_handle[j]);
        add_scrollback(log_buffer);
        redraw_scrollback = true;
        sprintf(handle, "%s", handle_copy);
      }
    }
  }



  /* tell everyone around how cool we are */
  if (millis() - last_beacon > beacon_interval) {

    sprintf(radio_buffer, "\x01\x01%c\xff%c%s\x04", unit_id, strlen(handle), handle); /* start - packet type 1 (hello) - sender - receiver (all) - length 1 - id byte - end */

    rfPrint(radio_buffer);

    last_beacon = millis();

    beacons_sent++;
  }


}
