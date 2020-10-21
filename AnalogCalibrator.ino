
#include <AsyncTCP.h>
#include <WireSlave.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_SLAVE_ADDR 0x10
String System_DigitData ="http://192.168.4.1/readcpu";
String sysdata;
String Power_Data ="http://192.168.4.1/readpower";
String Powerdata;
String  ADC_Data = "http://192.168.4.1/readADC";
String ADCData;
String Power_cal_Phase = "http://192.168.4.1/phaseadjust";
bool testfinishedgood = false;
bool adctestdone = false;
bool powerdonegood = false;
bool sysdonegood = false;
bool wifitestgood = false;
bool externali2cgood = false;
bool adccalfinished = false;
String Power_cal_vc = "http://192.168.4.1/calgain";
struct SpiRamAllocator {
    void* allocate(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    void deallocate(void* pointer) {
        heap_caps_free(pointer);
    }
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

////////////////////////////////////////////////////////This is the server that will do the calibrations 
// REPLACE WITH THE MAC Address of your receiver 
uint8_t broadcastAddress[] = {0x24, 0x0A, 0xC4, 0x47, 0xCC, 0x68};///this is server will a
bool datarecieved=false;
bool connectedwifi = false;
// Variable to store if sending data was successful
String success;

String serial="";
 uint16_t DACval[]={255,254,253,252,250,247,245,240,235,230,220,210,200,175,150,60,40,30,20,11,7,4,3,2,0};
 float voltage[]={3.143,3.130,3.117,3.106,3.084,3.048,3.023,2.963,2.906,2.845,2.727,2.608,2.489,2.195,1.898,0.822,0.585,0.466,0.346,0.241,0.191,0.154,0.145,0.133,0.10};
 uint8_t DACcount = 0;
 
//Structure example to send data
//Must match the receiver structure
typedef struct struct_data {
   uint8_t broadcastAddress[6];
   uint8_t DAC;///this is what measurement the dac is 
   uint8_t Command;//list 255 start cal,1 send mac back,ok 10,20 next DAC,30 go to dac sent,25 dac set,35 finished
   float Dacvolt;
};
struct_data outdatastruct;
struct_data indatastruct;
// Create a struct_message called BME280Readings to hold sensor readings

esp_err_t result;
bool gotdevice = false;
void receiveEvent(int howMany) {
    Serial.print("88,");
    int i = 0;
    char ssid[14];
    while (WireSlave.available()) // loop through all but the last byte
    {
       
        char c = WireSlave.read(); 
        ssid[i] = c;// receive byte as a character
        //Serial.print(c);  
        i++;// print the character
    }
    ssid[13] = 0;
    serial = String(ssid);
   // int x = WireSlave.read();   // receive byte as an integer
   // Serial.println(x);          // print the integer
    Serial.println(serial);
}

// called to fill the output buffer
void requestEvent() {
    
    WireSlave.write(253);
    Serial.println("got i2c request");
}
// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
 // Serial.print("\r\nLast Packet Send Status:\t");
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
   // success = "Delivery Success :)";
  }
  else{
   // success = "Delivery Fail :(";
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  
  memcpy(&indatastruct, incomingData, sizeof(struct_data));
  //Serial.print("Bytes received: ");
 // Serial.println(len);
  datarecieved=true;
}

void changewifimode() {
  
    char ssid[33];
    char password[33];
    String ssidss = "ChatterBox"+serial;
    Serial.println(ssidss);
    String pas = "06082020";
    String ssids = "ChatterBox98F4AB7DBD68h";
    ssidss.toCharArray(ssid, ssidss.length() + 1);
    pas.toCharArray(password, pas.length() + 1);
  
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        ////put a kick out and let know no connect
        Serial.print(".");
        
    }
    wifitestgood = true;
    SendCalStatus();
    Serial.println("00,Connted to device");
    connectedwifi = true;
}
bool SysCheck() {
    SpiRamJsonDocument doc(1000);
    deserializeJson(doc, sysdata);///////////////////////////////////////error catch later
    bool test = true;
   uint8_t in = doc["Inputs"].as<uint8_t>();
    float systemp = doc["systemp"].as<float>();
    if (in == 256) {
        Serial.printf("00,Inputs,Good\n");
    }
    else {
        Serial.printf("99,Inputs,Bad,%f\n", in);

        test = false;
    }
    if (systemp > 0 && systemp < 260) {
        Serial.printf("00,systemp,Good\n");
    }
    else {
        Serial.printf("99,systemp,Bad,%f\n", systemp);

        test = false;
    }
    doc.clear();
    return test;
}
bool getsysdata() {
    HTTPClient Hclient;
    Hclient.begin(System_DigitData.c_str());
    int httpCode = Hclient.GET();                                        //Make the request

    if (httpCode > 0) { //Check for the returning code

        sysdata = Hclient.getString();
        //Serial.println(httpCode);
       // Serial.println(sysdata);
        return SysCheck();
    }

    else {
        Serial.printf("Error on HTTP request %d\n", httpCode);
    }

    if (Hclient.connected())Hclient.end(); //Free the resources
}
bool PowerCheck() {
    SpiRamJsonDocument doc(1000);
    deserializeJson(doc, Powerdata);///////////////////////////////////////error catch later
    bool test = true;
    float vol = doc["volt"].as<float>();
    float cur1 = doc["cur1"].as<float>();
    float cur2 = doc["cur2"].as<float>();
    float cur3 = doc["cur3"].as<float>();
    float cur4 = doc["cur4"].as<float>();
   
    float freq = doc["freq"].as<float>();
    float pfa = doc["pfa"].as<float>();
    float pfb = doc["pfb"].as<float>();
    float pfc = doc["pfc"].as<float>();
    if (vol > 0 && vol < 260) {
        Serial.printf("00,voltage,Good\n");
    }
    else {
        Serial.printf("99,voltage,Bad,%f\n", vol);

        test = false;
    }
    if (cur1 > 0 && cur1 < 100) {
        Serial.printf("00,cur1,Good\n");
    }
    else {
        Serial.printf("99,cur1,Bad,%f\n", cur1);

        test = false;
    }
    if (cur2 > 0 && cur2 < 100) {
        Serial.printf("00,cur2,Good\n");
    }
    else {
        Serial.printf("99,cur2,Bad,%f\n", cur2);

        test = false;
    }
    if (cur3 > 0 && cur3 < 100) {
        Serial.printf("00,cur3,Good\n");
    }
    else {
        Serial.printf("99,cur3,Bad,%f\n", cur3);

        test = false;
    }
    if (cur4 > 0 && cur4 < 100) {
        Serial.printf("00,cur4,Good\n");
    }
    else {
        Serial.printf("99,cur4,Bad,%f\n", cur4);

        test = false;
    }
    if (freq > 0 && freq < 100) {
        Serial.printf("00,freq,Good\n");
    }
    else {
        Serial.printf("99,freq,Bad,%f\n", freq);

        test = false;
    }
    if (pfa > 0 && pfa < 100) {
        Serial.printf("00,pfa,Good\n");
    }
    else {
        Serial.printf("99,pfa,Bad,%f\n", pfa);

        test = false;
    }
    if (pfb > 0 && pfb < 100) {
        Serial.printf("00,pfb,Good\n");
    }
    else {
        Serial.printf("99,pfb,Bad,%f\n", pfb);

        test = false;
    }
    if (pfc > 0 && pfc < 100) {
        Serial.printf("00,pfc,Good\n");
    }
    else {
        Serial.printf("99,pfc,Bad,%f\n", pfc);

        test = false;
    }

    doc.clear();
    return test;
}
bool getpowdata() {
    HTTPClient Hclient;
    Hclient.begin(Power_Data.c_str());
    int httpCode = Hclient.GET();                                        //Make the request

    if (httpCode > 0) { //Check for the returning code

     Powerdata = Hclient.getString();
        //Serial.println(httpCode);
       // Serial.println(Powerdata);
        return PowerCheck();
    }

    else {
        Serial.printf("Error on HTTP request %d\n", httpCode);
    }

    if (Hclient.connected())Hclient.end(); //Free the resources
}
bool ADCCheck() {
    SpiRamJsonDocument doc(1000);
    deserializeJson(doc, ADCData);///////////////////////////////////////error catch later
   
    
  float hst =  doc["HighSideTemp"].as<float>();
  float lst= doc["LowSideTemp"].as<float>();
  float hsp= doc["HighSidePres"].as<float>();
  float lsp = doc["LowSidePres"].as<float>();
  float vol = doc["voltage"].as<float>();
  bool test = true;
  if (hst > 0 && hst < 100) {
      Serial.printf("00,HST,Good\n");
  }
  else {
      Serial.printf("99,HST,Bad,%f\n", hst);

      test = false;
  }
  if(lst > 0 && lst < 100) {
      Serial.printf("00,LST,Good\n");
  }
  else {
      Serial.printf("99,LST,Bad,%f\n", lst);
      test = false;
  }
  if (hsp > 0 && hsp < 100) {
      Serial.printf("00,HSP,Good\n");
  }
  else {
      Serial.printf("99,HSP,Bad,%f\n", hsp);
      test = false;
  }
  if (lsp > 0 && lsp < 100) {
      Serial.printf("00,LSP,Good\n");
  } 
  else {
      Serial.printf("99,LSP,Bad,%f\n", lsp);
      test = false;
  }
  if (vol > 0 && vol < 100) {
      Serial.printf("00,Vol,Good\n");
  }
  else {
      Serial.printf("99,Vol,Bad,%f\n", vol);
      test = false;
  }
  doc.clear();
  return test;
}
bool getadcdata() {
    HTTPClient Hclient;
    Hclient.begin(ADC_Data.c_str());
    int httpCode = Hclient.GET();                                        //Make the request

    if (httpCode > 0) { //Check for the returning code

        ADCData = Hclient.getString();
       // Serial.println(httpCode);
     //   Serial.println(ADCData);
        return ADCCheck();
    }

    else {
        Serial.printf("Error on HTTP request %d\n", httpCode);
        return false;
    }

    if (Hclient.connected())Hclient.end(); //Free the resources
}
   // Register peer
void SendCalStatus() {

    Serial.printf("22,%d,%d,%d,%d,%d,%d,%d\n",externali2cgood,adccalfinished,wifitestgood,sysdonegood,powerdonegood,adctestdone,testfinishedgood);
}
esp_now_peer_info_t peerInfo;
void setup() {
  // Init Serial Monitor
    bool success = WireSlave.begin(SDA_PIN, SCL_PIN, I2C_SLAVE_ADDR);
    if (!success) {
        Serial.println("I2C slave init failed");
        while (1) delay(100);
    }
    WireSlave.onReceive(receiveEvent);
    WireSlave.onRequest(requestEvent);
    Serial.begin(115200);

  // Init BME280 sensor
  
 
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  Serial.println(WiFi.macAddress());
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  

  
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
 
}
 
void loop() {
   
  // Send message via ESP-NOW
    if (connectedwifi) {
        delay(500);
        if (!sysdonegood) {
            if (getsysdata()) {
                sysdonegood = true;
                testfinishedgood = true;
            }
            else {
                sysdonegood = false;
                testfinishedgood = false;
            }
        }
        delay(1);
        if (!powerdonegood) {
            if (getpowdata()) {
                powerdonegood = true;
                testfinishedgood = true;
            }
            else {
                powerdonegood = false;
                testfinishedgood = false;
            }
        }
        delay(1);

        if (!adctestdone) {
            if (getadcdata()) {
                adctestdone = true;
                testfinishedgood = true;
            }
            else {
                adctestdone = false;
                testfinishedgood = false;
            }
        }
        SendCalStatus();
  }
  if(datarecieved)
  {
    datarecieved=false;
    //Serial.println ("GotData");
    //Serial.print("Command : ");
    //Serial.println(indatastruct.Command);
    if (indatastruct.Command == 1) {
        if (!gotdevice) {
            memcpy(peerInfo.peer_addr, indatastruct.broadcastAddress, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            gotdevice = true;
            // Add peer        
            if (esp_now_add_peer(&peerInfo) != ESP_OK) {
                Serial.printf("99,falied to add pier");
                return;
            }

            outdatastruct.Command = 10;
            result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&outdatastruct, sizeof(struct_data));
            if (result == ESP_OK);// Serial.println("Sent with success");
            else Serial.println("99,Error sending the data");
        }
        else {
            Serial.println("55,device attached");
        }
  
  
 
    }
    else if(indatastruct.Command ==10){}
    else if (indatastruct.Command == 21) {//star dac auto process
        externali2cgood = true;
        SendCalStatus();
        DACcount = 0;
        dacWrite(25, DACval[DACcount]);
        outdatastruct.Command = 25;//SET DAC
        outdatastruct.DAC = DACval[DACcount];// reset dac
        outdatastruct.Dacvolt = voltage[DACcount];
         result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&outdatastruct, sizeof(struct_data));
         if (result == ESP_OK);//  Serial.println("Sent with success");
        else Serial.println("99,Error sending the data");
    }
    else if (indatastruct.Command == 35) {
        
       
        Serial.println("00,Finished onto the next device");
        adccalfinished = true;
        SendCalStatus();
        changewifimode();
    }
    else if(indatastruct.Command ==20){
      
        DACcount++;
        dacWrite(25, DACval[DACcount]);
        outdatastruct.Command = 25;//SET DAC
        outdatastruct.DAC = DACval[DACcount];// reset dac
        outdatastruct.Dacvolt = voltage[DACcount];
        result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&outdatastruct, sizeof(struct_data));
        if (result == ESP_OK);// Serial.println("Sent with success");
        else Serial.println("99,Error sending the data");

      }
    else if(indatastruct.Command ==30){
        dacWrite(25,indatastruct.DAC);
        outdatastruct.Command = 25;//SET DAC
        outdatastruct.DAC = indatastruct.DAC;// reset dac
         result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&outdatastruct, sizeof(struct_data));
         if (result == ESP_OK);//  Serial.println("Sent with success");
        else Serial.println("99,Error sending the data");
      }
    else Serial.println("99,unknown command sent back");
  }
  delay(1);

  WireSlave.update();
}
