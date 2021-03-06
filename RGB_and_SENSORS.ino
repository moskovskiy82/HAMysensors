// https://forum.mysensors.org/topic/6765/rgb-led-strip
#define SN "Bedroom"
#define SV "2.1"

#define MY_DEBUG
#define MY_RF24_CE_PIN 8

#define MY_RADIO_NRF24
#define MY_NODE_ID 31

//Disable blinking
#define MY_DEFAULT_ERR_LED A10
#define MY_DEFAULT_TX_LED A10
#define MY_DEFAULT_RX_LED A10

//Libraries include
#include <MySensors.h>
#include <DHT.h> 
#include <math.h>
#include <Wire.h>
#include <SPI.h>

#define CHILD_ID_LIGHT 1
#define CHILD_ID_HUM 2
#define CHILD_ID_TEMP 3
#define CHILD_ID_MQ 4

//Set pins
#define REDPIN 6
#define GREENPIN 5
#define BLUEPIN 9
#define DHT_PIN 7
const int MQ_Pin = A3;
DHT dht;

//Messages
MyMessage lightMsg(CHILD_ID_LIGHT, V_LIGHT);
MyMessage rgbMsg(CHILD_ID_LIGHT, V_RGB);
MyMessage dimmerMsg(CHILD_ID_LIGHT, V_DIMMER);
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgMQ(CHILD_ID_MQ, V_LEVEL);

//Global variables
byte red = 255;
byte green = 255;
byte blue = 255;
byte r0 = 255;
byte g0 = 255;
byte b0 = 255;
char rgbstring[] = "ffffff";

int on_off_status = 0;
int dimmerlevel = 100;
int fadespeed = 0;

long MQ_Millis = 0;
long MQ_interval = 60000;
long DHT_Millis = 0;
long DHT_interval = 60000;

bool initialValueSent = false;

void setup()
{
  // Output pins
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);

  dht.setup(DHT_PIN);  
}

void presentation()
{
  sendSketchInfo(SN, SV);
  present(CHILD_ID_LIGHT, S_RGB_LIGHT);
  present(CHILD_ID_HUM, S_HUM);
  present(CHILD_ID_TEMP, S_TEMP);
  present(CHILD_ID_MQ, S_AIR_QUALITY);  
}

void loop()
{
unsigned long DHT_Current_Millis = millis();
if((unsigned long)(DHT_Current_Millis - DHT_Millis) >= DHT_interval)
  {
    DHT_Millis = DHT_Current_Millis; 
    delay(dht.getMinimumSamplingPeriod());
    float temperature = dht.getTemperature();
    float humidity = dht.getHumidity();
    if (isnan(temperature)) 
      {Serial.println("Failed reading temperature from DHT");}
    if (isnan(humidity)) 
      {Serial.println("Failed reading humidity from DHT");}
    else 
    {
      send(msgTemp.set(temperature, 1));
      send(msgHum.set(humidity, 1));
      Serial.print("T: ");
      Serial.println(temperature);
      Serial.print("H: ");
      Serial.println(humidity);
    }
  }

unsigned long MQ_Current_Millis = millis();
if((unsigned long)(MQ_Current_Millis - MQ_Millis) >= MQ_interval)
  {
    MQ_Millis = MQ_Current_Millis; 
    float mq_reading = analogRead(MQ_Pin);
    if (isnan(mq_reading)) 
      { Serial.println("Failed mq_reading"); } 
    else 
    {
    send(msgMQ.set(mq_reading, 1));
    Serial.print("MQ: ");
    Serial.println(mq_reading);
    } 
  }  

  static bool first_message_sent = false;
  if ( first_message_sent == false ) 
  {
    Serial.println( "Sending initial state..." );
    set_hw_status();
    send_status();
    first_message_sent = true;
  }
}

void receive(const MyMessage &message)
{
  int val;
  
  if (message.type == V_RGB) {
    Serial.println( "V_RGB command: " );
    Serial.println(message.data);
    long number = (long) strtol( message.data, NULL, 16);

    // Save old value
    strcpy(rgbstring, message.data);
    
    // Split it up into r, g, b values
    red = number >> 16;
    green = number >> 8 & 0xFF;
    blue = number & 0xFF;

    send_status();
    set_hw_status();

  } else if (message.type == V_LIGHT || message.type == V_STATUS) {
    Serial.println( "V_LIGHT command: " );
    Serial.println(message.data);
    val = atoi(message.data);
    if (val == 0 or val == 1) {
      on_off_status = val;
      send_status();
      set_hw_status();
    }
    
  } else if (message.type == V_DIMMER || message.type == V_PERCENTAGE) {
    Serial.print( "V_DIMMER command: " );
    Serial.println(message.data);
    val = atoi(message.data);
    if (val >= 0 and val <=100) {
      dimmerlevel = val;
      send_status();
      set_hw_status();
    }
    
  } else if (message.type == V_VAR1 ) {
    Serial.print( "V_VAR1 command: " );
    Serial.println(message.data);
    val = atoi(message.data);
    if (val >= 0 and val <= 2000) {
      fadespeed = val;
    }
    
  } else {
    Serial.println( "Invalid command received..." );
    return;
  }

}

void set_rgb(int r, int g, int b) {
  analogWrite(REDPIN, r);
  analogWrite(GREENPIN, g);
  analogWrite(BLUEPIN, b);
}

void set_hw_status() {
  int r = on_off_status * (int)(red * dimmerlevel/100.0);
  int g = on_off_status * (int)(green * dimmerlevel/100.0);
  int b = on_off_status * (int)(blue * dimmerlevel/100.0);

  if (fadespeed >0) {
    
    float dr = (r - r0) / float(fadespeed);
    float db = (b - b0) / float(fadespeed);
    float dg = (g - g0) / float(fadespeed);
    
    for (int x = 0;  x < fadespeed; x++) {
      set_rgb(r0 + dr*x, g0 + dg*x, b0 + db*x);
      delay(100);
    }
  }

  set_rgb(r, g, b);
 
  r0 = r;
  b0 = b;
  g0 = g;
  
}


void send_status() {
  send(rgbMsg.set(rgbstring));
  send(lightMsg.set(on_off_status));
  send(dimmerMsg.set(dimmerlevel));
}
