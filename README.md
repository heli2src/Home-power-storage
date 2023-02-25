# Home-power-storage
AC coupled power storage for home supply.
This project was implemented in 2018. Since then it has been running without any problems. The cost of battery storage with charger and battery inverter was around 2500 €. Calculated over the year, the solar coverage is 85%. In the summer months 97% and in the foggy December 52%.
The self-consumption of the battery inverter and Arudiuno controller is <10 Watt.

Build with:
- 3.6 kWp Photovoltaic house roof system and solar inverter <img src="docs/images/roof.jpg" width="10%"></img> 
- Electricity meter reading head with USB-Interface <img src="docs/images/ir-kopf.png" width="5%"></img> 
  - reading the head using a Raspberry Pi (it was already there from the house automation) and the [libsml](src/rasperry_pi/libsml/examples/sma_mqtt.py). Sending the current reference line to an MQTT broker
- 14s/250Ah Battery from an accident car VW e-up! from 2014 <img src="docs/images/batterie-e-up.jpg" width="10%"></img> 
- Energus Tiny BMS <img src="docs/images/Energus-bms.png" width="10%"></img> 
- 500 Watt Solar Inverter from AEC, connect via RS485 with the Arduino <img src="docs/images/aec-inv500.png" width="10%"></img>
- 2000 Watt Eltek power supply, connect via CAN with the Arduino <img src="docs/images/eltek.png" width="10%"></img>
- Arduino Mega-2560 with CAN and RS485 for controlling <img src="docs/images/mega-2560.jpg" width="10%"></img>
- 3 Relais for power switching
<br><br><br>

<img src="docs/images/home_storage.png"></img>
