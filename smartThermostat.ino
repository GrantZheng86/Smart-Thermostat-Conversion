// Some packages and header files here
// Make sure that you have installed the "ESPalexa" library before compiling the code
// Library link https://github.com/Aircoookie/Espalexa


#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <Espalexa.h>
#include <Math.h>


// Wifi cridentials,
// **** CHANGE THEM TO YOUR 2.4GHZ BAND CRIDENTIALS *****
#define WIFI_SSID "*********"
#define WIFI_PASS "*********"


// Channel and pin configuration
const byte temperatureChannel = 0;
const byte fanChannel = 1;
const byte heatACChannel = 2;
const byte temperaturePin = 13;
const byte fanPin = 12;
const byte heatACPin = 14;

// PWM setup
const int freq = 50; // 50 Hz for servo control
const int resolution = 16; // 16 bit resolution
const int tempLow = 50; // Lower limit on my thermostat setting 
const int tempHigh = 90; // Upper limit on my thermostat settings


// Declear global variables
boolean wifiConnection;
int prevTempTic;
Espalexa espalexa;




/*
 *  Here are some servo position data that I have tried out, you may have a slihtly different numbers for your setup
 *  The PWM servo range is from 2100 to 8100, this can be different from servo to servos
 *  
 */
static const int LOWER_TIC = 4000;  // The lower side of PWM tics, cooresponding to the highest temperature
static const int UPPER_TIC = 7000;  // The upper side of PWM tics, cooresponding to the lowest temperature

void setup() {

  // Serial communication, used for debugging
  Serial.begin(115200);
  wifiConnection = connectWiFi();

  // The first argument will be the name appear in the Alexa App when you are searching for them
  if (wifiConnection){
    espalexa.addDevice("Servo 1", adjustTemperature);
    espalexa.addDevice("on off servo",switchFan );
    espalexa.addDevice("Heat servo", heatStatus);
    espalexa.begin();
  }else{
    Serial.println("WiFi connection failure, check connection and try agian");
    delay(5000);
  }

  // Setup the PWMs
  ledcSetup(temperatureChannel, freq, resolution);
  ledcSetup(fanChannel, freq, resolution);
  ledcSetup(heatACChannel, freq, resolution);
  ledcAttachPin(temperaturePin, temperatureChannel);
  ledcAttachPin(fanPin, fanChannel);
  ledcAttachPin(heatACPin, heatACChannel);

  // initialize the temperature tic, which is in the middle of temperature settings
  prevTempTic = 128;
}


// This loop executes alexa command
void loop() {
  espalexa.loop();
  delay(1);
}


/*
 * This function converts degree to the required PWM tic needed by the servo, this 
 * function is only used by the temperature control servo
 */
int angleToPWM(double degree){

  double fraction;
  int toReturn;
  if (degree < 0.0){
    degree = 0;
  } else if (degree > 180.0){
    degree = 180.0;
  }

  fraction = degree / (double)180;
  toReturn = (double)UPPER_TIC - fraction * (UPPER_TIC - LOWER_TIC);
  return round(toReturn);  
}

/*
 * This function handles WiFi connection
 */
boolean connectWiFi(){
  boolean wifiState = true;
  int counter = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  Serial.println("");
  Serial.println("Connecting to WiFi ");

  while (WiFi.status() != WL_CONNECTED){
    delay(200);
    Serial.print(".");
    if (counter > 100){
      wifiState = false;
      break;
    }

    counter ++;
  }


  if (wifiState){
    Serial.println("");
    Serial.printf("Connected to %s \n", WIFI_SSID);
    Serial.println("The IP address is ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Connection Failed");
  }

  return wifiState;
}

/*
 * The adjust temperature function, changes the currTemperature to the user designated one.
 * I did a conversion from the percentage to the actual temperature, so foe example, turning the
 * temperature to 70F can be done just saying "change the [temperature] to 70" instead of "setting the
 * temperature to 50%"
 */
void adjustTemperature(uint8_t rawAlexaTic){
  Serial.printf("Raw alexa Tic is %d\n", rawAlexaTic );
  float convertedAlexaTic = float(rawAlexaTic)*2.52 + -325.6930693;
  int alexaTic= round(convertedAlexaTic);
  
  Serial.printf("Converted alexa Tic is %f\n", convertedAlexaTic );

  if (alexaTic < 0){
    alexaTic = 0;
  } else if (alexaTic > 255){
    alexaTic = 255;
  }

  int tempDifference = prevTempTic - alexaTic;
  prevTempTic = alexaTic;
  double angleDegree = alexaToAngle(alexaTic);
  int pwmTics = angleToPWM(angleDegree);

  // There is a bit play in the servo gears, so by turning the
  // servo over a bit and change it back can reduce the inconsistency a bit
  if (tempDifference < 0){
    ledcWrite(temperatureChannel, pwmTics - 300);
  } else {
    ledcWrite(temperatureChannel, pwmTics + 300);
  }
  delay(500);
  ledcWrite(temperatureChannel, pwmTics);
  Serial.printf("The current pwm tic is %d\n", pwmTics);
  delay(1000);
  ledcWrite(temperatureChannel, 0); // Turning the servo off
}

/**
 * This one converts alexa tic to the angles
 */
double alexaToAngle(uint8_t alexaTic){
  double fraction = (double) alexaTic / 255.0f;
  return 180.0f*fraction;
}
/**
 * This one converts alexa tic to temperature to control the tempearture lever
 */
int alexaToTemperature(uint8_t alexaTic){
  double fraction = (double) alexaTic / 2550.f;
  double toReturn = tempLow + fraction * (tempHigh - tempLow);
  return round(toReturn);
}

/**
 * This function controls the fan status
 */
void switchFan(uint8_t fanStatus){
  if (fanStatus >= 129){ // When the tic is more than 50%, fan on
    ledcWrite(fanChannel, UPPER_TIC - 1300 );
  } else {              // When the tic is less than 50%, fan switch to auto
    ledcWrite(fanChannel, LOWER_TIC);
  }
  delay(750);
  ledcWrite(fanChannel, 0);
}


void heatStatus(uint8_t heatStatus){
  if (heatStatus >= 255.0 * 2.0/3.0){   // When the tic is more than 2/3, change to heat
    ledcWrite(heatACChannel, LOWER_TIC - 700 );
  } else if (heatStatus < 255.0 * 2.0/3.0 && heatStatus >= 255.0 /3.0){ // When the tic is in between 1/3 and 2/3, change to OFF
    ledcWrite(heatACChannel, (LOWER_TIC + UPPER_TIC) / 2 - 300 );
  } else {  // Otherwise change to cool
    ledcWrite(heatACChannel, UPPER_TIC );
  }

  delay (750);
  ledcWrite(heatACChannel, 0);
}
