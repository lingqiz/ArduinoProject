#include <SPI.h>
#include <MFRC522.h>

#define MFR_RST_PIN		49		
#define MFR_SS_PIN		53		

MFRC522 rfid(MFR_SS_PIN, MFR_RST_PIN);
//Libraries for MFRC522

#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11

Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed but DI

#define WLAN_SSID       "515"        // cannot be longer than 32 characters!
#define WLAN_PASS       "sustc2013515"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2
//Libraries for CC3000

#define IDLE_TIMEOUT_MS  3000      

// Identify Code of this Arduino is 695858119
#define WEBSITE      "idoorsustc.sinaapp.com"
#define PATH         String("/query/?identify_code=695858119")
#define SCAN         "&phase=scan"
#define SYNC         "&phase=sync"
#define LOCK         "&lock="
#define CARD_ID      "&card_id="

static int online = 0;
static int lock   = 0;
static uint32_t ip = 0;

#define Y_LED   31
#define R_LED   33
#define G_LED   35

#include <Servo.h> 
#define SERVO_CONTROL 26
Servo servo;


void setup()
{
  Serial.begin(115200);
  
  // Initilize RFID Reader
  SPI.begin();			// Init SPI bus
  rfid.PCD_Init();		// Init MFRC522
  Serial.println("MFRC522 Started!");
  
  // Initilize Servo
  servo.attach(SERVO_CONTROL); 
  
  pinMode(31, OUTPUT);
  pinMode(30, OUTPUT);
  digitalWrite(30, LOW);
  digitalWrite(31, LOW);
  
  pinMode(33, OUTPUT);
  pinMode(32, OUTPUT);
  digitalWrite(32, LOW);
  digitalWrite(33, LOW);
  
  pinMode(35, OUTPUT);
  pinMode(34, OUTPUT);
  digitalWrite(34, LOW);
  digitalWrite(35, LOW);

  servo.write(160);  // Assume at angle = 0, the door is locked


  cc3000.begin();  // Initialize CC3000 
  if(cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY))
  {
    Serial.print("Connect to WLAN : "); Serial.println(WLAN_SSID);

    Serial.println(F("Request DHCP"));
    while (!cc3000.checkDHCP())
    {
      delay(100); // ToDo: Insert a DHCP timeout!
      /* Wait for DHCP to complete */
    }  
    
    while (! displayConnectionDetails()) {
    delay(1000);
    }

    Serial.print(WEBSITE); Serial.print(F(" -> "));
    while (ip == 0) {
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
    }
    
    cc3000.printIPdotsRev(ip); Serial.println();
    if(ip) online = 1;
  }
  
}

static boolean Y_LED_STATE = LOW;
void loop()
{
  Y_LED_STATE = !Y_LED_STATE;
  digitalWrite(Y_LED, Y_LED_STATE);

  if(rfid.PICC_IsNewCardPresent())
  {
    if(rfid.PICC_ReadCardSerial())
    {
       unsigned long UID;
       getUID(&UID, rfid.uid.uidByte);
       Serial.print("CARD UID : ");
       Serial.println(UID);
          
       rfid.PICC_HaltA();
       findCardHandler(UID);
       return;
     }
  }
  
  
  if(online)
  {
    char op_code = queryServer(makeSyncStr());
    Serial.print("Receive Op Code: "); Serial.println(op_code);
    if(op_code == '1') servoRotate();
  }
  
  delay(500);
}

void findCardHandler(unsigned long UID)
{
  digitalWrite(Y_LED, LOW);
  char op_code = queryServer(makeScanStr(UID));
  Serial.println(op_code);

  if(op_code == '1') servoRotate();
  else if(op_code == '2')
  {
    digitalWrite(Y_LED, HIGH);
    digitalWrite(G_LED, HIGH);
    digitalWrite(R_LED, HIGH);
    
    delay(1000);

    digitalWrite(Y_LED, LOW);
    digitalWrite(G_LED, LOW);
    digitalWrite(R_LED, LOW);
  }
  else if(op_code == '0')
  {
    digitalWrite(Y_LED, LOW);
    digitalWrite(R_LED, HIGH);

    delay(1000);

    digitalWrite(R_LED, LOW);    
  }

  return;
}

void servoRotate()
{
  digitalWrite(Y_LED, LOW);
  digitalWrite(G_LED, HIGH);

  if(lock == 0)
  {
    lock = 1;
    for(int i = 160; i >= 0; i--)
    {
      servo.write(i);
      delay(5);
    }
    
  }
  
  else
  {
    lock = 0;
    for(int i = 0; i <= 160 ; i++)
    {
      servo.write(i);
      delay(5);
    }     
  }

  digitalWrite(G_LED, LOW);
}


void getUID(unsigned long* ptr, byte* buffer)
{
  byte* stp = (byte*) ptr;
  for(int idx = 0; idx < 4; idx ++)
  {
     stp[idx] = buffer[idx]; 
  }
}


char queryServer(String request)
{
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, 80);
 
  char query_str[request.length() + 1];
  request.toCharArray(query_str, request.length() + 1);
  Serial.println(query_str);
  
  
  if (www.connected()) 
  {
    www.fastrprint(F("GET "));
    www.fastrprint(query_str);
    www.fastrprint(F(" HTTP/1.1\r\n"));
    www.fastrprint(F("Host: ")); www.fastrprint(WEBSITE); www.fastrprint(F("\r\n"));
    www.fastrprint(F("\r\n"));
    www.println();
  } 
  else 
  {
    Serial.println(F("Connection failed"));    
    return 'E';
  }

  Serial.println();
  delay(500);

  int idx = 0;
  char opcode = 'E';
    while (www.available()) 
    {
      
      char c = www.read();
      
      if(++idx == 312)
        opcode = c;
        
      Serial.print(c);
    }
  www.close();

  return opcode;
}


String makeSyncStr()
{
  String buffer = String(PATH + SYNC);
  buffer = String(buffer + LOCK);
  String state = String(lock, DEC);
  buffer = String(buffer + state);
  return buffer;
}

String makeScanStr(unsigned long uid)
{
  String buffer = String(PATH + SCAN);
  buffer = String(buffer + CARD_ID);
  String card = String(uid, DEC);
  buffer = String(buffer + card);
  return buffer;
}

bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}
