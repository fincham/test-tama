#include "RadioFunctions.h"
#include <SmartResponseXE.h>

char buf[100];
char serial_buf[100];
char serial_in = 0;

size_t count = 0;

void setup() { 
  delay(200);
  Serial1.begin(9600);
  rfBegin(11);
  Serial1.println("Egg Game RF sniffer online...");
  pinMode(PE0, OUTPUT);
  digitalWrite(PE0, HIGH);
}
 
void loop() {
  if (Serial1.available()) {
    serial_in = Serial1.read();
    buf[count] = serial_in;
    if (serial_in == 13) {
      buf[count] = 0;
      rfPrint(buf);
      sprintf(serial_buf, "TX %i\r\n", count);
      Serial1.print(serial_buf);
      count = 0;
    } else {
      count++;    
         
    }
  }

  if (rfAvailable()) {
    Serial1.print(rfRead());
  }
}
