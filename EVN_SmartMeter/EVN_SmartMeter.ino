// Programm zur Auslesung und Entschlüsselung der Kundenschnittstelle eines SmartMeters der EVN (NÖ)
// Bibliothek: https://github.com/rweather/arduinolibs/tree/master/libraries/Crypto
// Aus dem Ordner "libraries" die Bibliothek "Crypto" installieren!
// Prg. funktioniert für einen Atmega2560 (Arduino Mega), Man könnte aber auch die Bibliothek SoftwareSerial benutzen um 
// das Programm auf einem anderen Mikrocontroller zu realisieren.

#include <Crypto.h>
#include <AES.h>
#include <GCM.h>

#define MAX_PLAINTEXT_LEN 300

int byteNumber = 0;
unsigned long timeSinceLastData = 0;
bool processData = false;

struct Vector_GCM {
  const char *name;
  byte keysize;
  unsigned int datasize;
  byte authsize;
  byte ivsize;
  byte tagsize;
  uint8_t key[16];
  byte plaintext[MAX_PLAINTEXT_LEN];
  byte ciphertext[MAX_PLAINTEXT_LEN];
  byte authdata[17];
  byte iv[12];
  byte tag[12];
};

struct IncommingData {
  byte year, month, day, hour, minutes, seconds;
  unsigned long wirkenergiePlus, wirkenergieMinus, momentanleistungPlus, momentanleistungMinus;
  float uL1, uL2, uL3, iL1, iL2, iL3, powerF;
};

IncommingData aktuelleDaten;

Vector_GCM datenMbus = {   //static
  .name        = "AES-128 GCM",
  .keysize     = 16,
  .datasize    = 297,
  .authsize    = 17,
  .ivsize      = 12,
  .tagsize     = 12,
  .key         = {0xA1, 0x2A, 0xD9, 0x82, 0x5A, 0x73, 0x49, 0x5E, 0xA4, 0x09, 0xDE, 0xD5, 0xFA, 0x11, 0xD6, 0x6E},
  .plaintext   = {},
  .ciphertext  = {},
  .authdata    = {},
  .iv          = {},
  .tag         = {},  
};

void setup() {
  for (int i = 0; i < MAX_PLAINTEXT_LEN; i++) {
    datenMbus.plaintext[i] = 0x00;
    datenMbus.ciphertext[i]=0x00;
  }
  Serial.begin(115200);
  Serial1.begin(2400);
}

void loop() {
  if (millis() > timeSinceLastData + 3000) {                // Das SmartMeter sendet alle 5Sek neue Daten-> 
      byteNumber = 0;                                       // Position im Speicher Array nach 3Sek wieder auf null setzen um neue Daten empfangen zu können
    }
  while (Serial1.available() > 0) {                         // Wenn Daten im Buffer der Ser. SChnittstelle sind....
    if (byteNumber < MAX_PLAINTEXT_LEN) {
      datenMbus.ciphertext[byteNumber] = Serial1.read();    // Daten speichern
      byteNumber++;                                         // Zählvariable erhöhen
    }
    timeSinceLastData = millis();
  }
  if (millis() > timeSinceLastData + 3000) {                // Sind mehr als 3 Sekunden vergangen-> Daten entschlüsseln
    if (processData) {
      /*Serial.println("Daten vom Smart Meter: ");          // Ausgabe der eingelesenen Rohdaten(verschlüsselt)
      for (int i = 0; i < MAX_PLAINTEXT_LEN; i++) {   //
        if (datenMbus.ciphertext[i] < 0x10)Serial.print("0");
        Serial.print(datenMbus.ciphertext[i], HEX);
      }
      Serial.println("");*/
      
      for (int i = 0; i < 8; i++) {                          // Initialisation Vektor (IV) bilden (8Byte System Title + 4Byte Frame Counter) ...befinden sich immer an der selben stelle im Datensatz
        datenMbus.iv[i] = datenMbus.ciphertext[i + 11];
      }
      for (int i = 0; i < 4; i++) {
        datenMbus.iv[i + 8] = datenMbus.ciphertext[i + 22];  // FrameCounter anhängen...
      }
      
      for (unsigned int i = 0; i < datenMbus.datasize - 26; i++) { // Anfang der Nachricht "löschen", sodass nur mehr die verschlüsselten Daten in dem Array bleiben
        datenMbus.ciphertext[i] = datenMbus.ciphertext[i + 26];
      }
      for(int i = 256;i<MAX_PLAINTEXT_LEN;i++){
      datenMbus.ciphertext[i]=0x00;
      }
      decrypt_text(datenMbus);
      /*
      Serial.print("Iv: ");
      for (int i = 0; i < 12; i++) {
        if (datenMbus.iv[i] < 0x10)Serial.print("0");
        Serial.print(datenMbus.iv[i], HEX);
      }
      Serial.println();
      Serial.println("Entschluesselte Daten: ");
      for (unsigned int i = 0; i < datenMbus.datasize; i++) {
        if (datenMbus.plaintext[i] < 16)Serial.print("0");
        Serial.print(datenMbus.plaintext[i], HEX);
      }
      Serial.println(" ");*/
      aktuelleDaten.year = ((datenMbus.plaintext[6] << 8) | datenMbus.plaintext[7]) - 2000;
      aktuelleDaten.month = datenMbus.plaintext[8];
      aktuelleDaten.day = datenMbus.plaintext[9];
      aktuelleDaten.hour = datenMbus.plaintext[11];
      aktuelleDaten.minutes = datenMbus.plaintext[12];
      aktuelleDaten.seconds = datenMbus.plaintext[13];
      aktuelleDaten.wirkenergiePlus=((unsigned long)datenMbus.plaintext[43]<<24)|((unsigned long)datenMbus.plaintext[44]<<16)|((unsigned long)datenMbus.plaintext[45]<<8)|(unsigned long)datenMbus.plaintext[46];
      aktuelleDaten.wirkenergieMinus=((unsigned long)datenMbus.plaintext[62]<<24)|((unsigned long)datenMbus.plaintext[63]<<16)|((unsigned long)datenMbus.plaintext[64]<<8)|(unsigned long)datenMbus.plaintext[65];
      aktuelleDaten.momentanleistungPlus=((unsigned long)datenMbus.plaintext[81]<<24)|((unsigned long)datenMbus.plaintext[82]<<16)|((unsigned long)datenMbus.plaintext[83]<<8)|(unsigned long)datenMbus.plaintext[84];
      aktuelleDaten.momentanleistungMinus=((unsigned long)datenMbus.plaintext[100]<<24)|((unsigned long)datenMbus.plaintext[101]<<16)|((unsigned long)datenMbus.plaintext[102]<<8)|(unsigned long)datenMbus.plaintext[103];
      aktuelleDaten.uL1=float((datenMbus.plaintext[119]<<8)|datenMbus.plaintext[120])/10.0;
      aktuelleDaten.uL2=float((datenMbus.plaintext[136]<<8)|datenMbus.plaintext[137])/10.0;
      aktuelleDaten.uL3=float((datenMbus.plaintext[153]<<8)|datenMbus.plaintext[154])/10.0;
      aktuelleDaten.iL1=float((datenMbus.plaintext[170]<<8)|datenMbus.plaintext[171])/100.0;
      aktuelleDaten.iL2=float((datenMbus.plaintext[187]<<8)|datenMbus.plaintext[188])/100.0;
      aktuelleDaten.iL3=float((datenMbus.plaintext[204]<<8)|datenMbus.plaintext[205])/100.0;
      aktuelleDaten.powerF=float((datenMbus.plaintext[221]<<8)|datenMbus.plaintext[222])/1000.0;
      Serial.print(aktuelleDaten.day);
      Serial.print(".");
      Serial.print(aktuelleDaten.month);
      Serial.print(".");
      Serial.print(aktuelleDaten.year);
      Serial.print("  ");
      Serial.print(aktuelleDaten.hour);
      Serial.print(":");
      Serial.print(aktuelleDaten.minutes);
      Serial.print(":");
      Serial.println(aktuelleDaten.seconds);
      Serial.print("A+: ");
      Serial.print(aktuelleDaten.wirkenergiePlus);
      Serial.print("Wh | A-: ");
      Serial.print(aktuelleDaten.wirkenergieMinus);
      Serial.println("Wh");
      Serial.print("P+: ");
      Serial.print(aktuelleDaten.momentanleistungPlus);
      Serial.print("W | P- (einsp.): ");
      Serial.print(aktuelleDaten.momentanleistungMinus);
      Serial.print("W  ");
      Serial.print("Saldo: ");
      Serial.print(aktuelleDaten.momentanleistungPlus-aktuelleDaten.momentanleistungMinus);
      Serial.println(" W");
      Serial.println("U1: " + String(aktuelleDaten.uL1) + "V  U2: " + String(aktuelleDaten.uL2)+ "V  U3: " + String(aktuelleDaten.uL3)+"V");
      Serial.println("I1: " + String(aktuelleDaten.iL1) + "A  I2: " + String(aktuelleDaten.iL2)+ "A  I3: " + String(aktuelleDaten.iL3)+"A");
      Serial.print("PowerFactor: ");
      Serial.println(aktuelleDaten.powerF);
      Serial.println("");
      for (int i = 0; i < MAX_PLAINTEXT_LEN; i++) {
        datenMbus.plaintext[i] = 0x00;
        datenMbus.ciphertext[i] = 0x00;
      }
      processData = false;
    }
  } else {
    processData = true;
  }
}

void decrypt_text(Vector_GCM &vect) {
  GCM<AES128> *gcmaes128 = 0;
  gcmaes128 = new GCM<AES128>();
  gcmaes128->setKey(vect.key, gcmaes128->keySize());
  gcmaes128->setIV(vect.iv, vect.ivsize);
  gcmaes128->decrypt(vect.plaintext, vect.ciphertext, vect.datasize);
  delete gcmaes128;
}
