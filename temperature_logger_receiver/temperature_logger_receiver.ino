// Connection matrix:
//
// Arduino	| RF24		| OLED		
//----------------------------------
// D0		|			|			
// D1		|			|			
// D2		| CE		|			
// D3		| CSN		|			
// D4		|			|			
// D5		|			|			
// D6		|			|			
// D7		|			|			
// D8		|			|			
// D9		|			|			
// D10		|			|			
// D11		| MOSI		|			
// D12		| MISO		|			
// D13		| SCK		|			
// A0		|			|			
// A1		|			|			
// A2		|			|			
// A3		|			|			
// A4		|			| SDA		
// A5		|			| SCL		
// A6		|			|			
// A7		|			|			

#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_ADDRESS 0x3C

RF24 radio(2, 3); // CE, CSN
Adafruit_SSD1306 oled(-1); // no reset pin

const byte radioId[4] = "ZFT";
byte radioPayload[8] = {0};

bool loggingEnabled = false;
char timestamp[20] = "2000-00-00 00:00:00";
char temperatures[8] = "  0   0";
unsigned long lastMessage = 0xfff00000;
byte animationIndex = 0;

void packedBcdToAscii(byte b, char * dst)
{
	dst[0] = '0' + ((b >> 4) & 0x0f);
	dst[1] = '0' + ((b >> 0) & 0x0f);
}

void unpackPayload()
{
	loggingEnabled = (radioPayload[0] & 0x80) == 0x80;
	
	packedBcdToAscii(radioPayload[0] & 0x7f, timestamp + 2);
	packedBcdToAscii(radioPayload[1], timestamp + 5);
	packedBcdToAscii(radioPayload[2], timestamp + 8);
	packedBcdToAscii(radioPayload[3], timestamp + 11);
	packedBcdToAscii(radioPayload[4], timestamp + 14);
	packedBcdToAscii(radioPayload[5], timestamp + 17);

	int t1 = radioPayload[6] - 40;
	int t2 = radioPayload[7] - 40;

	String str = String(t1);
	
	while(str.length()<3)
		str = String(" " + str);

	strcpy(temperatures, str.c_str());

	str = String(t2);
	
	while(str.length()<3)
		str = String(" " + str);

	strcpy(temperatures + 4, str.c_str());

	temperatures[3] = ' ';
}

void setup()
{
	Wire.begin();

	oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
	oled.clearDisplay();
	oled.display();
	
	radio.begin();
	radio.setChannel(37);
	radio.setAddressWidth(3);
	radio.setPayloadSize(8);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.openReadingPipe(0, radioId);
	radio.startListening();
}

void loop()
{
	if (radio.available())
  	{
  		lastMessage = millis();
  		animationIndex = 255;
  		
		radio.read(radioPayload, sizeof(radioPayload));
	
		unpackPayload();
		
		oled.clearDisplay();
		
		if(loggingEnabled)
		{
			oled.fillRect(0, 0, 4, 8, WHITE);
			oled.fillRect(124, 0, 4, 8, WHITE);
		}
		
		oled.setTextColor(WHITE);
		oled.setTextSize(1);
		oled.setCursor(8, 0);
		oled.println(timestamp);
		oled.setTextSize(3);
		oled.setCursor(0, 11);
		oled.println(temperatures);
		oled.drawFastHLine(0, 8, 128, WHITE);
		oled.drawFastVLine(64, 8, 24, WHITE);
		oled.display();
  	}
  	else if(millis() - lastMessage > 90000)
  	{
  		byte index = (millis() >> 9) & 0x3;

  		if(index != animationIndex)
  		{
  			animationIndex = index;
  			
	  		oled.clearDisplay();

			oled.fillRect(0, 0, 128, 8, WHITE);
			oled.setTextColor(BLACK);
			oled.setTextSize(1);
			oled.setCursor(34, 0);
			oled.print("Waiting");

			for(int i=0; i<animationIndex; i++)
				oled.print(".");
	  		
			oled.setTextColor(WHITE);
			oled.setTextSize(2);
			oled.setCursor(11, 12);
			oled.println("No signal");
			oled.display();
  		}
  	}
}
