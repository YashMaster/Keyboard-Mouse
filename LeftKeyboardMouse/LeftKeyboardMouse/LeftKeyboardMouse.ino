#include <Arduino.h>

// Using micro
#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

const int CE = 13;
const int CSN = 12;
const int IRQ = 2;
// thumb -> pinky
const uint8_t FINGER_PINS[5] = {6,7,8,9,10};
const uint8_t REPEAT_SEND_DELAY = 400;
const uint8_t NUMBER_RESENDS = 3;

// Args = [ce_pin, csn_pin]
RF24 radio(CE, CSN);

const uint64_t pipe = 0xE8E8F0F0E1LL;

void setup(void){
 Serial.begin(9600);
 radio.begin();
 radio.openWritingPipe(pipe);
}

uint8_t last_hand = 0, current_hand = 0;
uint8_t send_counter = 0;
unsigned long last_send_time = 0;

void loop(void){
  current_hand = 0;
  for(int index = 0; index < 5; ++index){
    current_hand = current_hand | (digitalRead(FINGER_PINS[index]) << index);
  }

  if(current_hand != last_hand){
    radio.write(&current_hand, sizeof(current_hand));
//      Serial.println(current_hand, BIN);
    last_send_time = micros();
    last_hand = current_hand;
    send_counter = 1;
  } else{
    if(micros() - last_send_time > REPEAT_SEND_DELAY && send_counter < NUMBER_RESENDS){
      radio.write(&current_hand, sizeof(current_hand));
      last_send_time = micros();
      ++send_counter;
    }
  }
}
