import paho.mqtt.client as paho
import time
import sys
import re


broker="192.168.0.10"
topic_commands="/iot/miflo/mattijs/timer"
topic_status="/iot/miflo/mattijs/status"
client= paho.Client("milfo_broker")

def send_time():
  year = time.strftime("%Y", time.localtime())
  month = time.strftime("%m", time.localtime())
  day = time.strftime("%d", time.localtime())
  hour = time.strftime("%H", time.localtime())
  min = time.strftime("%M", time.localtime())
  sec = time.strftime("%S", time.localtime())

  json = "{{'type':'settime', 'year':{}, 'month':'{}', 'day':'{}', 'hour':{}, 'minute':'{}', 'second':'{}'}}".format(year, month, day, hour, min, sec)
  print(json)

  client.publish(topic_commands,json)#publish

def on_message(client, userdata, message):
    time.sleep(1)
    msg = str(message.payload.decode("utf-8"))
    print("received message =", msg)

    if (msg == "booted"):
        send_time()

def parse_command( message ):
  if (re.match(r'[rR]eset', message)):
    return "{'type':'reset'}", "Resetting"
  elif (re.match(r'[aA]udio', message)):
    return "{'type':'audio'}", "Playing audio"
  elif (re.match(r'[tT]ime (\d+):(\d+)', message)):
    values = re.findall(r'[tT]ime (\d+):(\d+)', message)[0]
    return "{{'type':'settime', 'hour':{}, 'minute':'{}'}}".format(values[0], values[1]), "Instructie om klok te zetten verstuurd"
  elif (re.match(r'[tT]imer (\d+) (.+)', message)):
    values = re.findall(r'[tT]imer (\d+) (.+)', message)[0]
    return "{{'type':'timetimer', 'minutes':{}, 'job':'{}'}}".format(values[0], values[1]), "Time-timer instructies verstuurd"
  elif (re.match(r'[tT]imer (\d+)', message)):
    values = re.findall(r'[tT]imer (\d+)', message)[0]
    return "{{'type':'timetimer', 'minutes':{}, 'job':''}}".format(values[0]), "Sending instructions to start a time-timer"
  elif (re.match(r'[tT]odo ([a-z]+) ([a-z]+) ([a-z]+) ([a-z]+)', message)):
    values = re.findall(r'[tT]odo ([a-z]+) ([a-z]+) ([a-z]+) ([a-z]+)', message)[0]
    return "{{'type':'todo', 'job':['{}','{}','{}','{}']}}".format(values[0], values[1], values[2], values[3]), "Sending instructions for a todo list"
  elif (re.match(r'[tT]odo ([a-z]+) ([a-z]+) ([a-z]+)', message)):
    values = re.findall(r'[tT]odo ([a-z]+) ([a-z]+) ([a-z]+)', message)[0]
    return "{{'type':'todo', 'job':['{}','{}','{}','']}}".format(values[0], values[1], values[2]), "Sending instructions for a todo list"
  elif (re.match(r'[tT]odo ([a-z]+) ([a-z]+)', message)):
    values = re.findall(r'[tT]odo ([a-z]+) ([a-z]+)', message)[0]
    return "{{'type':'todo', 'job':['{}','{}','','']}}".format(values[0], values[1]), "Sending instructions for a todo list"
  elif (re.match(r'[tT]odo ([a-z]+)', message)):
    values = re.findall(r'[tT]odo ([a-z]+)', message)[0]
    return "{{'type':'todo', 'job':['{}','','','']}}".format(values[0]), "Sending instructions for a todo list"
  elif (re.match(r'[rR]eminder (.+)', message)):
    values = re.findall(r'[rR]eminder (.+)', message)[0]
    return "{{'type':'reminder', 'message': '{}'}}".format(values[0]), "Reminder verstuurd"
  elif (re.match(r'[aA]larm (.+)', message)):
    values = re.findall(r'[aA]larm (.+)', message)[0]
    return "{{'type':'alarm', 'message': '{}'}}".format(values[0]), "Alarm verstuurd"
  elif (re.match(r'[pP]luspunt', message)):
    return "{'type':'plus'}", "Pluspunt"
  elif (re.match(r'[mM]inpunt', message)):
    return "{'type':'min'}", "Minpunt"
  elif (re.match(r'[lL]og', message)):
    return "{'type':'log'}", "Log"
  else:
    return "", "Ik heb je commando niet goed begrepen"

def main(argv): 
  ######Bind function to callback
  client.on_message=on_message
  #####
  print("connecting to broker ", broker)
  client.connect(broker)#connect
  client.loop_start() #start loop to process received messages
  print("subscribing ")
  client.subscribe(topic_status)#subscribe

  while(1):
    time.sleep(2)
  

  client.disconnect() #disconnect
  client.loop_stop() #stop loop

if __name__ == "__main__":
  main(sys.argv[1:])
