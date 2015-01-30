#ifndef SIM900_h
#define SIM900_h

#include "Arduino.h"
#include <SoftwareSerial.h>

#define BUFFER_LEN 200

class SIM900{

public:
	SIM900(SoftwareSerial *ser, uint8_t pwrPin);
	
	void init(char* pinNumber);
	
	void sendAtCommand(char* command);
	uint8_t sendAtCommandAndCheckOK(char* command, char* response, unsigned int timeout);
	uint8_t checkOK(char* response, unsigned int timeout);
	uint8_t sendSMS(char* phoneNumber,char* msg);
	char* readSMS(uint8_t smsIdx, char* num_tel, unsigned int timeout);
	uint8_t deleteSMS(uint8_t smsIdx);
	boolean isRegistered();
	void debugLastError();

protected:
	uint8_t waitPrompt(unsigned int timeout);
	uint8_t readLine(char* response, unsigned int timeout, boolean append);

private:
	SoftwareSerial *simSwSerial;
	uint8_t powerPin;

	char* buffer;
	uint8_t answer;
};

#endif
