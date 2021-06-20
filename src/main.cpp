#include <Arduino.h>
#include <RCSwitch.h>
#include <SoftwareSerial.h>

//SoftwareSerial mySerial(D8, D7); // RX, TX

RCSwitch mySwitch = RCSwitch();

long count = 0;

void setup() 
{
  Serial.begin(74880);
  //mySerial.begin(2400);
  //pinMode(D8, INPUT);
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

  // if(mySerial.available()){
  //   int c = mySerial.read();
  //   Serial.print(c);
  //   Serial.print(" ");
  // }

  if (mySwitch.available()) {
    tone(D5, 800, 50);
    tone(D6, 800, 50);
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