# EVN Kundenschnittstelle auslesen (SMART METER)
ENGLISH TRANSLATION AT THE BOTTOM!

Mit dieser Anleitung möchte ich zeigen, wie man Daten von den SmartMetern, die in ganz Niederösterreich installiert wurden, auf die EINFACHSTE und wahrscheinlich GÜNSTIGSTE ART UND WEISE auslesen kann.

Zunächst sollte man wissen, dass die Stromversorgungsunternehmen in ganz Österreich viele verschiedene Stromzähler von verschiedenen Herstellern und mit unterschiedlichen Schnittstellen installiert haben. Deshalb gilt diese Anleitung nur für den Stromzähler T210-D von SAGEMCOM, der eine M-Bus-Schnittstelle hat.
Unser Anbieter (EVN) bietet einige Informationen darüber, wie man Daten von dem SmartMeter lesen kann:
https://www.netz-noe.at/Download-(1)/Smart-Meter/218_9_SmartMeter_Kundenschnittstelle_lektoriert_14.aspx
Jeder der hierbei auf eine Schritt für Schritt Anleitung gehofft hat, welche zeigt wie man das Smart Meter mit einem RPi oder Arduino ausließt, wird aber enttäuscht.
Deshalb hier mein Lösungsvorschlag:

Zunächst sollte man den Schlüssel zum Entschlüsseln der Daten von der EVN beantragen. Wie in dem oben genannten Dokument angegeben, müssen Sie nur eine E-Mail mit Ihrer Kundennummer, Zählernummer und Telefonnummer an smartmeter@netz-noe.at senden.

Der M-Bus Anschluss in form einer RJ12 Buchse befindet sich hinter der grünen Klappe an der Front des Zählers. Das SmartMeter sendet Daten alle 5 Sekunden, indem es die Spannung zwischen beiden M-Bus-Leitungen von 36 V (1) auf 24 V (0) mit einer Baudrate von 2400 ändert.
Daher ist ein Pegelwandler notwendig der 36V->5V und 24V->0V umwandelt. Danach kann der Arduino die Daten über UART einlesen. In meinem Fall musste ich nur den Ausgang des Pegelwandlers an Pin 19 (RX1) meines Atmega2560 anschließen. Mit Serial.read() konnte ich dann die verschlüsselten Daten vom Zähler einlesen.
Bei Verwendung eines anderen Microncontrollers können die Daten auch per Software mit der Bibliothek SoftwareSerial eingelesen werden.   

--Pegelwandler--

Circuit_Mbus.pdf
Um einen Verpolungsschutz hinzuzufügen, habe ich einen Brückengleichrichter am Eingang hinzugefügt.
K1A erzeugt eine stabile Referenzspannung für den Schmitt-Trigger, der mit K1B realisiert wurde. Die Referenzspannung sollte mit dem Trimmer R7 auf etwa 2,5 V eingestellt werden. Durch Ändern der Referenzspannung können Sie die Schaltschwelle nach oben und unten verschieben. Die Hysterese ist auf etwa 1 V eingestellt. Sie können die Werte gerne neu berechnen oder eine bessere Lösung für den Pegelwandler finden. Für mich funktioniert die Schaltung einwandfrei, nachdem ich sie auf die Schaltpunkte 27V und 28V eingestellt habe.

--Entschlüsselung--

Ich habe eine wunderbare Beschreibung gefunden, wie die AES128-GCM-Verschlüsselung funktioniert.
https://www.weigu.lu/tutorials/sensors2bus/04_encryption/index.html
Mit folgender Bibliothek und einem Teil des oben genannten Programms konnte ich die Daten erfolgreich entschlüsseln:
https://github.com/rweather/arduinolibs/tree/master/libraries/Crypto (Sie müssen NUR die "Crypto" -Bibliothek im "Library" Ordner installieren).

Die Programmdatei EVN_SmartMeter beinhaltet nur die Einlesung, Entschlüsselung und Ausgabe der Daten am Seriellen Monitor.
Die Programmdatei EVN_SmartMeter_ArduinoMega_LCD umfasst zudem eine Ausgabe der aktuellen Daten auf einem 20x4 LCD Display (mit I2C Schnittstelle) und eine Ausgabe der jeweiligen Energieeinspeisung und  Energiebezug vom Netz (Wh) bis zu ein Monat zurück (im EEPROM gespeichert). 


This tutorial shows you how to read data from the SmartMeters, which have been installed all over lower Austria, in the SIMPLEST and probably CHEAPEST POSSIBLE WAY.

First off, one must know that the electrical companies all over Austria installed many different power meters from different manufactureres and with different interfaces. Therefore this guide only applies to the power meter T210-D from SAGEMCOM which has a M-Bus interface. 
Our provider (EVN) offers some informations about how to read data from the SmartMeter:
https://www.netz-noe.at/Download-(1)/Smart-Meter/218_9_SmartMeter_Kundenschnittstelle_lektoriert_14.aspx

You should first start by requesting the key to decipher the data. As the bevore mentioned document states, you only have to send them an email with your "Kundennummer", "Zählernummer" and telephone number. 

In order to connect to the M-Bus you'll need a RJ12 cable. 
The SmartMeter sends data every 5 seconds by changing the voltage between both M-Bus-lines from 36V (1) to 24V (0) with a baud rate of 2400.
All you need is a level converter which converts 36V->5V and 24V->0V. After that, the arduino is able to read the data over UART. In the case of my Atmega2560, I only had to connect the output of the level shifter to pin 19 (RX1). Using Serial.read() I was then able to read the encrypted data from the meter. 


--LEVEL CONVERTER-- 

Circuit_Mbus.pdf
In order to add reverse polarity protection I added a full bridge rectivier at the input.
K1A creates stable reference voltage for the schmitttrigger built with K1B. The reference voltage should be tuned to around 2,5V using the trimmer R7. By changing the reference voltage, you can shift the switching threshold up and down, the hysteresis is set to around 1V. Feel free to recalculate it or even find a better solution for the level converter. For me the circuit works perfectly after i tuned it to have its switching points at roughly 27V and 28V. 

--Decyphering--

I found a wonderful description about how the AES128-GCM Encryption works.
https://www.weigu.lu/tutorials/sensors2bus/04_encryption/index.html
Using the following library and part of the bevore mentioned code I was able to successfully decipher the data:
https://github.com/rweather/arduinolibs/tree/master/libraries/Crypto  (you ONLY need to install the "Crypto" library in the library folder)



