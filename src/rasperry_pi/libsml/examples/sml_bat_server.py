'''
read from arduino log and save it to logfile


todo:



done:
    read on UDP:
      - rx: '1-0:16.7.0'         - key for  export power
        tx: '12:59:49|1566.0*W'  - send time and power

      - rx: 'Bat:'               - sending Battery load/discharge data
                                 - PAC;dayload;monthload;yearsload;sumload;'
                                 - 130;0,1;50;423;4330;0,0;132;787;15173
    - create bat_d_day.dat	               bat discharge, gesamtverbrauch eines Tages:
		24.08.2019, 360
		25.08.2019, 345
    - create bat_c.dat                     bat charge, gesamt laden eines Tages


'''

import socket
import os
import time
import datetime
import shutil

sml_file='/mnt/RAMDisk/sml_obis.txt'
dicharge_filename='bat_d_day.dat'
charge_filename='bat_c_day.dat'
bat_path_ram='/mnt/RAMDisk/'
bat_path_log='/home/smarthome/battery/'

def newbatfile():
    filedate=datetime.datetime.today()
    filename='battery_'+filedate.strftime('%Y%m%d')+'.log'
    if not os.path.exists(bat_path_ram+filename):
        with open(bat_path_ram+filename,'w') as f: f.write('Time;Solar;PAC;Bat_target;ae_status;ae_Pnetz;ae_Pbat;ae_Vbat;ae_Ibat;ae_temp;ae_Wh;el_status;el_temp;el_pout;el_tavout;el_vout;el_taiout;iout;el_Wh;\n')
    return(filename,filedate)

sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )   #UDP
#sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)	#Stream
home='192.168.178.'                  # only internal clients are allowed
port=14550
sock.bind( ('',port) )
bat_filename,bat_filedate=newbatfile()
print ('sml_server.py started')

ae_wh_reset=False
ae_wh_sum=0
ae_wh_old=0

el_wh_reset=False
el_wh_sum=0.0
el_wh_old=0.0

while True:
    data, addr = sock.recvfrom( 1024 )                              # wait for new receive

    #print (data,addr)
    if addr[0].find(home)==0:
        if 	data.find('1-0:')==0 and len(data)>4:                   # identified key for export power
            channel=data[:data.find('*')]
            ltime=time.ctime(os.path.getmtime(sml_file))[11:-5]		# get file date
            with open(sml_file) as f:
                for line in f:
                    if line.find(channel)==0:
                        txdat='{}|{}'.format(ltime,line[line.find('(')+1:line.find(')')])		#00:16:04|194.3*W
                        sock.sendto(txdat,addr)                     # and send date and power to client
                        #print (data,addr,txdat)
        elif data.find('Bat:')==0 and len(data)>4:                  # identified key for logging
            now=datetime.datetime.today()
            if bat_filedate.date()<now.date():											#new day starts
                shutil.move(bat_path_ram+bat_filename, bat_path_log+bat_filename)
                
                with open(bat_path_log+dicharge_filename,"a")	as fd:                  # save day discharging energy to file
                    fd.write(bat_filedate.strftime('%Y%m%d')+', '+str(ae_wh+ae_wh_sum)+'\n')
                ae_wh_sum=-ae_wh
                
                with open(bat_path_log+charge_filename,"a")	as fd:                      # save day charging energy to file
                    fd.write(bat_filedate.strftime('%Y%m%d')+', '+str(el_wh+el_wh_sum)+'\n')
                el_wh_sum=-el_wh
                bat_filename,bat_filedate=newbatfile()
                
            data=data[data.find(':')+1:]        # remove key
            dayload=data.split(';')[-2]
            data=now.strftime('%H:%M:%S')+';'+ data+'\n'    # add receive time
            #print('{}'.format(data))
            #print ('dayload {}'.format(dayload))
            with open(bat_path_ram+bat_filename,'a') as f:
                f.write(data)

            #search for discharging energy
            ae_wh=data[data.find('ae_Wh')+6:]                  
            ae_wh=int(ae_wh[:ae_wh.find(';')])              #extract discharging energy
            if  ae_wh==0:
                ae_wh_reset=True
            elif ae_wh_reset and ae_wh>0 and ae_wh<ae_wh_old:
                ae_wh_sum=ae_wh_old+ae_wh_sum				#whole discharging energy
                ae_wh_reset=False
            #elif ae_wh_reset and ae_wh=0:
            #    ae_wh_sum=0
            #    ae_wh_reset=False
            else:
                ae_wh_old=ae_wh
                ae_wh_reset=False


            #search for charging energy
            el_wh=data[data.find('el_Wh')+6:]
            el_wh=float(el_wh[:el_wh.find(';')])            #extract charging energy
            if  el_wh==0.0:
                el_wh_sum=el_wh_old+el_wh_sum				#whole charging energ
                el_wh_old=el_wh
            else:
                el_wh_old=el_wh

            #print ('0x{:04x}, {}'.format(power,power))

# test with echo -n 1-0:16.7.0* | nc -u 192.168.178.28 14550
