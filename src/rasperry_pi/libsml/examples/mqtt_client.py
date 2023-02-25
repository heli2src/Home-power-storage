# -*- coding: utf-8 -*-
"""
Created on Tue Feb 18 09:52:09 2020

@author: C. Jung


todo:
    change payload to json format and move the attribute in the topic to the payload
    e.q:    payload={
                    "voltage": 3.0,
                    "current": 0.3,
                    "v_range": 6.0
                    }
        think about get/set to move in to the payload
        oder ganz weglassen, und mit retain=True arbeiten,
            nach einem diconnect wid mqtt_connect=False und alle anderen attribute
            gelöscht mit clearattribute   --> dannn entfällt aber nur das get von extern
                                              das set extern wird dann aber immer noch gebraucht ?!


"""

import time
import sys
import inspect
import json

try:
    import paho.mqtt.client as mqttc                        # for more info, see https://github.com/eclipse/paho.mqtt.python
    has_mqtt = True
except Exception:
    has_mqtt = False

import socket
from pytestsharing.instruments.singleton import Singleton
# from pytestsharing.instruments.base_instrument import logger
# from uuid import getnode as get_mac


class mqtt_init(object, metaclass=Singleton):
    '''
        Methods:
            clearattribute(attr)
                remove retained publish message from broker
            publish
                send message to broker
            setup
                setup the mqtt client
            close()
                disconnect from broker

    '''

    def __init__(self):
        def on_connect(client, userdata, flags, rc):
            if rc == 0:
                print('MQTT Broker {}, user {} connected'.format(self.broker, self.username))
            elif rc == 5:
                print('MQTT Broker {}, user {} authentication error'.format(self.broker, self.username))
            else:
                print('MQTT Broker {},  user {} connection failed ({})'.format(self.broker, self.username, mqttc.connack_string(rc)))

        def on_disconnect(client, userdata,  rc):
            msg = ("MQTT Broker {} disconnected".format(self.broker))
            if rc > 0:
                msg += ', {}'.format(mqttc.connack_string(rc))
            print(msg)

        self.broker = ''
        self.username = ''
        self.qos = 0
        self.retain = False
        if not has_mqtt:
            return
        client = mqttc.Client(clean_session=True)
        client.enable_logger(logger=None)
        client.on_connect = on_connect
        client.on_disconnect = on_disconnect
        self.client = client

    def init(self, broker='172.27.112.62', port=1883, message_client=None, username='appslab', userpasswd='raspberry', qos=0, retain=False):
        '''
        message_client = None if client the device like e.q. smu, digital-multimeter or thermostreamer
                       = e.q. 'DT1604092/vdd' if gui, = hostname and device.instname from the device we want to connect
        '''
        import socket
        if not has_mqtt:
            msg = '\nmqtt not usable!! missing paho.mqtt\n'
            msg = msg+'   please install\n'
            msg = msg+'   see https://github.com/eclipse/paho.mqtt.python\n'
            print('Warning: '+msg)
            return
        self.qos = qos
        self.retain = retain
        self.username = username
        if broker.find('.') > 0:
            self.broker = broker
        else:
            self.broker = socket.gethostname(broker)
        self.client.username_pw_set(username, password=userpasswd)
        self.client.connect(self.broker, port, 60)         # blocking until connection establish or timeout
        self.client.loop_start()
        if message_client is None:
            message_client = socket.gethostname()
        self.client.subscribe(message_client+'/#', qos=qos)

    def publish(self, attr, value, qos=None, retain=None):
        if qos is None:
            qos = self.qos
        if retain is None:
            retain = self.retain
        # print("    publish {}={} ".format(attr, str(value)))
        value=json.dumps(value)
        self.client.publish("{}".format(attr), str(value), qos=qos, retain=retain)       # payload must be string,bytearray,int,float or None

    def clearattribute(self, attr):
        """Remove retained publish message from broker."""
        self.client.publish(attr, '', 0, True)             # send 0 message to remove retained flag

    def close(self):
        """Disconnect from broker."""
        if self.broker is not None:
            self.client.disconnect()
            self.client.loop_stop()


class mqtt_deviceattributes(object):
    '''
        mqqt messages for devices (='transmitter')
           After establish connection to broker, send attribute mqtt_connect=True (='hostname/instName/mqtt_connect True') to broker
           If new message for broker arrive, than should call _mqtt_message automatically
           If device closed, than send attribute mqtt_connect=False (='hostname/instName/mqtt_connect False') to broker

           format from the messages:
               if attribute called:
                   @setter -> publish 'hostname/instName/attribute/set value'
                   @getter -> publish 'hostname/instName/attribute/get value'
               if message arrived:
                  'hostname/instName/attribute/set value'    -> do nothing, it is the answer from @setter
                  'hostname/instName/attribute/get value'    -> do nothing, it is the answer from @getter
                  'hostname/instName/attribute/extern/set value' -> if in mqtt_list than set the attribute to value
                  'hostname/instName/attribute/extern/get None'  -> if in mqtt_list than call attribute.@getter
               missing: 'hostname/instName/attribute/extern/set None'  -> if in mqtt_list than publish value from @setter  !


        Methods:
            mqtt_add(client,instName)
                call from base_instrument, if create the device. Add the device(=client) to mqtt
            mqtt_disconnect()
                call from base_instrument, if close the device. Remove the device from mqtt

            _mqtt_message()
                call, if message received from broker
                if attribute in mqtt_list than call __setattr__ (if set) or __getattribute__(if get)
            __setattr__
                if attribute in mqtt_list, than publish the attribute and value  (=send 'hostname/instName/attibute/set value' to broker)
            __getattribute__
                if attribute in mqtt_list, than publish the attribute and value (=send 'hostname/instName/attibute/get value' to broker)


        Bugs:
            after exception, the attribute mqtt_connect see no update
                    -> the counterpart thinks everthing is ok, but it isn't


    '''
    import json
    mqtt_list = ''
    _mqttclient = None
    mqtt_enable = False
    mqtt_connect = False
    mqtt_debug = False

    def mqtt_add(self, client, instName, liste='#', qos=0):
        if not has_mqtt:
            return
        self._mqttclient = client
        self._mqttName = socket.gethostname()+'/'+instName
        if liste == '#':
            self.mqtt_list = self.mqtt_all
        else:
            self.mqtt_list = liste
        self.mqtt_list.append('mqtt_connect')
        self._mqttclient.client.will_set(self._mqttName+'/mqtt_connect', False, 1, True)                # last will: set mqtt_connect =False
        self._mqttclient.publish(self._mqttName+'/mqtt_connect', True, qos, True)
        # subscribe all messages from instName which comes from extern
        self._mqttclient.client.message_callback_add(self._mqttName+'/+/extern/#', self._mqtt_message)
        if self._mqttclient is not None:
            self.mqtt_enable = True
            self.mqtt_connect = True

    def mqtt_disconnect(self):
        if hasattr(self, '_mqttclient'):
            self._mqttclient.publish(self._mqttName+'/mqtt_connect', False, 1, True)
            self._mqttclient.client.message_callback_remove(self._mqttName+'/+/extern/#')   # remove subscribe
            self.mqtt_connect = False
            print('{} {} disconnected'.format(self.__class__.__name__, self.instName))

    def __setattr__(self, attr, value):
        object.__setattr__(self, attr, value)
        if self.mqtt_enable and attr in self.mqtt_list:
            topic = self._mqttName+'/{}/set'.format(attr)
            self._mqttclient.publish(topic, value)
            if self.mqtt_debug:
                print('{} {} publish: {} {}'.format(self.__class__.__name__, self.instName, topic, value))

    def __getattribute__(self, attr):
        value = super(__class__, self).__getattribute__(attr)
#       if attr!='mqtt_list' and attr!='_mqttclient' and hasattr(self,'mqtt_list') and self._mqttclient!=None and attr in self.mqtt_list:
        # if attr!='mqtt_list' and attr!='_mqttclient' and self._mqttclient!=None and attr in self.mqtt_list:
        if attr != '__class__' and attr != 'mqtt_list' and attr != 'INST_FUNCTION' and attr in self.mqtt_list:
            topic = self._mqttName+'/{}/get'.format(attr)
            self._mqttclient.publish(topic, value)
            if self.mqtt_debug:
                print('{} {} publish: {} {}'.format(self.__class__.__name__, self.instName, topic, value))
        return value

    def publish_get(self, function_name, value):
        # function_name=inspect.stack()[1][3]
        if self.mqtt_enable:
            topic = self._mqttName+'/{}/get'.format(function_name)
            if self.mqtt_debug:
                print('publish_get', topic, value)
            self._mqttclient.publish(topic, value)

    def publish_set(self, function_name, value):
        # function_name=inspect.stack()[1][3]
        if self.mqtt_enable:
            topic = '{}/{}/set'.format(self._mqttName, function_name)
            if self.mqtt_debug:
                print('publish_set', topic, value)
            self._mqttclient.publish(topic, value)

    def _mqtt_message(self, client, userdata, message):
        value = message.payload.decode('utf-8', 'ignore')
        # print('received mqtt message {}={}'.format(message.topic,message.payload))
        try:
            value = self.json.loads(str(value))
        except Exception:
            # print("failed:",type(value))
            pass
        attr = message.topic[len(self._mqttName)+1:]           # extract attribute without instName
        attr = attr.split('/')
        attribute = ''
        for a in attr[:-2]:
            attribute += '.'+a
        attribute = attribute[1:]
        if attr[-1] == 'set':
            if attribute in self.mqtt_list:
                print('set {}.{} = {}  from extern'.format(self._mqttName, attribute, value))
                self.__setattr__(attribute, value)
            elif self.mqtt_debug:
                print('set {}.{}  from extern not allowed'.format(self._mqttName, attribute))
        elif attr[-1] == 'get':
            if self.mqtt_enable and attribute in self.mqtt_list:
                print('get {}.{} = {} from extern'.format(self._mqttName, attribute, value))
                self.__getattribute__(attribute)
            elif self.mqtt_enable and self.mqtt_debug:
                print('get {}.{} from extern not allowed'.format(self._mqttName, attribute))
        else:
            print('unknown mqtt message {}, do nothing'.format(attribute, message.payload))
        sys.stdout.flush()


class mqtt_displayattributes(object):
    '''
        mqqt messages for display and controlling (='receiver')
           After establish connection to broker, get attribute mqtt_connect=True or False (=status counterpart)
           if mqtt_connect=True than publish 'hostname/instName/attribute/extern/get None' for each attribute in self.mqtt_list
           If new message for broker arrive, than call _mqtt_message automatically,
               if the received attribute in self.mqtt_list than
               attribute=(value,'get or set')
               for each attribute in the self.mqtt_list there have to be @setter/@getter in class above!

           format from the messages:
               if attribute called (have to be in the mqtt_list):
                   @setter -> publish 'hostname/instName/attribute/extern/set value'
                   @getter -> you should call publish_get
               if message arrived:
                  'hostname/instName/attribute/set value'       -> call attribute.setter with value=(value,set) (is a list !!)
                  'hostname/instName/attribute/get value'       -> call attribute.setter with value=(value,get)
                  'hostname/instName/attribute/extern/set None' -> do nothing, it`s the answer from setter
                  'hostname/instName/attribute/extern/get None' -> do nothing, it´s the answer from getter

        Methods:
            mqtt_add()

            mqtt_disconnect()

            publish_get()
               publish 'hostname/instName/attribute/extern/get None'
            publish_set(value)
               publish 'hostname/instName/attribute/extern/set Value'

            _mqtt_message
                automatically called, if message received from broker
                if received attribute in mqtt_list, than set attribute with (value,get or set)

        Properties:
            mqtt_connect
                True/False read message mqtt_connect from mqtt broker (status from counterpart)
                    if etablish connection than publish 'hostname/instName/attribute/extern/get None' for each attribute in self.mqtt_list
            instName

            mqtt_list
                list of attribute, have to be assigned in the class above, otherwise it is empty


        Known Bugs:
            sometime other mqtt_add(), attribute mqtt_connect didn't send from broker ??
    '''
    import json
    mqtt_debug = True
    mqtt_list = []
    _mqttclient = None

    def __init__(self, client, message_client):
        self.mqtt_connect = False
        self._mqttclient = client
        self.instName = message_client
        # self._mqtt_add()

    def __setattr__(self, attribute, value):
        # if  hasattr(self,'mqtt_list') and self._mqttclient!=None and attribute in self.mqtt_list:
        if self._mqttclient is not None and attribute in self.mqtt_list:
            if self.mqtt_debug:
                print('mqtt: send {}/{}/extern/set {}'.format(self.instName, attribute, value))
            self._mqttclient.client.publish('{}/{}/extern/set'.format(self.instName, attribute), value)
        else:
            object.__setattr__(self, attribute, value)

    def publish_get(self):
        function_name = inspect.stack()[1][3]
        topic = '{}/{}/extern/get'.format(self.instName, function_name)
        if self.mqtt_debug:
            print('publish_get', topic, 'None')
        self._mqttclient.publish(topic, 'None')

    def publish_set(self, value):
        function_name = inspect.stack()[1][3]
        topic = '{}/{}/extern/set'.format(self.instName, function_name)
        if self.mqtt_debug:
            print('publish_set', topic, value)
        self._mqttclient.publish(topic, value)

    def mqtt_add(self):
        self._mqttclient.client.message_callback_add(self.instName+'/#', self._mqtt_message)   # subscribe all messages for this client
        print('mqtt: add {} as client'.format(self.instName))

    def mqtt_disconnect(self):
        self._mqttclient.client.message_callback_remove(self.instName+'/#')         # remove subscribe for this client
        print('{}.mqtt_disconnect'.format(self.instName))

    def _mqtt_message(self, client, userdata, message):            # function for subscribe messages, get/set from device-client
        value = message.payload.decode('utf-8', 'ignore')
        # if self.mqtt_debug: print('{}.mqtt_message value/payload:  {} = {}'.format(self.__class__.__name__,message.topic,value))
        try:
            value = self.json.loads(str(value))
        except Exception:
            pass
        attr = message.topic[len(self.instName)+1:]               # attribute without instName
        attr = attr.split('/')
        if (len(attr) == 1 and attr[0] == 'mqtt_connect') or (len(attr) > 1 and attr[-2] == 'mqtt_connect'):
            if value == "True":
                print('{} mqtt_message mqtt_connect'.format(self.instName))        # if connection than get all values for initialisation
                for attribute in self.mqtt_list:
                    topic = '{}/{}/extern/get'.format(self.instName, attribute)
                    self._mqttclient.publish(topic, None)
                    if self.mqtt_debug:
                        print('{}.mqtt_message: publish {} = None'.format(self.__class__.__name__, topic))
            elif value == 'False':
                print('{} mqtt_message disconnected'.format(self.instName))
            else:
                print('{} mqtt_message unknown = {}'.format(self.instName, value))
            self.mqtt_connect = value
        elif len(attr) > 1:
            if attr[-2] == 'extern':
                return()                   # do nothing if received the own message
            atr_mod = ''
            for a in attr[:-1]:
                atr_mod += '.'+a
            if attr[-1] == 'set' or attr[-1] == 'get':
                func = attr[-1]
                attr = atr_mod[1:]
                if attr in self.mqtt_list and value != "":
                    if self.mqtt_debug:
                        print('{}.mqtt_message({}):  {}.{} = {}'.format(self.__class__.__name__, func, self.instName, attr, value))
                    object.__setattr__(self, attr, [value, func])   # only attribute have to be set (not publish this message again)
                elif self.mqtt_debug:
                    print('{}.mqtt_message({}): {}/{} = {} not found in mqtt_list '.format(self.__class__.__name__, func, self.instName, attr, value))
            else:
                if self.mqtt_debug:
                    print('unknown mqtt message {}'.format(attr, message.payload))


if __name__ == '__main__':
    mqtt = mqtt_init('172.27.112.62')                       # broker mosquitto on 'ubuntu-0001'
    i = 0
    while True:
        time.sleep(2)
        mqtt.client.publish("mqtt_client/temperature", "25")
        i += 1
        if i > 10:
            break
    mqtt.close()
