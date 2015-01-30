#include "Arduino.h"
#include "SIM900.h"

SIM900::SIM900(SoftwareSerial *ser, uint8_t pwrPin){
	simSwSerial = ser;
	powerPin = pwrPin;
	buffer = new char[BUFFER_LEN];
}

void SIM900::init(char* pinNumber){
	pinMode(powerPin, OUTPUT);
		
	simSwSerial->begin(19200);
		
	uint8_t answer=0;
	boolean simPinNeeded = false;
		
	//auto-bauding : send "AT"
	delay(500);
	answer = sendAtCommandAndCheckOK("AT",buffer,1000);
	if(answer==2 && strstr(buffer,"RDY")!=NULL) answer = 0;
	if(answer == 2){
		//timeout
		//On démarre le SIM900
		Serial.println("power on pulse");
		digitalWrite(powerPin,HIGH);
		delay(500);
		digitalWrite(powerPin,LOW);
		delay(2500);
	}
		
			
	// waits for an answer from the module
	//Si on est en timeout, AT n'a pas répondu OK. Ca peut etre une RDY
	unsigned long baudrates [] = {115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200};
	uint8_t idx = 0;
	uint8_t retry = 0;
	while(answer>0){
		// Send AT every two seconds and wait for the answer
		answer = sendAtCommandAndCheckOK("AT",buffer,1000);
		if(answer==2 && strstr(buffer,"RDY")!=NULL) answer = 0;
		if(retry++ > 2  && idx<=8){
			Serial.print("Set baudrate=");Serial.println(baudrates[idx]);
			simSwSerial->begin(baudrates[idx++]);
			retry = 0;
		}
	}
		
	/*
	 * ATE0 : command echo off
	 */
	sendAtCommandAndCheckOK("ATE0",buffer,500);
	
	/*
	 * set auto baud
	 */
	//sendAtCommandAndCheckOK("AT+IPR?",buffer,500);
	sendAtCommandAndCheckOK("AT+IPR=0",buffer,500);
	
	/*
	 * Check phone functionnality
	 */
	answer = sendAtCommandAndCheckOK("AT+CFUN?",buffer,500);
	if(answer == 0 && strstr(buffer,"+CFUN: 1")==NULL){
		sendAtCommandAndCheckOK("AT+CFUN=1,1",buffer,10000);
	}
	
	/*
	 * Check PIN input need
	 */
	if(pinNumber != NULL){
		answer = sendAtCommandAndCheckOK("AT+CPIN?",buffer,500);
		if(answer == 0 && strstr(buffer,"+CPIN: SIM PIN")!=NULL){
			sprintf(buffer,"AT+CPIN=%s", pinNumber);
			answer = sendAtCommandAndCheckOK(buffer,buffer,  500);
			if(answer>0) return;
		}
	}

	/*
	 * Mode texte
	 */	
	sendAtCommandAndCheckOK("AT+CMGF=1",buffer,500);    // sets the SMS mode to text
}

/*
 * Envoi une command AT au SIM900, et vérifie que la réponse contient la chaine attendue ()
 * Attend jusqu'au timeout.
 * Réponse : 
 *    0:OK - response match
 *    1:Empty
 *    2:Timeout
 *    3:Overflow
 *    4:Response does not match
 */
uint8_t SIM900::sendAtCommandAndCheckOK(char* command, char* response, unsigned int timeout){
	 sendAtCommand(command);
	 return checkOK(response,timeout);
}

/*
 * Envoi une command AT au SIM900
 */
void SIM900::sendAtCommand(char* command){
	delay(200);
	boolean flag = false;
	while(simSwSerial->available()){
		char c = simSwSerial->read();
		if(!flag){
			Serial.print("  SKIPPED : ");
			flag = true;
		}
		Serial.print(c);
	}
	//if(flag) Serial.println("");
	
	Serial.print("sendAtCommand : "); Serial.println(command);
	simSwSerial->println(command);
}

/*
 * Attend et vérifie que la réponse contient la chaine attendue
 * Attend jusqu'au timeout.
 * Réponse : 
 *    0:OK - response match
 *    1:Empty
 *    2:Timeout
 *    3:Overflow
 *    4:ERROR
 */
uint8_t SIM900::checkOK(char* response, unsigned int timeout){
	//La réponse commence éventuellement par un "echo" de la requete, puis une ligne vide
	//On ignore cette première partie
	uint8_t answer = readLine(response,timeout,false);
	if(answer==2){
		//Serial.println("TIMEOUT sur le première ligne");
		return answer; 
	}

	do{
		answer = readLine(response, timeout, false);
	}
	while(answer==1); //Tant que c'est une ligne vide, on passe
	
	char* sub;
	while(answer==0 || answer==1){
		//On regarde si la réponse se termine par OK
		//sub=strstr(response,"\r\nOK");
		unsigned int idx = strlen(response)-6;
		if(idx>=0 && strcmp(&response[idx],"\r\nOK\r\n") == 0){
			//Reponse OK
			//On tronque le buffer pour supprimer "OK"
			response[idx] = '\0';
			break;
		}
		//On regarde si la réponse commence par OK
		if(strcmp("OK",response)<0){
			response[0] = '\0';
			break;
		}
		
		//On regarde si la réponse commence ou termine par "ERROR"
		idx = strlen(response)-9;
		if(strcmp("ERROR",response)<0 || idx>=0 && strcmp(&response[idx],"\r\nERROR\r\n") == 0){
			//Reponse ERROR
			answer = 4;
			break;
		}
		//On lit les lignes suivantes tant qu'on n'a pas le réponse escomptée
		answer = readLine(response, timeout, true);
	}
	
	
	switch(answer){
		case 0: Serial.print("  OK      "); break;
		case 1: Serial.print("  OK-EMPTY"); break;
		case 2: Serial.print("  TIMEOUT "); break;
		case 3: Serial.print("  OVERFLOW"); break;
		case 4: Serial.print("  ERROR:");break;
	}
	Serial.print(" (nb chars="); Serial.print(strlen(response)); Serial.println(") : "); 
	Serial.println(response);
	
	if(answer==4) debugLastError();
	
	return answer;
}

void SIM900::debugLastError(){
	sendAtCommandAndCheckOK("AT+CEER=0",buffer,500); //message d'erreur textuel
	sendAtCommandAndCheckOK("AT+CEER",buffer,500); 
	Serial.print(buffer);
}

/*
 * Lit une ligne en provenance du SIM900
 * Attend jusqu'au timeout.
 * Réponse : 
 *    0:OK
 *    1:Empty
 *    2:Timeout
 *    3:Overflow
 *
 */
uint8_t SIM900::readLine(char* response, unsigned int timeout, boolean append){
	uint8_t idx=0;
	if(append){
		idx = strlen(response);
	}
	else{
		memset(response, '\0', BUFFER_LEN);    // Initialice the string
	}
	uint8_t answer=2;  //Timeout par défaut
	
	unsigned long previous = millis();
	
	while((millis()-previous)<timeout){
		if(simSwSerial->available() != 0){
			byte c = simSwSerial->read();
			
			//On vérifie que ça rentre dans le buffer
			if(idx+1>=BUFFER_LEN){
			  Serial.println("ERROR: Buffer overflow");
			  answer = 3;
			  break;
			}
			response[idx] = c;
			idx = idx+1;
			
			if(c==10){
			  //C'est la fin de ligne
			  //if(idx>0 && response[idx-1]==13)  response[idx--] = '\0';
			  
			  answer = 0;
			  if(idx==2) answer = 1; //uniquement les caractères CR(13) LF(10)
			  break;
			}
		}
	}
	
	return answer;
}

/*
 * Attend jusqu'au timeout l'invite ">" de la part de la SIM
 * Réponse : 
 *    0:OK
 *    2:Timeout
 */
uint8_t SIM900::waitPrompt(unsigned int timeout){
	Serial.print("SIM900::WaitPrompt : ");
	uint8_t idx=0;
	uint8_t answer=2;  //Timeout par défaut
	
	unsigned long previous = millis();
	
	while((millis()-previous)<timeout){
		if(simSwSerial->available() != 0){
			byte c = simSwSerial->read();
			if(c==10 || c==13){
			  //on ignore les caractères CR LF
			}
			else if(c=='>'){
			  //on a le prompt
			  answer = 0;
			  break; 
			}
		}
	}
	 
	switch(answer){
		case 0: Serial.print("OK"); break;
		case 2: Serial.print("TIMEOUT"); break;
	}
	Serial.println();
	
	return answer;
}

/*
 * Envoi d'un SMS
 */
uint8_t SIM900::sendSMS(char* phoneNumber,char* msg){
	Serial.println("SIM900::sendSMS");
	//On initie l'envoi de SMS avec la commande AT et le n° de tel destinataire
	sprintf(buffer,"AT+CMGS=\"%s\"", phoneNumber);
	sendAtCommand(buffer);
	answer = waitPrompt(2000);
	if(answer == 0){
		//Envoi du message
		Serial.print(">"); Serial.println(msg);
		simSwSerial->println(msg);
		simSwSerial->write(0x1A);
				
		//On attend la réponse
		answer = checkOK(buffer,10000);
	}
	
	return answer;
}

/*
 * Lecture d'un SMS
 * Réponse: le message du SMS, ou NULL si erreur
 */
char* SIM900::readSMS(uint8_t smsIdx, char* num_tel,unsigned int timeout){
	Serial.println("SIM900::readSMS");
	answer = sendAtCommandAndCheckOK("AT+CMGR=1",buffer,timeout);    //Récupère le SMS n°1
	if( answer==0 && strcmp("+CMGR:",buffer)<0 ){
		//Il y a un SMS
		
		char* tmp = strchr(&buffer[2],10);
		char* msg = &tmp[1];
		tmp[0] = '\0';
		
		strtok(buffer,"\"");               //1° token : +CMGR:
		char* stat = strtok(NULL,"\"");    //2° token : REC UNREAD ou REC READ
		strtok(NULL,"\"");                 //3° token : ","
		char* tel = strtok(NULL,"\"");    //4° token : n° de tel
		//Serial.print("stat:"); Serial.println(stat);
		//Serial.print("tel :"); Serial.println(tel);
		
		if(num_tel != NULL) strcpy(num_tel,tel);
		return msg;
	}
	else{
		Serial.print("response:"); Serial.println(answer);
		return NULL;
	}
}

/*
 * Supression d'un SMS
 */
uint8_t SIM900::deleteSMS(uint8_t smsIdx){
	return sendAtCommandAndCheckOK("AT+CMGD=1",buffer,500);
}

/*
 * Est-ce qu'on est enregistré auprès du réseau?
 */
boolean SIM900::isRegistered(){
	answer = sendAtCommandAndCheckOK("AT+CREG?",buffer,  500);
	if(answer==0 && ( strstr(buffer,"+CREG: 0,1") != NULL || strstr(buffer,"+CREG: 0,5") != NULL ) ) return true;
	else return false;
}
