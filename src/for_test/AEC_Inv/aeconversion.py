#!/usr/bin/python
import serial, time

_idwr = 123													# Die Geraeteadresse des Wechselrichters sind die letzten 5 Ziffern der Seriennummer

ser = serial.Serial(										# Initialieren der RS485 Schnittstelle
	port = "/dev/ttyUSB0",
	baudrate = 9600,
	bytesize = serial.EIGHTBITS,
	parity = serial.PARITY_NONE,
	stopbits = serial.STOPBITS_ONE,
	timeout = 1,
	xonxoff = False,
	rtscts = False,
	dsrdtr = False,
	writeTimeout = 2
)

# Funktion zur Berechnung des Steuercodes incl Checksumme
def c_crc(_message_hex, _idwr):
	_x = 0
	_idwr_hex = '{:04x}'.format(_idwr).decode('hex')		# idwr in hex umwandeln
	_m = _idwr_hex + _message_hex							# Code fuer crc-Berechnung
	
	l = len(_m)												# crc Pruefsumme berechnen
	for i in range(0,l):
		_x ^= ord(_m[i])
	
	_crc_hex = '{:02x}'.format(_x).decode('hex')			# crc Pruefsumme in hex umwandeln
	_idwr_hex = '{:04x}'.format(_idwr).decode('hex')
	_data_hex = "\x21" + _idwr_hex + _message_hex + _crc_hex + "\x0D"
	return _data_hex

def wr_read(_wr_message_hex):								# Abfrage des Wechselrichters
	_wr_error = ""
	try:
		ser.open()
	except Exception, e:
		_wr_error = "Der serielle Port konnte nicht geoeffnet werden:" + str(e)
		exit()
	if ser.isOpen():
		try:
			ser.flushInput() 								# Eingangspuffer leeren
			ser.flushOutput()								# Ausgangspuffer leeren
			ser.write(_wr_message_hex)
			time.sleep(0.5)  								# Pause vor Datenempfang
			_wr_response_hex = ser.readline().encode("hex")	# Antwort des Wechselrichters
			ser.close()
		except Exception, e1:
			_wr_error = "Kommunikationsfehler: " + str(e1)
			
	else:
		_wr_error = "Der com-Port konnte nicht geoeffnet werden"
	
	if _wr_error == "":
		return _wr_response_hex
	else:
		return _wr_error

# Liest Leistung und Ertrag des WR aus
def wr_ertrag():
	_message_hex = "\x03\xFD"							# Response 10007 Ertrag auslesen
	
	_wr_result = wr_read (c_crc(_message_hex, _idwr))
	
	_p_ac_m = round(int(_wr_result[6:14], 16) / 65536.0, 2)
	_ertrag = round(int(_wr_result[14:22], 16) / 65536.0, 0)
	
	return (_p_ac_m,_ertrag)

	
# Liest die Betriebsdaten des WR aus 	
def wr_betriebsdaten():
	_message_hex = "\x03\xED"							# Response 10002 Betriebsdaten auslesen
	
	_wr_result = wr_read (c_crc(_message_hex, _idwr))
	
	_u_s_ac = round(int(_wr_result[6:14], 16) / 65536.0, 2)
	_i_s_ac = round(int(_wr_result[14:22], 16) / 65536.0, 2)
	_i_pv = round(int(_wr_result[22:30], 16) / 65536.0, 2)
	_u_pv = round(int(_wr_result[30:38], 16) / 65536.0, 2)
	_p_ac = round(int(_wr_result[38:46], 16) / 65536.0, 2)
	_p_pv = round(int(_wr_result[46:54], 16) / 65536.0, 2)
	_temp = round(int(_wr_result[54:62], 16) / 65536.0, 2)	# gibt immer 50 zurueck
	_dc_ac = round(int(_wr_result[62:70], 16) / 65536.0, 2)	
	
	return (_u_s_ac, _i_s_ac, _i_pv, _u_pv, _p_ac, _p_pv, _temp, _dc_ac)


# Reduziert die Eingangsleistung (PV) auf den angegebenen Wert in Watt
def wr_leistungsreduzierung(_p_soll):
	_message_hex = "\x03\xFE"							# Response 10007 Ertrag auslesen

	_p = (_p_soll + 7) * 65536							# +7 = Korrektur Verluste
	_p_hex = '{:08x}'.format(_p).decode('hex')			# in hex umwandeln
	_message_hex = _message_hex + _p_hex
	_wr_result = wr_read (c_crc(_message_hex, _idwr))
	
	if _wr_result == '212710370d':
		return 'P soll: ' + str(_p_soll) + ' W'
	else:
		return 'error'


print 'Bsp: Betriebsdaten auslesen'
(_u_s_ac, _i_s_ac, _i_pv, _u_pv, _p_ac, _p_pv, temp, _dc_ac) = wr_betriebsdaten()

print 'Aktuelle PV-Spannung: ' + str(_u_pv)
print 'Aktueller PV-Strom: ' + str(_i_pv)
print 'Aktuelle PV-Leistung: ' + str(_i_pv)
print 'Aktuelle AC-Leistung: ' + str(_p_ac)

#Pause
time.sleep(5)

print 'Bsp: Leistung / Ertrag auslesen'

(_leistung, _tagesertrag) = wr_ertrag()	
print 'Aktuelle Leistung: ' + str(_leistung)
print 'Aktueller Tagesertrag: ' + str(_tagesertrag)	

#Pause
time.sleep(5)

print 'Bsp: Leistungsreduzierung auf 80 W'		

_result = wr_leistungsreduzierung(80)
print _result



# Zur Doku:
# Aufbau _wr_result am Beispiel Betriebsdaten
# Bsp: 21 2712 0147b9d8 0000911b 00035ec7 001cfa67 005b8fad 005efeef 00320000 ffff6dba 50 0d
# Umrechnung: Hex > Dez / 65536.0
# 21						Startzeichen
# 2712						MessageID 10002
# 0147b9d8					Scheitelwert AC-Spannung (A)
# 0000911b					Scheitelwert AC-Strom (A)
# 00035ec7		3 A			PV Strom (A)
# 001cfa67		28 V		PV Spannung (V)
# 005b8fad		91 W		AC Leistung WR Ausgang (W)
# 005efeef		94 W		PV Leistung (W)
# 00320000		50 Grad		Betriebstemperatur (Grad)
# ffff6dba					Gleichspannungsanteil auf AC-Strom (mA)
# 50						Checksumme
# 0D						Stoppzeichen