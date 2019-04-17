import paho.mqtt.client as paho
import time
import sys
import re
import telegram
from telegram.error import NetworkError, Unauthorized
import logging


import config
# in config.py:
#MIFLO_CONFIG = {
#    'broker': '0.0.0.0',
#    'topic_commands': '',
#    'topic_status': '',
#    'telegram_token': ''
#}


client= paho.Client("milfo_broker")
logging.basicConfig(format='%(asctime)s - %(name)s - %(levelname)s - %(message)s', level=logging.DEBUG)


update_id = None

def send_time():
  year = time.strftime("%Y", time.localtime())
  month = time.strftime("%m", time.localtime())
  day = time.strftime("%d", time.localtime())
  hour = time.strftime("%H", time.localtime())
  min = time.strftime("%M", time.localtime())
  sec = time.strftime("%S", time.localtime())

  json = "{{'type':'settime', 'year':{}, 'month':'{}', 'day':'{}', 'hour':{}, 'minute':'{}', 'second':'{}'}}".format(year, month, day, hour, min, sec)
  print(json)

  client.publish(config.MIFLO_CONFIG['topic_commands'],json)#publish

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
  elif (re.match(r'[tT]odo ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+)', message)):
    values = re.findall(r'[tT]odo ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+)', message)[0]
    return "{{'type':'todo', 'job':['{}','{}','{}','{}']}}".format(values[0], values[1], values[2], values[3]), "Sending instructions for a todo list"
  elif (re.match(r'[tT]odo ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+)', message)):
    values = re.findall(r'[tT]odo ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+)', message)[0]
    return "{{'type':'todo', 'job':['{}','{}','{}','']}}".format(values[0], values[1], values[2]), "Sending instructions for a todo list"
  elif (re.match(r'[tT]odo ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+)', message)):
    values = re.findall(r'[tT]odo ([a-zA-Z0-9_]+) ([a-zA-Z0-9_]+)', message)[0]
    return "{{'type':'todo', 'job':['{}','{}','','']}}".format(values[0], values[1]), "fdfsSending instructions for a todo list"
  elif (re.match(r'[tT]odo ([a-zA-Z0-9_]+)', message)):
    values = re.findall(r'[tT]odo ([a-zA-Z0-9_]+)', message)
    return "{{'type':'todo', 'job':['{}','','','']}}".format(values[0]), "Sending instructions for a todo list"
  elif (re.match(r'[rR]eminder (.+)', message)):
    values = re.findall(r'[rR]eminder (.+)', message)
    return "{{'type':'reminder', 'message': '{}'}}".format(values[0]), "Reminder verstuurd"
  elif (re.match(r'[aA]larm (.+)', message)):
    values = re.findall(r'[aA]larm (.+)', message)
    return "{{'type':'alarm', 'message': '{}'}}".format(values[0]), "Alarm verstuurd"
  elif (re.match(r'[pP]luspunt', message)):
    return "{'type':'plus'}", "Pluspunt"
  elif (re.match(r'[mM]inpunt', message)):
    return "{'type':'min'}", "Minpunt"
  elif (re.match(r'[lL]og', message)):
    return "{'type':'log'}", "Log"
  else:
    return "", "Ik heb je commando niet goed begrepen"

def query_telegram(bot):
  global update_id
  logging.debug ("query_telegram")
  for update in bot.get_updates(offset=update_id, timeout=10):
        update_id = update.update_id + 1

        if update.message:  # your bot can receive updates without messages
            logging.debug  ("incomming telegram %s", update.message.text)
            
            if (re.match(r'[mM]attijs (.+)', update.message.text)):
              values = re.findall(r'[mM]attijs (.+)', update.message.text)[0]
              json, bot_message = parse_command(values)

              logging.debug(json)
              logging.debug(bot_message)

              if json != "":
                ret = client.publish( "/iot/miflo/mattijs/timer", json )
                logging.debug("Sending %s to mqtt: %s", json, ret)

              if bot_message != "":
                update.message.reply_text(bot_message)
            else:
              update.message.reply_text("Die naam ken ik niet ... Gebruik bijvoorbeeld 'minne timer 15'")


def main(argv): 
  global update_id

  bot = telegram.Bot(config.MIFLO_CONFIG['telegram_token'])
  # get the first pending update_id, this is so we can skip over it in case
  # we get an "Unauthorized" exception.
  try:
      update_id = bot.get_updates()[0].update_id
  except IndexError:
      update_id = None


  ######Bind function to callback
  client.on_message=on_message
  #####
  logging.info("connecting to broker %s", config.MIFLO_CONFIG['broker'])
  client.connect(config.MIFLO_CONFIG['broker'])#connect
  client.loop_start() #start loop to process received messages
  logging.debug("subscribing ")
  client.subscribe(config.MIFLO_CONFIG['topic_status'])#subscribe

  while(1):
    try:
      query_telegram(bot)
    except NetworkError:
      time.sleep(1)
    except Unauthorized:
      logging.warning("Telegram Unauthorized")
       # The user has removed or blocked the bot.
      update_id += 1

  client.disconnect() #disconnect
  client.loop_stop() #stop loop

if __name__ == "__main__":
  main(sys.argv[1:])
