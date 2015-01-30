#include <SIM900.h>
#include <SoftwareSerial.h>

SoftwareSerial ser(10,3); 
SIM900 sim900(&ser,7);

char phone_number[]="0612345678";
char pin_number[]="0000";

void setup(){
  Serial.begin(115200);
  
  sim900.init(pin_number);  
}

void loop(){
  /*
   * Lit le premier SMS
   */
  char numTel[15];
  char* msg = sim900.readSMS(1,numTel,500);
  if(msg != NULL){
    Serial.print("SMS de: ");Serial.println(numTel);
    Serial.println(msg);
    sim900.deleteSMS(1);
    
    //Envoi d'une réponse
    sim900.sendSMS(numTel,"message recu");
  }
  
  delay(5000);
}
