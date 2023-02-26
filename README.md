# EVN Kundenschnittstelle auslesen SMART METER

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



