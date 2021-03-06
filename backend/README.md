# MiFLo backend

These two tools send commands to the MiFlo. One is a Google Calendar script that sends the 5 upcoming events on a calendar to the MiFlo, to be cached and executed when planned. The second is a Telegram bot that listens to commands over Telegram and forwards them to the MiFLo.

## Calendar

Follow ruby calendar quickstart at [https://developers.google.com/calendar/quickstart/ruby] (https://developers.google.com/calendar/quickstart/ruby) to obtain credentials in the form of a `client_secret.json` file so you can run the example:

`ruby calendar.rb -c CALENDAR_ID -p PERSON_NAME -h MQTT_BROKER`

## Telegram bot

Get a Telegram bot up and running, acquire the token and run the MiFlo bot:

`ruby telegram_bot.rb -h MQTT_BROKER -t TELEGRAM_BOT_TOKEN -p PERSON_NAME`


## Example installation rapsberry pi

The backend can run on any server, but here is a small example on how to do it on a raspberry pi:

* Install raspbian [https://www.raspberrypi.org/downloads/raspbian/](https://www.raspberrypi.org/downloads/raspbian/)
* Using `sudo raspi-config` you can adapt some basic settings (e.g. enable ssh, change hostname, update, ...), do not forget to correct the timezone
* `sudo apt-get update`
* install git: `sudo apt-get install git`
* clone the repo `git clone https://github.com/TeamScheire/MiFlo.git`
* `cd MiFlo/backend`
* install ruby: 
 ```
 sudo apt-get install ruby
 sudo gem install bundler
 bundle install
 ```
* install mosquitto broker:
 ```
 sudo wget https://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
 sudo apt-key add mosquitto-repo.gpg.key
 cd /etc/apt/sources.list.d/
 sudo wget http://repo.mosquitto.org/debian/mosquitto-stretch.list
 sudo apt-get update
 sudo apt-get install mosquitto mosquitto-clients
 ```

* get the calender up and running
 * Follow ruby calendar quickstart at [https://developers.google.com/calendar/quickstart/ruby] (https://developers.google.com/calendar/quickstart/ruby) to obtain credentials in the form of a `client_secret.json`
 * copy/rename the `client_secret.json` to the backend folder, e.g. using `scp`
 * test the calender script `ruby calendar.rb -c CALENDAR_ID -p PERSON_NAME -h localhost` 
  where CALENDER_ID can be found in the settings of the google calender
 * add it to crontab `crontab -e`
 * e.g. every 5 minutes: `*/5 * * * * script -c "cd /home/pi/MiFlo/backend/; ruby calendar.rb -c CALENDAR_ID -p PERSON_NAME -h localhost" /home/pi/MiFlo/backend/calendar.log`
* get the telegram bot up and running
 * create a new bot: `https://core.telegram.org/bots#6-botfather` 
 * test the script `ruby telegram_bot.rb -h localhost -t TELEGRAM_BOT_TOKEN -p PERSON_NAME
 * start on startup: `sudo nano /etc/rc.local`
 * add line `ruby /home/pi/MiFlo/backend/telegram_bot.rb -h localhost -t TELEGRAM_BOT_TOKEN -p PERSON_NAME & > /home/pi/MiFlo/backend/telegram_bot.log  2>&1`
