#include <Arduino.h>
#include <RCSwitch.h>


RCSwitch mySwitch = RCSwitch();

long count = 0;

void setup() 
{
  Serial.begin(74880);
  //pinMode(D3, INPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  mySwitch.enableReceive(D3);  // Receiver on interrupt 0 => that is pin #2
}

void loop() 
{
  count ++;

  // if(count % 100000 == 0){
  //   Serial.println(count);
  // }


  if (mySwitch.available()) {
    tone(D5, 800, 50); // beep sound
    tone(D6, 800, 50); // flash led
    Serial.print("Value: ");
    Serial.print(mySwitch.getReceivedValue());
    Serial.print(" bit length: ");
    Serial.print(mySwitch.getReceivedBitlength());
    Serial.print(" Delay: ");
    Serial.print(mySwitch.getReceivedDelay());
    // Serial.print("RawData: ");
    // Serial.println(mySwitch.getReceivedRawdata());
    Serial.print(" Protocol: ");
    Serial.println(mySwitch.getReceivedProtocol());
    
    mySwitch.resetAvailable();
  }
}