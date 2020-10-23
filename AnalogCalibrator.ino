
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
bool onelast = true;
bool waitpower = false;
String caldata = "";

int32_t averagerssi = 0;
uint16_t rssicount = 0;
int countincomming = 0;
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
 String calvoltage;  //captures first data String
 String calcurrent;  //captures first data String
 String calphase;
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
esp_now_peer_info_t peerInfo;
esp_err_t result;
bool gotdevice = false;
void receiveEvent(int howMany) {
    Serial.print("88,");
     countincomming = 0;
    char ssid[14];
    while (WireSlave.available()) // loop through all but the last byte
    {
       
        char c = WireSlave.read(); 
        ssid[countincomming] = c;// receive byte as a character
        //Serial.print(c);  
        countincomming++;// print the character
    }
    ssid[13] = 0;
    serial = String(ssid);
   // int x = WireSlave.read();   // receive byte as an integer
   // Serial.println(x);          // print the integer
    Serial.println(serial);
}

// called to fill the output buffer
void requestEvent() {
    if (countincomming == 13) {
        WireSlave.write(253);
        externali2cgood = true;
        SendCalStatus();
    }
    else {
        WireSlave.write(1);////////////////////////////////////////////////need to do somthing on otherside
    }
    Serial.printf("00,sent back %d bytes\n", countincomming);
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
    //Serial.println(ssidss);
    String pas = "06082020";
    String ssids = "ChatterBox98F4AB7DBD68h";
    ssidss.toCharArray(ssid, ssidss.length() + 1);
    pas.toCharArray(password, pas.length() + 1);
  
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        ////put a kick out and let know no connect
       // Serial.print(".");
        
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
    if (in > 254) {
        Serial.printf("55,Inputs,Pass,%d\n",in);
    }
    else {
        Serial.printf("55,Inputs,Fail,%d\n", in);

        test = false;
    }
    if (systemp > 0 && systemp < 260) {
        Serial.printf("55,systemp,Pass,%f\n", systemp);
    }
    else {
        Serial.printf("55,systemp,Fail,%f\n", systemp);

        test = false;
    }
    averagerssi = averagerssi / rssicount;
   
    if (averagerssi >-40) {
        Serial.printf("55,wifi,Pass,%d\n", averagerssi);
    }
    else {
        Serial.printf("55,wifi,Fail,%d\n", averagerssi);

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
    float volts = calvoltage.toFloat();
    if (volts == 0)volts = 120;
    float currents = calcurrent.toFloat();
    if (currents == 0)currents = 4.5;
    float curhigh = (currents*.02)+currents;
   
    float curlow = currents - (currents * .02);
    float phases = calphase.toFloat();
    if (phases == 0)phases = .4900;
    float phasehigh = (phases * .02) + phases;
    float phaselow = phases - (phases * .02);
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
    if (vol > volts-(volts*.02) && vol < volts+(volts*.02)) {///////////with in 2%
        Serial.printf("55,voltage,Pass,%f\n", vol);
    }
    else {
        Serial.printf("55,voltage,Fail,%f\n", vol);

        test = false;
    }
    if (cur1 > curlow && cur1 < curhigh) {
        Serial.printf("55,cur1,Pass,%f\n", cur1);
    }
    else {
        Serial.printf("55,cur1,Fail,%f\n", cur1);

        test = false;
    }
    if (cur2 > curlow && cur2 < curhigh) {
        Serial.printf("55,cur2,Pass,%f\n", cur2);
    }
    else {
        Serial.printf("55,cur2,Fail,%f\n", cur2);

        test = false;
    }
    if (cur3 > curlow && cur3 < curhigh) {
        Serial.printf("55,cur3,Pass,%f\n", cur3);
    }
    else {
        Serial.printf("55,cur3,Fail,%f\n", cur3);

        test = false;
    }
    if (cur4 > curlow && cur4 < curhigh) {
        Serial.printf("55,cur4,Pass,%f\n", cur4);
    }
    else {
        Serial.printf("55,cur4,Fail,%f\n", cur4);

        test = false;
    }
    if (freq > 56 && freq < 64) {
        Serial.printf("55,freq,Pass,%f\n", freq);
    }
    else {
        Serial.printf("55,freq,Fail,%f\n", freq);

        test = false;
    }
    if (pfa > phaselow && pfa < phasehigh) {
        Serial.printf("55,pfa,Pass,%f\n", pfa);
    }
    else {
        Serial.printf("55,pfa,Fail,%f\n", pfa);

        test = false;
    }
    if (pfb > phaselow && pfb < phasehigh) {
        Serial.printf("55,pfb,Pass,%f\n", pfb);
    }
    else {
        Serial.printf("55,pfb,Bad,%f\n", pfb);

        test = false;
    }
    if (pfc > phaselow && pfc < phasehigh) {
        Serial.printf("55,pfc,Pass,%f\n", pfc);
    }
    else {
        Serial.printf("55,pfc,Bad,%f\n", pfc);

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
  if (hst > -2 && hst < 0) {
      Serial.printf("55,HST,Pass,%f\n", hst);
      //Serial.println(hst);
  }
  else {
      Serial.printf("55,HST,Fail,%f\n", hst);

      test = false;
  }
  if(lst > -2 && lst < 0) {
      Serial.printf("55,LST,Pass,%f\n", lst);
      //Serial.println(lst);
  }
  else {
      Serial.printf("55,LST,Fail,%f\n", lst);
      test = false;
  }
  if (hsp > 230 && hsp < 240) {
      Serial.printf("55,HSP,Pass,%f\n", hsp);
      //Serial.println(hsp);
  }
  else {
      Serial.printf("55,HSP,Fail,%f\n", hsp);
      test = false;
  }
  if (lsp > 230 && lsp < 240) {
      Serial.printf("55,LSP,Pass,%f\n", lsp);
      //Serial.println(lsp);
  } 
  else {
      Serial.printf("55,LSP,Fail,%f\n", lsp);
      test = false;
  }
  if (vol > 8 && vol < 60) {////////////////////////////////////////////////////look at this later
      Serial.printf("55,Vol,Pass,%f\n", vol);
      //Serial.println(vol);
  }
  else {
      Serial.printf("55,Vol,Fail,%f\n", vol);
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
        if (ADCData != "{1}") return ADCCheck();
        return false;
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
void Resetfornextdevice() {
    WiFi.disconnect();
    esp_now_del_peer(peerInfo.peer_addr);
    sysdata="";
    
   Powerdata="";
 
    ADCData="";
    gotdevice = false;
    testfinishedgood = false;
   adctestdone = false;
   powerdonegood = false;
   sysdonegood = false;
    wifitestgood = false;
     externali2cgood = false;
   adccalfinished = false;
    onelast = true;
   datarecieved = false;
    connectedwifi = false;
    // Variable to store if sending data was successful
   success="";
    DACcount = 0;
     serial = "";
      averagerssi = 0;
      rssicount = 0;
      caldata = "";
       calvoltage="";  //captures first data String
       calcurrent="";  //captures first data String
       calphase="";
       waitpower = false;
}
bool volcurcal() {
    HTTPClient Hclient;
    String volcurcalibr = "http://192.168.4.1/calgain?voltmess=" + calvoltage + "&curmess=" + calcurrent;
    Hclient.begin(volcurcalibr.c_str());
    int httpCode = Hclient.GET();                                        //Make the request

    if (httpCode == 200) { //Check for the returning code
        Serial.println("00,Phase Voltage current data sent");
        return true;
    }

    else {
        Serial.printf("Error on HTTP request %d\n", httpCode);
        return false;
    }

    if (Hclient.connected())Hclient.end(); //Free the resources
}
bool phasecal() {
    HTTPClient Hclient;
    String phasecalibr = "http://192.168.4.1/phaseadjust?pfa=" + calphase;
    Hclient.begin(phasecalibr.c_str());
    int httpCode = Hclient.GET();                                        //Make the request

    if (httpCode == 200) { //Check for the returning code

        Serial.println("00,Phase Cal data sent");
        return true;
    }

    else {
        Serial.printf("Error on HTTP request %d\n", httpCode);
        return false;
    }

    if (Hclient.connected())Hclient.end(); //Free the resources
}
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
   ///////////////////////////////////////////////////////////////////////////////////////////////start thread to reset if error accurs after 10 seconds no action
  // Send message via ESP-NOW
    ///////////////////////////////////////we need to be able to send a reset test command to board 
   // need to do something with the serial number we get from i2c and the gotdevice holder
    if (Serial.available()) {
        Serial.println("got ser");
       
            caldata = Serial.readString();
            //Serial.println(caldata);
           // Serial.println(caldata.substring(0, 2));
        if (caldata.substring(0, 2) == "88") {
           // Serial.println("Got Data");
            uint8_t d1, d2, d3, d4, d5;
            d1 = caldata.indexOf(',');  //finds location of first ,
            d2 = caldata.indexOf(',',d1+1);
            d3 = caldata.indexOf(',', d2 + 1);
            calvoltage = caldata.substring(d1+1, d2);   //captures first data String
            calcurrent = caldata.substring(d2 + 1, d3);   //captures first data String
            calphase = caldata.substring(d3+1);   //captures first data String
          // Serial.println(calvoltage);
          // Serial.println(calcurrent);
          // Serial.println(calphase);
        }
    }

    if (connectedwifi) {
         averagerssi += WiFi.RSSI();
        rssicount++;
        delay(500);
        if (!sysdonegood) {
            if (getsysdata()) {
                sysdonegood = true;
                testfinishedgood = true;
                 onelast = true;
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
                onelast = true;
            }
            else {
                if (waitpower)delay(5000);
                powerdonegood = false;
                testfinishedgood = false;
                volcurcal();
                delay(500);
                phasecal();
                waitpower = true;
            }
        }
        delay(1);

        if (!adctestdone) {
            if (getadcdata()) {
                adctestdone = true;
                testfinishedgood = true;
                onelast = true;
            }
            else {
                adctestdone = false;
                testfinishedgood = false;
            }
        }
        if (!testfinishedgood || onelast) {
            onelast = false;
            SendCalStatus();
            if (testfinishedgood)Resetfornextdevice();
        }
  }
  if(datarecieved)
  {
    datarecieved=false;

    //Serial.println ("GotData");
    //Serial.print("Command : ");
    Serial.printf("00, command %d\n",indatastruct.Command);
    if (indatastruct.Command == 1) {
        if (!gotdevice) {
            memcpy(peerInfo.peer_addr, indatastruct.broadcastAddress, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
           
            result = esp_now_add_peer(&peerInfo);
            if (result != ESP_OK) {
                Serial.printf("99,falied to add pier %d %02x:%02x:%02x:%02x:%02x:%02x\n ",result, peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2], peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
                return;
            }
            else Serial.printf("00, start with pier  %02x:%02x:%02x:%02x:%02x:%02x \n", peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2], peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);

            outdatastruct.Command = 10;
            result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&outdatastruct, sizeof(struct_data));
            if (result == ESP_OK);// Serial.println("Sent with success");
            else Serial.println("99,Error sending the data1");
        }
        else {
            Serial.println("55,device attached");
        }
  
  
 
    }
    else if(indatastruct.Command ==10){}
    else if (indatastruct.Command == 21) {//star dac auto process
        gotdevice = true;
        DACcount = 0;
        dacWrite(25, DACval[DACcount]);
        outdatastruct.Command = 25;//SET DAC
        outdatastruct.DAC = DACval[DACcount];// reset dac
        outdatastruct.Dacvolt = voltage[DACcount];
         result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&outdatastruct, sizeof(struct_data));
         if (result == ESP_OK);//  Serial.println("Sent with success");
        else Serial.printf("99,Error sending the 2 data %d %02x:%02x:%02x:%02x:%02x:%02x \n", result, peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2], peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
    }
    else if (indatastruct.Command == 35) {
        
        dacWrite(25, 125);
        Serial.println("00,Finish Ana CAL");
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
        else Serial.println("99,Error sending the data3");

      }
    else if(indatastruct.Command ==30){
        dacWrite(25,indatastruct.DAC);
        outdatastruct.Command = 25;//SET DAC
        outdatastruct.DAC = indatastruct.DAC;// reset dac
         result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&outdatastruct, sizeof(struct_data));
         if (result == ESP_OK);//  Serial.println("Sent with success");
        else Serial.println("99,Error sending the data4");
      }
    else Serial.println("99,unknown command sent back");
  }
  delay(1);

  WireSlave.update();
}
