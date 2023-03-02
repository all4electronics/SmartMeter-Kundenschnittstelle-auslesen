// Programm zur Auslesung und Entschlüsselung der Kundenschnittstelle eines SmartMeters der EVN (NÖ)
// Bibliothek: https://github.com/rweather/arduinolibs/tree/master/libraries/Crypto
// Aus dem Ordner "libraries" die Bibliothek "Crypto" installieren!
// Prg. funktioniert für einen Atmega2560 (Arduino Mega), Man könnte aber auch die Bibliothek SoftwareSerial benutzen um
// das Programm auf einem anderen Mikrocontroller zu realisieren.

#include <Crypto.h>
#include <AES.h>
#include <GCM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <avr/wdt.h>

#define MAX_PLAINTEXT_LEN 300
#define BUTTON_UP 7
#define BUTTON_DOWN 5
#define BUTTON_MODE 6

int byteNumber = 0;
unsigned long timeSinceLastData = 0;
bool processData = false;

bool uebersichtOderVerbrauch = true;
byte scroll = 0;
unsigned long refreshScreen = 0;
unsigned long backlightTime = 0;

unsigned long tagesVerbrauch[4][31];
bool saveOnStartUp = true;
const unsigned long saveOnStartUpTime=20000;

LiquidCrystal_I2C lcd(0x3F, 20, 4);   // Richtige I2C Adresse EINSETZEN!


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
  int momentanleistungSaldiert = 0;
};

IncommingData aktuelleDaten;

Vector_GCM datenMbus = {
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
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  for (int i = 0; i < MAX_PLAINTEXT_LEN; i++) {
    datenMbus.plaintext[i] = 0x00;
    datenMbus.ciphertext[i] = 0x00;
  }
  Serial.begin(115200);
  Serial1.begin(2400);
  lcd.begin();
  lcd.backlight();
  readDataFromEeprom();
wdt_enable(WDTO_4S);
/*
  Serial.println("");
  for (int daten = 0; daten < 4; daten++) {
    for (int i = 0; i < 31; i++) {
      Serial.print(tagesVerbrauch[daten][i]);
      Serial.print("\t");
    }
  }
  
  for (int daten = 0; daten < 4; daten++) {
    for (int i = 0; i < 31; i++) {
      tagesVerbrauch[daten][i] = 0;
    }
  }
  writeDataToEeprom();*/
}

void loop() {
  if (millis() > backlightTime) {
    lcd.noBacklight();
  }
  if (aktuelleDaten.hour == 6||(millis()>saveOnStartUpTime&&saveOnStartUp)) {
    if (tagesVerbrauch[0][0] != aktuelleDaten.day) {
      for (int i = 30; i > 0; i--) {
        for (int daten = 0; daten < 4; daten++) {
          tagesVerbrauch[daten][i] = tagesVerbrauch[daten][i - 1];
          //Serial.println(i);
        }
      }
      tagesVerbrauch[0][0] = aktuelleDaten.day;
      tagesVerbrauch[1][0] = aktuelleDaten.month;
      tagesVerbrauch[2][0] = aktuelleDaten.wirkenergiePlus;
      tagesVerbrauch[3][0] = aktuelleDaten.wirkenergieMinus;
     
      writeDataToEeprom();
     
      //Serial.println("Write SUCCESFUL");
      }
    saveOnStartUp=false;
  }
  if (millis() > refreshScreen) {
    if (uebersichtOderVerbrauch) {
      lcd.clear();
      lcd.setCursor(0, 0);
      if (aktuelleDaten.hour < 10)lcd.print("0");
      lcd.print(aktuelleDaten.hour);
      lcd.print(":");
      if (aktuelleDaten.minutes < 10)lcd.print("0");
      lcd.print(aktuelleDaten.minutes);
      lcd.print(":");
      if (aktuelleDaten.seconds < 10)lcd.print("0");
      lcd.print(aktuelleDaten.seconds);
      if (aktuelleDaten.momentanleistungSaldiert < 0)
        lcd.print("          PV");
      else lcd.print("        NETZ");
      lcd.setCursor(9, 0);
      lcd.print(abs(aktuelleDaten.momentanleistungSaldiert));
      lcd.print("W");
      switch (scroll) {
        case 0:
          lcd.setCursor(0, 1);
          lcd.print("P+: ");
          lcd.print(aktuelleDaten.momentanleistungPlus);
          lcd.print("W");
          lcd.setCursor(10, 1);
          lcd.print("P-: ");
          lcd.print(aktuelleDaten.momentanleistungMinus);
          lcd.print("W");
          lcd.setCursor(0, 2);
          lcd.print("Verbr.: ");
          lcd.print(float(aktuelleDaten.wirkenergiePlus) / 1000);
          lcd.print("kWh");
          lcd.setCursor(0, 3);
          lcd.print("Einsp.: ");
          lcd.print(float(aktuelleDaten.wirkenergieMinus) / 1000);
          lcd.print("kWh");
          break;
        case 1:
          lcd.setCursor(0, 1);
          lcd.print(" |  L1 |  L2 |  L3 |");
          lcd.setCursor(0, 2);
          lcd.print("U|");
          lcd.print(aktuelleDaten.uL1, 1);
          lcd.print("|");
          lcd.print(aktuelleDaten.uL2, 1);
          lcd.print("|");
          lcd.print(aktuelleDaten.uL3, 1);
          lcd.print("|");
          lcd.setCursor(0, 3);
          if (aktuelleDaten.iL1 < 10)lcd.print("I| ");
          else lcd.print("I| ");
          lcd.print(aktuelleDaten.iL1, 2);
          if (aktuelleDaten.iL2 < 10)lcd.print("| ");
          else lcd.print("|");
          lcd.print(aktuelleDaten.iL2, 2);
          if (aktuelleDaten.iL3 < 10)lcd.print("| ");
          else lcd.print("|");
          lcd.print(aktuelleDaten.iL3, 2);
          lcd.print("|");
          break;
      }
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Datum|A+  Wh|A-  Wh|");
      for (int i = 0; i < 3; i++) {
        lcd.setCursor(0, i + 1);
        if (tagesVerbrauch[0][scroll + i] < 10)lcd.print("0");
        lcd.print(tagesVerbrauch[0][scroll + i]);
        lcd.print(".");
        if (tagesVerbrauch[1][scroll + i] < 10)lcd.print("0");
        lcd.print(tagesVerbrauch[1][scroll + i]);
        lcd.print("|");
        lcd.setCursor(6, i + 1);
        if (tagesVerbrauch[2][scroll + i] - tagesVerbrauch[2][scroll + 1 + i] < 100000)lcd.setCursor(7, i + 1);
        if (tagesVerbrauch[2][scroll + i] - tagesVerbrauch[2][scroll + 1 + i] < 10000)lcd.setCursor(8, i + 1);
        if (tagesVerbrauch[2][scroll + i] - tagesVerbrauch[2][scroll + 1 + i] < 1000)lcd.setCursor(9, i + 1);
        if (tagesVerbrauch[2][scroll + i] - tagesVerbrauch[2][scroll + 1 + i] < 100)lcd.setCursor(10, i + 1);
        if (tagesVerbrauch[2][scroll + i] - tagesVerbrauch[2][scroll + 1 + i] < 10)lcd.setCursor(11, i + 1);
        lcd.print(tagesVerbrauch[2][scroll + i] - tagesVerbrauch[2][scroll + 1 + i]);
        lcd.print("|");
        lcd.setCursor(13, i + 1);
        if (tagesVerbrauch[3][scroll + i] - tagesVerbrauch[3][scroll + 1 + i] < 100000)lcd.setCursor(14, i + 1);
        if (tagesVerbrauch[3][scroll + i] - tagesVerbrauch[3][scroll + 1 + i] < 10000)lcd.setCursor(15, i + 1);
        if (tagesVerbrauch[3][scroll + i] - tagesVerbrauch[3][scroll + 1 + i] < 1000)lcd.setCursor(16, i + 1);
        if (tagesVerbrauch[3][scroll + i] - tagesVerbrauch[3][scroll + 1 + i] < 100)lcd.setCursor(17, i + 1);
        if (tagesVerbrauch[3][scroll + i] - tagesVerbrauch[3][scroll + 1 + i] < 10)lcd.setCursor(18, i + 1);
        lcd.print(tagesVerbrauch[3][scroll + i] - tagesVerbrauch[3][scroll + 1 + i]);
        lcd.print("|");
      }
    }
    refreshScreen = millis() + 10000;
  }
  if (uebersichtOderVerbrauch) {
    if (digitalRead(BUTTON_UP) == LOW) {
      if (scroll > 0)scroll--;
      delay(10);
      refreshScreen = millis();
      backlightTime = millis() + 10000;
      lcd.backlight();
      while (digitalRead(BUTTON_UP) == LOW) {}
      delay(5);
    }
    if (digitalRead(BUTTON_DOWN) == LOW) {
      if (scroll < 1)scroll++;
      delay(10);
      refreshScreen = millis();
      backlightTime = millis() + 10000;
      lcd.backlight();
      while (digitalRead(BUTTON_DOWN) == LOW) {}
      delay(5);
    }
    if (digitalRead(BUTTON_MODE) == LOW) {
      if (millis() < backlightTime) {
        uebersichtOderVerbrauch = false;
        scroll = 0;
        delay(10);
        refreshScreen = millis();
      }
      backlightTime = millis() + 10000;
      lcd.backlight();
      while (digitalRead(BUTTON_MODE) == LOW) {}
      delay(5);
    }
  } else {
    if (digitalRead(BUTTON_UP) == LOW) {
      if (scroll > 0)scroll--;
      delay(10);
      refreshScreen = millis();
      backlightTime = millis() + 10000;
      lcd.backlight();
      while (digitalRead(BUTTON_UP) == LOW) {}
      delay(5);
    }
    if (digitalRead(BUTTON_DOWN) == LOW) {
      if (scroll < 26)scroll++;
      delay(10);
      refreshScreen = millis();
      backlightTime = millis() + 10000;
      lcd.backlight();
      while (digitalRead(BUTTON_DOWN) == LOW) {}
      delay(5);
    }
    if (digitalRead(BUTTON_MODE) == LOW) {
      if (millis() < backlightTime) {
        uebersichtOderVerbrauch = true;
        scroll = 0;
        delay(10);
        refreshScreen = millis();
      }
      backlightTime = millis() + 10000;
      lcd.backlight();
      while (digitalRead(BUTTON_MODE) == LOW) {}
      delay(5);
    }
  }

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
      for (int i = 256; i < MAX_PLAINTEXT_LEN; i++) {
        datenMbus.ciphertext[i] = 0x00;
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
      if (aktuelleDaten.hour < 24 && aktuelleDaten.minutes < 60 && aktuelleDaten.seconds < 60) {   // Nur falls kein Datenmüll eingelesen wird Daten weiterverarbeiten
        aktuelleDaten.wirkenergiePlus = ((unsigned long)datenMbus.plaintext[43] << 24) | ((unsigned long)datenMbus.plaintext[44] << 16) | ((unsigned long)datenMbus.plaintext[45] << 8) | (unsigned long)datenMbus.plaintext[46];
        aktuelleDaten.wirkenergieMinus = ((unsigned long)datenMbus.plaintext[62] << 24) | ((unsigned long)datenMbus.plaintext[63] << 16) | ((unsigned long)datenMbus.plaintext[64] << 8) | (unsigned long)datenMbus.plaintext[65];
        aktuelleDaten.momentanleistungPlus = ((unsigned long)datenMbus.plaintext[81] << 24) | ((unsigned long)datenMbus.plaintext[82] << 16) | ((unsigned long)datenMbus.plaintext[83] << 8) | (unsigned long)datenMbus.plaintext[84];
        aktuelleDaten.momentanleistungMinus = ((unsigned long)datenMbus.plaintext[100] << 24) | ((unsigned long)datenMbus.plaintext[101] << 16) | ((unsigned long)datenMbus.plaintext[102] << 8) | (unsigned long)datenMbus.plaintext[103];
        aktuelleDaten.uL1 = float((datenMbus.plaintext[119] << 8) | datenMbus.plaintext[120]) / 10.0;
        aktuelleDaten.uL2 = float((datenMbus.plaintext[136] << 8) | datenMbus.plaintext[137]) / 10.0;
        aktuelleDaten.uL3 = float((datenMbus.plaintext[153] << 8) | datenMbus.plaintext[154]) / 10.0;
        aktuelleDaten.iL1 = float((datenMbus.plaintext[170] << 8) | datenMbus.plaintext[171]) / 100.0;
        aktuelleDaten.iL2 = float((datenMbus.plaintext[187] << 8) | datenMbus.plaintext[188]) / 100.0;
        aktuelleDaten.iL3 = float((datenMbus.plaintext[204] << 8) | datenMbus.plaintext[205]) / 100.0;
        aktuelleDaten.powerF = float((datenMbus.plaintext[221] << 8) | datenMbus.plaintext[222]) / 1000.0;
        aktuelleDaten.momentanleistungSaldiert = int(aktuelleDaten.momentanleistungPlus) - int(aktuelleDaten.momentanleistungMinus); //Ein Int reicht völlig aus, da sicher nicht mehr als 32kW bezogen werden
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
        Serial.print(aktuelleDaten.momentanleistungSaldiert);
        Serial.println(" W");
        Serial.println("U1: " + String(aktuelleDaten.uL1) + "V  U2: " + String(aktuelleDaten.uL2) + "V  U3: " + String(aktuelleDaten.uL3) + "V");
        Serial.println("I1: " + String(aktuelleDaten.iL1) + "A  I2: " + String(aktuelleDaten.iL2) + "A  I3: " + String(aktuelleDaten.iL3) + "A");
        Serial.print("PowerFactor: ");
        Serial.println(aktuelleDaten.powerF);
        Serial.println("");
        refreshScreen = millis();
      }
      for (int i = 0; i < MAX_PLAINTEXT_LEN; i++) {
        datenMbus.plaintext[i] = 0x00;
        datenMbus.ciphertext[i] = 0x00;
      }
      processData = false;
    }
  } else {
    processData = true;
  }
  wdt_reset();
}

void decrypt_text(Vector_GCM &vect) {
  GCM<AES128> *gcmaes128 = 0;
  gcmaes128 = new GCM<AES128>();
  gcmaes128->setKey(vect.key, gcmaes128->keySize());
  gcmaes128->setIV(vect.iv, vect.ivsize);
  gcmaes128->decrypt(vect.plaintext, vect.ciphertext, vect.datasize);
  delete gcmaes128;
}

void writeDataToEeprom(){
  for (int y = 0; y < 31; y++) {
    for (int x = 0; x < 4; x++) {
      EEPROM.write(y * 16 + x * 4, byte(((unsigned long)tagesVerbrauch[x][y] >> 24) & (unsigned long)0xFF));
      EEPROM.write(y * 16 + x * 4 + 1, byte(((unsigned long)tagesVerbrauch[x][y] >> 16) & (unsigned long)0xFF));
      EEPROM.write(y * 16 + x * 4 + 2, byte(((unsigned long)tagesVerbrauch[x][y] >> 8) & (unsigned long)0xFF));
      EEPROM.write(y * 16 + x * 4 + 3, byte((unsigned long)tagesVerbrauch[x][y] & (unsigned long)0xFF));
    }
  }
}

void readDataFromEeprom(){
  byte readIn = 0;
  for (int y = 0; y < 31; y++) {
    for (int x = 0; x < 4; x++) {
      tagesVerbrauch[x][y] = 0;
      for (int i = 0; i < 4; i++) {
        readIn = EEPROM.read(y * 16 + x * 4 + i);
        tagesVerbrauch[x][y] = ((unsigned long)tagesVerbrauch[x][y] << 8) | (unsigned long)readIn;
      }
    }
  }
}
