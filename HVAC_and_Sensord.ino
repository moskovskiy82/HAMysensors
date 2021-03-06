#define SN "Bedroom"
#define SV "2.1"

/*
#define MODE_AUTO   1
#define MODE_HEAT   2
#define MODE_COOL   3
#define MODE_DRY    4
#define MODE_FAN    5
#define MODE_MAINT  6

#define FAN_AUTO    0
#define FAN_1       1
#define FAN_2       2
#define FAN_3       3
#define FAN_4       4
#define FAN_5       5

#define VDIR_AUTO   0
#define VDIR_MANUAL 0
#define VDIR_SWING  1
#define VDIR_UP     2
#define VDIR_MUP    3
#define VDIR_MIDDLE 4
#define VDIR_MDOWN  5
#define VDIR_DOWN   6

#define HDIR_AUTO   0
#define HDIR_MANUAL 0
#define HDIR_SWING  1
#define HDIR_MIDDLE 2
#define HDIR_LEFT   3
#define HDIR_MLEFT  4
#define HDIR_MRIGHT 5
#define HDIR_RIGHT  6
 
*/
 
#define MY_NODE_ID 31

//Disable blinking
#define MY_DEFAULT_ERR_LED A10
#define MY_DEFAULT_TX_LED A10
#define MY_DEFAULT_RX_LED A10

#define MY_RADIO_NRF24
#define MY_RF24_CE_PIN 8

#include <SPI.h>
#include <MySensors.h>
#include <DHT.h> 
#include <math.h>
#include <Wire.h>
#include <FujitsuHeatpumpIR.h>

//Define connections
#define CHILD_ID_HVAC 0
#define CHILD_ID_HUM 2
#define CHILD_ID_TEMP 3
#define CHILD_ID_MQ 4

#define DHT_PIN 7
const int MQ_Pin = A3;
DHT dht;

//MQ+DHT
long MQ_Millis = 0;
long MQ_interval = 60000;
long DHT_Millis = 0;
long DHT_interval = 60000;
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgMQ(CHILD_ID_MQ, V_LEVEL);

//HVAC
//Some global variables to hold the states
int POWER_STATE;
int TEMP_STATE;
int FAN_STATE;
int MODE_STATE;
int VDIR_STATE;
int HDIR_STATE;
String HA_MODE_STATE = "Off";
String HA_FAN_STATE = "Min";

IRSenderPWM irSender(3);       // IR led on Arduino digital pin 3, using Arduino PWM
HeatpumpIR *heatpumpIR = new FujitsuHeatpumpIR();

MyMessage msgHVACSetPointC(CHILD_ID_HVAC, V_HVAC_SETPOINT_COOL);
MyMessage msgHVACSpeed(CHILD_ID_HVAC, V_HVAC_SPEED);
MyMessage msgHVACFlowState(CHILD_ID_HVAC, V_HVAC_FLOW_STATE);

bool initialValueSent = false;

void before() 
{
  dht.setup(DHT_PIN);
  loadState(CHILD_ID_HVAC);
}
void setup()
{ 
Serial.begin(115200);
}
void presentation()  
{ 
  sendSketchInfo(SN, SV);
  present(CHILD_ID_HUM, S_HUM);
  present(CHILD_ID_TEMP, S_TEMP);
  present(CHILD_ID_MQ, S_AIR_QUALITY);
  present(CHILD_ID_HVAC, S_HVAC, "");
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
  if (!initialValueSent) 
  {
    Serial.println("Sending initial values");
    POWER_STATE = 0;
    MODE_STATE = MODE_AUTO;
    FAN_STATE = 1;
    TEMP_STATE = 24;
    sendHeatpumpCommand();
    sendNewStateToGateway();
    
    request(CHILD_ID_HVAC, V_HVAC_SETPOINT_COOL);
    wait(3000, C_SET, V_HVAC_SETPOINT_COOL);
    request(CHILD_ID_HVAC, V_HVAC_SPEED);
    wait(3000, C_SET, V_HVAC_SPEED);
    request(CHILD_ID_HVAC, V_HVAC_FLOW_STATE);
    wait(3000, C_SET, V_HVAC_FLOW_STATE);
    //initialValueSent = true;
  }
}

void receive(const MyMessage &message) {
  if (message.isAck()) {
     Serial.println("This is an ack from gateway");
     //return;
  }

  Serial.print("Incoming message for: ");
  Serial.print(message.sensor);

  String recvData = message.data;
  recvData.trim();

  Serial.print(", New status: ");
  Serial.println(recvData);
  switch (message.type) {
    case V_HVAC_SPEED:
      Serial.println("V_HVAC_SPEED");

      if(recvData.equalsIgnoreCase("auto")) {
        FAN_STATE = 0;
        HA_FAN_STATE = "Auto";
      }
      else if(recvData.equalsIgnoreCase("min")) {
        FAN_STATE = 1;
        HA_FAN_STATE = "Min";
      }
      else if(recvData.equalsIgnoreCase("normal")) {
        FAN_STATE = 2;
        HA_FAN_STATE = "Normal";
      }
      else if(recvData.equalsIgnoreCase("max")) {
        FAN_STATE = 3;
        HA_FAN_STATE = "Max";
      }
    break;

    case V_HVAC_SETPOINT_COOL:
      Serial.println("V_HVAC_SETPOINT_COOL");
      TEMP_STATE = message.getFloat();
      Serial.println(TEMP_STATE);
    break;

    case V_HVAC_FLOW_STATE:
      Serial.println("V_HVAC_FLOW_STATE");
      if (recvData.equalsIgnoreCase("coolon")) {
        POWER_STATE = 1;
        MODE_STATE = MODE_COOL;
        HA_MODE_STATE = "CoolOn";
      }
      else if (recvData.equalsIgnoreCase("heaton")) {
        POWER_STATE = 1;
        MODE_STATE = MODE_HEAT;
        HA_MODE_STATE = "HeatOn";
      }
      else if (recvData.equalsIgnoreCase("autochangeover")) {
        POWER_STATE = 1;
        MODE_STATE = MODE_AUTO;
        HA_MODE_STATE = "AutoChangeOver";
      }
      else if (recvData.equalsIgnoreCase("off")){
        POWER_STATE = 0;
        HA_MODE_STATE = "Off";
      }
      initialValueSent = true;
      break;
  }
  sendHeatpumpCommand();
  sendNewStateToGateway();
}

void sendNewStateToGateway() {
  send(msgHVACSetPointC.set(TEMP_STATE));
  wait(1000, C_SET, V_HVAC_SETPOINT_COOL);
  char fan_state[HA_FAN_STATE.length() + 1];
  HA_FAN_STATE.toCharArray(fan_state, HA_FAN_STATE.length() + 1);
  send(msgHVACSpeed.set(fan_state));
  wait(1000, C_SET, V_HVAC_SPEED);
  char mode_state[HA_MODE_STATE.length() + 1];
  HA_MODE_STATE.toCharArray(mode_state, HA_MODE_STATE.length() + 1);
  send(msgHVACFlowState.set(mode_state));
  wait(1000, C_SET, V_HVAC_FLOW_STATE);
}

void sendHeatpumpCommand() {
  Serial.println("Power = " + (String)POWER_STATE);
  Serial.println("Mode = " + (String)MODE_STATE);
  Serial.println("Fan = " + (String)FAN_STATE);
  Serial.println("Temp = " + (String)TEMP_STATE);

  heatpumpIR->send(irSender, POWER_STATE, MODE_STATE, FAN_STATE, TEMP_STATE, VDIR_AUTO, HDIR_AUTO);
}
