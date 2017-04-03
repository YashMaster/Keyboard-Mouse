#include <Arduino.h>
#include <SPI.h>
#include <avr/pgmspace.h>
#include <Mouse.h>
#include "Keyboard.h"
#include "nRF24L01.h"
#include "RF24.h"
#include "I2Cdev.h"

const uint8_t MOUSE_SS = 5; 

const uint8_t SOFT_SPI_MISO_PIN_MOUSE = 0;
const uint8_t SOFT_SPI_MOSI_PIN_MOUSE = 1;
const uint8_t SOFT_SPI_SCK_PIN_MOUSE  = 7;
const uint8_t SPI_MODE_MOUSE = 3;

extern const unsigned short firmware_length;
extern const unsigned char firmware_data[];

//In order of pinky -> thumb
const uint8_t FINGER_PINS[5] = {11,10,9,8,6};

SPISettings mouse_settings(16000000, MSBFIRST, SPI_MODE3);
SPISettings radio_settings(10000000, MSBFIRST, SPI_MODE0);


// Registers
#define REG_Product_ID                           0x00
#define REG_Revision_ID                          0x01
#define REG_Motion                               0x02
#define REG_Delta_X_L                            0x03
#define REG_Delta_X_H                            0x04
#define REG_Delta_Y_L                            0x05
#define REG_Delta_Y_H                            0x06
#define REG_SQUAL                                0x07
#define REG_Pixel_Sum                            0x08
#define REG_Maximum_Pixel                        0x09
#define REG_Minimum_Pixel
#define REG_Shutter_Lower                        0x0b
#define REG_Shutter_Upper                        0x0c
#define REG_Frame_Period_Lower                   0x0d
#define REG_Frame_Period_Upper                   0x0e
#define REG_Configuration_I                      0x0f
#define REG_Configuration_II                     0x10
#define REG_Frame_Capture                        0x12
#define REG_SROM_Enable                          0x13
#define REG_Run_Downshift                        0x14
#define REG_Rest1_Rate                           0x15
#define REG_Rest1_Downshift                      0x16
#define REG_Rest2_Rate                           0x17
#define REG_Rest2_Downshift                      0x18
#define REG_Rest3_Rate                           0x19
#define REG_Frame_Period_Max_Bound_Lower         0x1a
#define REG_Frame_Period_Max_Bound_Upper         0x1b
#define REG_Frame_Period_Min_Bound_Lower         0x1c
#define REG_Frame_Period_Min_Bound_Upper         0x1d
#define REG_Shutter_Max_Bound_Lower              0x1e
#define REG_Shutter_Max_Bound_Upper              0x1f
#define REG_LASER_CTRL0                          0x20
#define REG_Observation                          0x24
#define REG_Data_Out_Lower                       0x25
#define REG_Data_Out_Upper                       0x26
#define REG_SROM_ID                              0x2a
#define REG_Lift_Detection_Thr                   0x2e
#define REG_Configuration_V                      0x2f
#define REG_Configuration_IV                     0x39
#define REG_Power_Up_Reset                       0x3a
#define REG_Shutdown                             0x3b
#define REG_Inverse_Product_ID                   0x3f
#define REG_Motion_Burst                         0x50
#define REG_SROM_Load_Burst                      0x62
#define REG_Pixel_Burst                          0x64

volatile byte xydat[4];
int16_t * deltax = (int16_t *) &xydat[0];
int16_t * deltay = (int16_t *) &xydat[2];

void adns_com_begin(){
  SPI.beginTransaction(mouse_settings);
  digitalWrite(MOUSE_SS, LOW);
}

void adns_com_end(){
  digitalWrite(MOUSE_SS, HIGH);
  SPI.endTransaction();
}

byte adns_read_reg(byte reg_addr){
  adns_com_begin();

  // send adress of the register, with MSBit = 0 to indicate it's a read
  SPI.transfer(reg_addr & 0x7f );
  delayMicroseconds(100); // tSRAD
  // read data
  byte data = SPI.transfer(0);

  delayMicroseconds(1); // tSCLK-NCS for read operation is 120ns
  adns_com_end();
  delayMicroseconds(19); //  tSRW/tSRR (=20us) minus tSCLK-NCS

  return data;
}

void adns_write_reg(byte reg_addr, byte data){
  adns_com_begin();

  //send adress of the register, with MSBit = 1 to indicate it's a write
  SPI.transfer(reg_addr | 0x80 );
  //sent data
  SPI.transfer(data);

  delayMicroseconds(20); // tSCLK-NCS for write operation
  adns_com_end();
  delayMicroseconds(100); // tSWW/tSWR (=120us) minus tSCLK-NCS. Could be shortened, but is looks like a safe lower bound
}

void adns_upload_firmware(){
  // send the firmware to the chip, cf p.18 of the datasheet
  // set the configuration_IV register in 3k firmware mode
  adns_write_reg(REG_Configuration_IV, 0x02); // bit 1 = 1 for 3k mode, other bits are reserved

  // write 0x1d in SROM_enable reg for initializing
  adns_write_reg(REG_SROM_Enable, 0x1d);

  // wait for more than one frame period
  delay(10); // assume that the frame rate is as low as 100fps... even if it should never be that low

  // write 0x18 to SROM_enable to start SROM download
  adns_write_reg(REG_SROM_Enable, 0x18);

  // write the SROM file (=firmware data)
  adns_com_begin();
  SPI.transfer(REG_SROM_Load_Burst | 0x80); // write burst destination adress
  delayMicroseconds(15);

  // send all bytes of the firmware
  unsigned char c;
  for(int i = 0; i < firmware_length; i++){
    c = (unsigned char)pgm_read_byte(firmware_data + i);
    SPI.transfer(c);
    delayMicroseconds(15);
  }
  adns_com_end();
}


void performStartup(void){
  adns_com_begin(); // ensure that the serial port is reset
  adns_com_end(); // ensure that the serial port is reset
  adns_write_reg(REG_Power_Up_Reset, 0x5a); // force reset
  delay(50); // wait for it to reboot
  // read registers 0x02 to 0x06 (and discard the data)
  adns_read_reg(REG_Motion);
  adns_read_reg(REG_Delta_X_L);
  adns_read_reg(REG_Delta_X_H);
  adns_read_reg(REG_Delta_Y_L);
  adns_read_reg(REG_Delta_Y_H);
  // upload the firmware
  adns_upload_firmware();
  delay(10);
  //enable laser(bit 0 = 0b), in normal mode (bits 3,2,1 = 000b)
  // reading the actual value of the register is important because the real
  // default value is different from what is said in the datasheet, and if you
  // change the reserved bytes (like by writing 0x00...) it would not work.
  byte laser_ctrl0 = adns_read_reg(REG_LASER_CTRL0);
  adns_write_reg(REG_LASER_CTRL0, laser_ctrl0 & 0xf0 );

  delay(1);

}

void dispRegisters(void){
  int oreg[7] = {
    0x00,0x3F,0x2A,0x02  };
  char* oregname[] = {
    "Product_ID","Inverse_Product_ID","SROM_Version","Motion"  };
  byte regres;

  adns_com_begin();

  int rctr=0;
  for(rctr=0; rctr<4; rctr++){
    SPI.transfer(oreg[rctr]);
    delay(1);
    Serial.println("---");
    Serial.println(oregname[rctr]);
    Serial.println(oreg[rctr],HEX);
    regres = SPI.transfer(0);
    Serial.println(regres,BIN);
    Serial.println(regres,HEX);
    delay(1);
  }
  digitalWrite(MOUSE_SS,HIGH);
  adns_com_end();
}

void check_and_move_mouse(){
  xydat[0] = (byte)adns_read_reg(REG_Delta_X_L);
  xydat[1] = (byte)adns_read_reg(REG_Delta_X_H);
  xydat[2] = (byte)adns_read_reg(REG_Delta_Y_L);
  xydat[3] = (byte)adns_read_reg(REG_Delta_Y_H);
  
  //Reduce sensitivity at higher speeds
  if(abs(*deltax) != 1){
    *deltax = *deltax / 2;
  }
  if(abs(*deltay) != 1){
    *deltay = *deltay / 2;
  }
  if(*deltax != 0 || *deltay != 0){
    Mouse.move(*deltax, *deltay, 0);
  }
}

const uint8_t RADIO_CSN = 12;
const uint8_t RADIO_CE = 13;
const uint8_t RADIO_IRQ = 2;
//Transceiver radio(RADIO_CE, RADIO_CSN, radio_settings);
RF24 radio(RADIO_CE, RADIO_CSN);
const uint64_t pipe = 0xE8E8F0F0E1LL;
uint8_t msg;

uint8_t left_hand_fingers = 0, left_hand_fingers_test = 0;
void check_radio(){
//  bool tx, fail, rx; 
//  radio.whatHappened(tx, fail, rx);
//
//  // Will ignore fail and tx because we will never send
//  // from this hand
//
//  if(rx){
//    radio.read(&left_hand_fingers, sizeof(left_hand_fingers));
////    Serial.println("Left Hand Only:");
////    Serial.println(left_hand_fingers, BIN);
//  }
//  radio.flush_rx();
  while(radio.available()){
    radio.read(&left_hand_fingers, sizeof(left_hand_fingers));
  }
}

//END MOUSE STUFF************************************

// Amount of time to wait between sending keys when key is being held down
// in micros
const unsigned long PAUSE_BETWEEN_HOLD = 25000;
// Amount of time to wait after sending first key before sending a bunch more
const unsigned long PAUSE_AFTER_PRESS = 400000;
// The amount of time to wait until sending character from initial
// press. Don't want this to be too quick in case there is a combo
// which is coming up
const unsigned long INITIAL_HOLDOFF = 55000;

const unsigned long HOLDOFF_BETWEEN_COMBOS = 15000;
const unsigned NUM_FINGERS = 10;

bool first_time = false;

unsigned long time;

// CHARACTER COMBO DEFINITIONS **********************************
const uint16_t NINE = 0b0000101110;
const uint16_t SLASH = 0b0110100000;
const uint16_t CTRL = 0b0000000001;
const uint16_t SHIFT = 0b1000000000;
const uint16_t ENTER = 0b0001100000;
const uint16_t TWO = 0b0110100100;
const uint16_t UP = 0b0001101000;
const uint16_t DOWN = 0b0010100100;
const uint16_t LESS_THAN = 0b0000100100;
const uint16_t BACK_SPACE = 0b0000000010;
const uint16_t SEMICOLON = 0b0000100010;
const uint16_t OPENING_CURLY = 0b0100101000;
const uint16_t QUOTE = 0b0000100110;
const uint16_t ONE = 0b0110101000;
const uint16_t COMMA = 0b0000100100;
const uint16_t TAB = 0b0100100000;
const uint16_t EIGHT = 0b0001101010;
const uint16_t LEFT = 0b0011100000;
const uint16_t FUNCTION = 0b1000010001;
const uint16_t SEVEN = 0b0001101100;
const uint16_t SIX = 0b0011100010;
const uint16_t SPACE = 0b0010000000;
const uint16_t PERIOD = 0b0000101000;
const uint16_t THREE = 0b0110100010;
const uint16_t ZERO = 0b0111100000;
const uint16_t RIGHT = 0b0000101100;
const uint16_t FIVE = 0b0011100100;
const uint16_t CLOSING_CURLY = 0b0100100100;
const uint16_t ESCAPE = 0b0011101100;
const uint16_t SPECIAL_ACTIVATOR = 0b0000100000;
const uint16_t a = 0b0000000110;
const uint16_t FOUR = 0b0011101000;
const uint16_t i = 0b0010001000;
const uint16_t b = 0b0011000010;
const uint16_t e = 0b0001001000;
const uint16_t o = 0b0001000100;
const uint16_t g = 0b0011001000;
const uint16_t f = 0b0001001100;
const uint16_t c = 0b0110000000;
const uint16_t h = 0b0101000000;
const uint16_t k = 0b0110001000;
const uint16_t j = 0b0001000110;
const uint16_t m = 0b0010000010;
const uint16_t l = 0b0001000010;
const uint16_t d = 0b0011000000;
const uint16_t n = 0b0010000100;
const uint16_t q = 0b0010000110;
const uint16_t p = 0b0010001100;
const uint16_t s = 0b0100000010;
const uint16_t r = 0b0000001010;
const uint16_t u = 0b0100001000;
const uint16_t t = 0b0000001100;
const uint16_t w = 0b0100000100;
const uint16_t v = 0b0100001100;
const uint16_t y = 0b0011000100;
const uint16_t x = 0b0110000100;
const uint16_t ALT = 0b0000010000;
const uint16_t z = 0b0101001000;
const uint16_t GRTR_THAN = 0b1000101000;
//const uint16_t UNDERSCORE = 0b0011101000; //These are the same as numbers
//const uint16_t EQUALS = 0b0011100100;

const uint16_t LEFT_CLICK = 0b0000001000;
const uint16_t RIGHT_CLICK = 0b0000000100;
// END CHARACTER COMBO DEFINITIONS ******************************


void setup() {
  Serial.begin(115200);
  pinMode(MOUSE_SS, OUTPUT);
  digitalWrite(MOUSE_SS, HIGH);
  pinMode(RADIO_CSN, OUTPUT);
  digitalWrite(RADIO_CSN, HIGH);

  
  Mouse.begin();
  Keyboard.begin();
  SPI.begin();  
  radio.begin();
  
  delay(100);
  performStartup();
  dispRegisters();

//   args = [pipe#, pipe_address]
  radio.openReadingPipe(1, pipe);
  radio.startListening();
}

// Button values will be stored here
uint8_t button_vals[NUM_FINGERS];
// Used to store value of the current press in each loop
uint16_t current_key = 0;
// Used for the logic of what the last key press was
uint16_t last_key = 0;
unsigned remove_index = 2;

unsigned long testing_time_start = 0, testing_time_end = 0;

void loop() {
  check_and_move_mouse();
  check_radio();

  current_key = 0;
  //Read the values of the fingers and put them in current_key
  for(int index = 0; index < 5; ++index){
    current_key = current_key | (digitalRead(FINGER_PINS[index]) << index);
  }
  current_key = current_key | (left_hand_fingers << 5);
//  if(current_key != last_key){
//    Serial.println(current_key, BIN);
//  }
  

  remove_index = 2;
  //No key is being pushed
  if(current_key == 0){
    if(last_key != 0){
      Keyboard.releaseAll();
      Mouse.release(MOUSE_LEFT);
      Mouse.release(MOUSE_RIGHT);
    }
    last_key = 0b00000000;
    time = micros();
    return;
  }

    //Asynchronous characters
  if((FUNCTION & current_key) == FUNCTION){
    // TODO: Figure out function key
    // Potential fix: Double click shift and delete?
  }
  else{
    if((CTRL & current_key) == CTRL){
     Keyboard.press(KEY_LEFT_CTRL);
    }
    if((SHIFT & current_key) == SHIFT){
      Keyboard.press(KEY_LEFT_SHIFT);
    }
    if((ALT & current_key) == ALT){
      Keyboard.press(KEY_LEFT_ALT);
    }
  }


  press_key();


  last_key = current_key;
}

void press_key(){
  //Has been holding key down
  if(last_key == current_key){
    //Wait after hold
    if(micros() < time){
      return;
    }
    if(first_time){
      time = micros() + PAUSE_AFTER_PRESS;
      first_time = false;
    } else{
      time = micros() + PAUSE_BETWEEN_HOLD;
    }
  }
  else{
    // Don't send character immediately after push as there could
    // be a possible combo coming up, so wait
    time = micros() + INITIAL_HOLDOFF;
    first_time = true;
    return;
  }
  send_key();
}

//This function actually figures out what key to push
void send_key(){
  unsigned index = remove_index;
  //Remove the ctrl, shift, and alt (or function) characters
  uint16_t current_key_removed = current_key & 0b0111101110;
  if(current_key_removed == a){
    Keyboard.press('a');
    Keyboard.release('a');
  }
  else if(current_key_removed == b){
    Keyboard.press('b');
    Keyboard.release('b');
  }
  else if(current_key_removed == c){
    Keyboard.press('c');
    Keyboard.release('c');
  }
  else if(current_key_removed == d){
    Keyboard.press('d');
    Keyboard.release('d');
  }
  else if(current_key_removed == e){
    Keyboard.press('e');
    Keyboard.release('e');
  }
  else if(current_key_removed == f){
    Keyboard.press('f');
    Keyboard.release('f');
  }
  else if(current_key_removed == g){
    Keyboard.press('g');
    Keyboard.release('g');
  }
  else if(current_key_removed == h){
    Keyboard.press('h');
    Keyboard.release('h');
  }
  else if(current_key_removed == i){
    Keyboard.press('i');
    Keyboard.release('i');
  }
  else if(current_key_removed == j){
    Keyboard.press('j');
    Keyboard.release('j');
  }
  else if(current_key_removed == k){
    Keyboard.press('k');
    Keyboard.release('k');
  }
  else if(current_key_removed == l){
    Keyboard.press('l');
    Keyboard.release('l');
  }
  else if(current_key_removed == m){
    Keyboard.press('m');
    Keyboard.release('m');
  }
  else if(current_key_removed == n){
    Keyboard.press('n');
    Keyboard.release('n');
  }
  else if(current_key_removed == o){
    Keyboard.press('o');
    Keyboard.release('o');
  }
  else if(current_key_removed == p){
    Keyboard.press('p');
    Keyboard.release('p');
  }
  else if(current_key_removed == q){
    Keyboard.press('q');
    Keyboard.release('q');
  }
  else if(current_key_removed == r){
    Keyboard.press('r');
    Keyboard.release('r');
  }
  else if(current_key_removed == s){
    Keyboard.press('s');
    Keyboard.release('s');
  }
  else if(current_key_removed == t){
    Keyboard.press('t');
    Keyboard.release('t');
  }
  else if(current_key_removed == u){
    Keyboard.press('u');
    Keyboard.release('u');
  }
  else if(current_key_removed == v){
    Keyboard.press('v');
    Keyboard.release('v');
  }
  else if(current_key_removed == w){
    Keyboard.press('w');
    Keyboard.release('w');
  }
  else if(current_key_removed == x){
    Keyboard.press('x');
    Keyboard.release('x');
  }
  else if(current_key_removed == y){
    Keyboard.press('y');
    Keyboard.release('y');
  }
  else if(current_key_removed == z){
    Keyboard.press('z');
    Keyboard.release('z');
  }
  else if(current_key_removed == TAB){
    Keyboard.press(KEY_TAB);
    Keyboard.release(KEY_TAB);
  }
  else if(current_key_removed == SPACE){
    Keyboard.press(' ');
    Keyboard.release(' ');
  }
  else if(current_key_removed == ENTER){
    Keyboard.press(KEY_RETURN);
    Keyboard.release(KEY_RETURN);
  }
  else if(current_key_removed == BACK_SPACE){
    Keyboard.press(KEY_BACKSPACE);
    Keyboard.release(KEY_BACKSPACE);
  }
  else if(current_key_removed == PERIOD){
    Keyboard.press('.');
    Keyboard.release('.');
  }
  else if(current_key_removed == COMMA){
    Keyboard.press(',');
    Keyboard.release(',');
  }
  else if(current_key_removed == DOWN){
    Keyboard.press(KEY_DOWN_ARROW);
    Keyboard.release(KEY_DOWN_ARROW);
  }
  else if(current_key_removed == LEFT){
    Keyboard.press(KEY_LEFT_ARROW);
    Keyboard.release(KEY_LEFT_ARROW);
  }
  else if(current_key_removed == UP){
    Keyboard.press(KEY_UP_ARROW);
    Keyboard.release(KEY_UP_ARROW);
  }
  else if(current_key_removed == RIGHT){
    Keyboard.press(KEY_RIGHT_ARROW);
    Keyboard.release(KEY_RIGHT_ARROW);
  }
  else if(current_key_removed == SEMICOLON){
    Keyboard.press(';');
    Keyboard.release(';');
  }
  else if(current_key_removed == OPENING_CURLY){
    Keyboard.press('[');
    Keyboard.release('[');
  }
  else if(current_key_removed == CLOSING_CURLY){
    Keyboard.press(']');
    Keyboard.release(']');
  }
  else if(current_key_removed == QUOTE){
    Keyboard.press(39);
    Keyboard.release(39);
  }
  else if(current_key_removed == SLASH){
    Keyboard.press(92);
    Keyboard.release(92);
  }
  else if(current_key_removed == ZERO){
    Keyboard.press('0');
    Keyboard.release('0');
  }
  else if(current_key_removed == ONE){
    Keyboard.press('1');
    Keyboard.release('1');
  }
  else if(current_key_removed == TWO){
    Keyboard.press('2');
    Keyboard.release('2');
  }
  else if(current_key_removed == THREE){
    Keyboard.press('3');
    Keyboard.release('3');
  }
  else if(current_key_removed == FOUR){
    Keyboard.press('4');
    Keyboard.release('4');
  }
  else if(current_key_removed == FIVE){
    Keyboard.press('5');
    Keyboard.release('5');
  }
  else if(current_key_removed == SIX){
    Keyboard.press('6');
    Keyboard.release('6');
  }
  else if(current_key_removed == SEVEN){
    Keyboard.press('7');
    Keyboard.release('7');
  }
  else if(current_key_removed == EIGHT){
    Keyboard.press('8');
    Keyboard.release('8');
  }
  else if(current_key_removed == NINE){
    Keyboard.press('9');
    Keyboard.release('9');
  }
  else if(current_key_removed == ESCAPE){
    Keyboard.press(177);
    Keyboard.release(177);
  }
  else if(current_key_removed == LEFT_CLICK){
    Mouse.press(MOUSE_LEFT);
  }
  else if(current_key_removed == RIGHT_CLICK){
    Mouse.press(MOUSE_RIGHT);
  }
//  else if(current_key_removed == UNDERSCORE){
//    Keyboard.press('-');
//    Keyboard.release('-');
//  }
//  else if(current_key_removed == EQUALS){
//    Keyboard.press('=');
//    Keyboard.release('=');
//  }
}
