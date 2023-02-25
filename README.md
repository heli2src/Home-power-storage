# Home-power-storage
AC coupled power storage for home supply

Build with:
- 3.6 kWp Photovoltaic house roof system and solar inverter <img src="docs/images/roof.jpg" width="10%"></img> 
- Electricity meter reading head with USB-Interface <img src="docs/images/ir-kopf.png" width="5%"></img> 
  - reading the reading head using a Raspberry Pi (it was already there from the house automation) and the [libsm](src/rasperry_pi/libsml/examples/sma_mqtt.py), sending the current reference line to an MQTT broker
- 14s/250Ah Battery from an VW e-up! <img src="docs/images/batterie-e-up.jpg" width="10%"></img> 
- Energus Tiny BMS <img src="docs/images/Energus-bms.png" width="10%"></img> 
- 500 Watt Solar Inverter from AEC with RS485 <img src="docs/images/aec-inv500.png" width="10%"></img>
- 2000 Watt Eltek power supply <img src="docs/images/eltek.png" width="10%"></img>
- Arduino Mega-2560 with CAN and RS485 for controlling <img src="docs/images/mega-2560.jpg" width="10%"></img>
- 3 Relais


<img src="docs/images/home_storage.png"></img>
