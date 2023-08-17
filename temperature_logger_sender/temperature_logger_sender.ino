// Connection matrix:
//
// Arduino	| RTC		| RF24		| SD CARD	| TS1		| TS2		| Other
//--------------------------------------------------------------------------------------------------------
// D0		|			|			|			|			|			|
// D1		|			|			|			|			|			|
// D2		|			| CE		|			|			|			|
// D3		|			| CSN		|			|			|			|
// D4		|			|			| CS		| 			|			|
// D5		|			|			|			|V+/2.2k	|			|
// D6		|			|			|			|			| V+/2.2k	| 
// D7		|			|			|			|			|			| OUT: Debug flag
// D8		|			|			|			|			|			| IN:  Logging enabled (active low)
// D9		|			|			|			|			|			| 
// D10		|			|			|			|			|			|
// D11		|			| MOSI		| MOSI		|			|			|
// D12		|			| MISO		| MISO		|			|			|
// D13		|			| SCK		| SCK		|			|			|
// A0		|			|			|			| Value		|			|
// A1		|			|			|			|			| Value		|
// A2		|			|			|			|			|			|
// A3		|			|			|			|			|			|
// A4		| SDA		|			|			|			|			|
// A5		| SCL		|			|			|			|			|
// A6		|			|			|			|			|			|
// A7		|			|			|			|			|			|


#define WITH_DEBUG_PULSES 0
#define WITH_OLED 0
#define WITH_60_SECOND_UPDATE 1
#define RTC_ADDRESS 0x68

#include <avr/wdt.h>
#include <avr/sleep.h>

#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <SD.h>

RF24 radio(2, 3); // CE, CSN
const byte radioId[4] = "ZFT";
byte radioPayload[8] = {0};

const byte temperatureSteps[] = {
    0x01, 0x01, 0x00, 0x10, 0x00, 0x00, 0x01, 0x00,
    0x10, 0x00, 0x10, 0x00, 0x01, 0x01, 0x10, 0x10,
    0x10, 0x10, 0x01, 0x01, 0x11, 0x11, 0x01, 0x11,
    0x11, 0x11, 0x21, 0x11, 0x12, 0x21, 0x21, 0x12,
    0x22, 0x22, 0x22, 0x31, 0x23, 0x23, 0x23, 0x33,
    0x42, 0x33, 0x34, 0x34, 0x43, 0x53, 0x44, 0x54,
    0x44, 0x45, 0x64, 0x55, 0x65, 0x55, 0x56, 0x65,
    0x66, 0x76, 0x66, 0x66, 0x76, 0x67, 0x77, 0x76,
    0x77, 0x86, 0x77, 0x77, 0x77, 0x87, 0x77, 0x77,
    0x87, 0x77, 0x77, 0x78, 0x77, 0x77, 0x76, 0x77,
    0x77, 0x76, 0x76, 0x67, 0x67, 0x67, 0x65, 0x66,
    0x66, 0x66, 0x56, 0x65, 0x55, 0x55, 0x56, 0x55,
    0x54, 0x54, 0x54, 0x45, 0x45, 0x43, 0x44, 0x44,
    0x44, 0x34, 0x33, 0x34, 0x43, 0x33, 0x43, 0x33 };

char logData[] = "2000-00-00 00:00:00;  0;   ";

ISR(WDT_vect)
{
	wdt_reset();
}

#if WITH_DEBUG_PULSES

void debugPulse()
{
	PORTD |= (1 << 7);
	_delay_us(100);
	PORTD &= ~(1 << 7);
}

#else

#define debugPulse void

#endif

void packedBcdToAscii(byte b, char * dst)
{
	dst[0] = '0' + ((b >> 4) & 0x0f);
	dst[1] = '0' + ((b >> 0) & 0x0f);
}

int getTemperature(int a)
{
    int value = 1024;
    int temperature = -1;

    do
    {
        temperature++;

        if ((temperature & 1) == 1)
            value -= (temperatureSteps[temperature >> 1] >> 4) & 0x0f;
        else
            value -= temperatureSteps[temperature >> 1] & 0x0f;

    } while (a < value && temperature < 2 * sizeof(temperatureSteps) - 1);

    return temperature - 40;
}
        
void setTime()
{
	Wire.beginTransmission(RTC_ADDRESS);
	Wire.write(0x00);
	Wire.write(0x00); // Seconds
	Wire.write(0x22); // Minutes
	Wire.write(0x13); // Hours
	Wire.endTransmission();

	Wire.beginTransmission(RTC_ADDRESS);
	Wire.write(0x04);
	Wire.write(0x14); // Day of month
	Wire.write(0x05); // Month
	Wire.write(0x18); // Year
	Wire.endTransmission();
}

void packPayload()
{
	Wire.beginTransmission(RTC_ADDRESS);
	Wire.write(0x00);
	Wire.endTransmission();
  
	Wire.requestFrom(RTC_ADDRESS, 7);

	byte b;

	radioPayload[5] = Wire.read() & 0x7f;
	radioPayload[4] = Wire.read();
	radioPayload[3] = Wire.read();
	Wire.read();
	radioPayload[2] = Wire.read();
	radioPayload[1] = Wire.read();
	radioPayload[0] = Wire.read();

	debugPulse();

	PORTD |= (1 << 5);
	_delay_us(100);
	//int t1 = map(analogRead(0), 0, 1023, -40, 120);
    int t1 = getTemperature(analogRead(0));
	PORTD &= ~(1 <<5);
	
	PORTD |= (1 << 6);
	_delay_us(100);
	//int t2 = map(analogRead(1), 0, 1023, -40, 120);
    int t2 = getTemperature(analogRead(1));
	PORTD &= ~(1 << 6);

	radioPayload[6] = (t1 + 40) & 0xff;
	radioPayload[7] = (t2 + 40) & 0xff;

	if(!(PINB & (1 << 0)))
		radioPayload[0] |= 0x80;
}

void savePayload()
{
	if(!(radioPayload[0] & 0x80))
		return;
		
	packedBcdToAscii(radioPayload[0] & 0x1f, logData + 2);
	packedBcdToAscii(radioPayload[1], logData + 5);
	packedBcdToAscii(radioPayload[2], logData + 8);
	packedBcdToAscii(radioPayload[3], logData + 11);
	packedBcdToAscii(radioPayload[4], logData + 14);
	packedBcdToAscii(radioPayload[5], logData + 17);

	String str = String(radioPayload[6] - 40);
	
	while(str.length()<3)
		str = String(" " + str);

	strcpy(logData + 20, str.c_str());

	str = String(radioPayload[7] - 40);
	
	while(str.length()<3)
		str = String(" " + str);

	strcpy(logData + 24, str.c_str());

	logData[23] = ';';
	
	File fOut = SD.open("ARDUINO.TXT", FILE_WRITE);

	if(fOut)
	{
		fOut.println(logData);
		fOut.close();
	}
}

void setup()
{
	PORTB = 0;
	PORTC = 0;
	PORTD = 0;
	
	DDRB = 0;
	DDRC = 0;
	DDRD = (1 << 5) | (1 << 6);

	PORTB = (1 << 0);
	
#if WITH_DEBUG_PULSES
	DDRD |= (1 << 7);
#endif

	Wire.begin();
	//setTime();
	
	radio.begin();
	radio.setChannel(37);
	radio.setAddressWidth(3);
	radio.setPayloadSize(8);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.openWritingPipe(radioId);
	radio.stopListening();
	radio.powerDown();

	SD.begin(4);

	File fOut = SD.open("ARDUINO.TXT", FILE_WRITE);

	if(fOut)
		fOut.close();
	
    MCUSR &= ~(1<<WDRF);
    cli();
    WDTCSR = (1<<WDCE) | (1<<WDE) ;
    WDTCSR = (1<<WDIE) | (1 << WDP2) | (1 << WDP1);
    sei();
}

void loop()
{
	debugPulse();
	
	packPayload();
	debugPulse();
	
	savePayload();
	debugPulse();
	
	radio.powerUp();
	debugPulse();
	
	radio.write(radioPayload, sizeof(radioPayload));
	debugPulse();
	
	radio.powerDown();
	debugPulse();

#if WITH_60_SECOND_UPDATE
	for(int i=0; i<55; i++)
#endif
	{
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		sleep_mode();
	}	
}

