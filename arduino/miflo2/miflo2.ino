#include <ESP8266WiFi.h>
#include <vector>

#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <EEPROM.h>
#include <SPI.h>
#include <GD2.h>
#include "RTClib.h"
RTC_DS1307 rtc;
#include <map>

#include "settings.h"

#define COLOR_BEIGE 0xffe9ac
#define COLOR_GREEN 0x588751
#define COLOR_RED 0xd41942
#define COLOR_ORANGE 0xff6f25
#define COLOR_YELLOW 0xffc700
#define COLOR_BLUE 0x2b19d4
#define COLOR_BLACK 0x030302
#define COLOR_WHITE 0xffe9e6

#define GOED_AANTAL 16
#define LAST_IMAGE_ID 15
  
#define JINGLE_PLUS       0
#define JINGLE_TIMERFINISHED  0
#define JINGLE_TIMETIMER  1

enum State { SLEEP_STATE, CLOCK_STATE, TIME_TIMER_STATE, FINISHED_STATE, REMINDER_STATE, ALARM_STATE, TODO_STATE, GOED_GEDAAN_STATE, UPCOMING_STATE, SHOW_LOG_STATE };
static char* goed[GOED_AANTAL] = { "fantastisch", "buitengewoon", "geweldig", "fenomenaal", "super", "wonderlijk", "uitstekend", "eersteklas", "opperbest", "excellent", "briljant", "grandioos", "machtig", "sensationeel", "volmaakt", "uitzonderlijk" };


std::vector< String > log_history;
WiFiClient espClient;
PubSubClient client(espClient);
State state = SHOW_LOG_STATE;
long time_timer = 0;
bool gotanswerafterboot = false;
std::map< int, String > cache;
int points = 1;
int todo_jpgs[ 4 ] = { -1, -1, -1, -1 };
String todo_texts[ 4 ] = { "", "", "", "" };
bool todo_done[ 4 ] = { false, false, false, false };
char current_job_string[200] = "";
int current_job_img = -1;
bool play_sample_next = false;
bool play_sample_next_next = false; // dirty, dirty hack but I must move fast
int goed1, goed2;
long next_alarm_bleep = 0;


void parse_command( char* json );
void mqttCallback(char* topic, byte* payload, unsigned int length); 
void mqttOnlineCheck();
String format_time( int hour, int minute, int second );
char* string2char(String s);
char* int2char(int n) ;
void add_log( String message );
void show_log();

void statusbar( int hour, int minute, int second );

/// SOUND
void jingle( int n = 0 ) {
  digitalWrite(15, HIGH);
  if ( n == 0 ) {
    GD.play( MUSICBOX, 60 );
    delay(250);
    GD.play( MUSICBOX, 60 );
    delay(500);
    GD.play( MUTE );
  } else if ( n == 1 ) {
    GD.play( MUSICBOX, 60 );
    delay(250);
    GD.play( MUSICBOX, 60 );
    delay(250);
    GD.play( MUSICBOX, 60 );
    delay(500);
    GD.play( MUTE );
  } else if ( n == 2 ) {
    GD.play( PIANO, 55 );
    delay(250);
    GD.play( PIANO, 64 );
    delay(250);
    GD.play( PIANO, 60 );
    delay(500);
    GD.play( MUTE );
  }
  digitalWrite(15, LOW);
}


/// COMMANDS


void parse_command( char* json ) {
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  const char* type = root["type"];

  Serial.print("Parse :");
  Serial.println(type);

  if ( strcmp( type, "settime" ) == 0 ) {
    add_log("Setting the clock");
    int year = root["year"];
    int month = root["month"];
    int day = root["daynd"];
    int hour = root["hour"];
    int minute = root["minute"];
    int second = root["second"];
    //String timestring = format_time( hour, minute, 0 );
    rtc.adjust(DateTime(year, month, day, hour, minute, second));

    //add_log(timestring);

    if (!gotanswerafterboot) gotanswerafterboot = true;
  } else if ( strcmp( type, "sleep" ) == 0 ) {
    add_log("Go to sleep");
    state = SLEEP_STATE;
  } else if ( strcmp( type, "wakeup" ) == 0 ) {
    add_log("waking up");
    gotanswerafterboot = false;
    state = CLOCK_STATE;
  } else if ( strcmp( type, "plus" ) == 0 ) {
    add_log("Running plus");
    points++;
    jingle(JINGLE_PLUS);
  } else if ( strcmp( type, "min" ) == 0 ) {
    add_log("Running min");
    points--;
  } else if ( strcmp( type, "todo" ) == 0 ) {
    add_log("Running todo");
    todo_texts[ 0 ] = (const char*)root["job"][0];
    todo_jpgs[ 0 ] = int(root["img"][0]);
    todo_texts[ 1 ] = (const char*)root["job"][1];
    todo_jpgs[ 1 ] = int(root["img"][1]);
    todo_texts[ 2 ] = (const char*)root["job"][2];
    todo_jpgs[ 2 ] = int(root["img"][2]);
    todo_texts[ 3 ] = (const char*)root["job"][3];
    todo_jpgs[ 3 ] = int(root["img"][3]);
    todo_done[ 0 ] = ( todo_texts[ 0 ] == "" );
    todo_done[ 1 ] = ( todo_texts[ 1 ] == "" );
    todo_done[ 2 ] = ( todo_texts[ 2 ] == "" );
    todo_done[ 3 ] = ( todo_texts[ 3 ] == "" );
    state = TODO_STATE;
  } else if ( strcmp( type, "timetimer" ) == 0 ) {
    add_log("Running timetimer");
    int minutes = root["minutes"];
    Serial.print( "minutes: " );
    Serial.println( minutes );
    const char* job = root["job"];
    strcpy(current_job_string, job);
    current_job_img = int(root["img"]);
    Serial.print("img");
    Serial.println(current_job_img);
    add_log( job );
    DateTime now = rtc.now();
    time_timer = now.unixtime() + minutes * 60;
    state = TIME_TIMER_STATE;
    jingle(JINGLE_TIMETIMER);
  } else if ( strcmp( type, "reminder" ) == 0 ) {
    add_log("Running reminder");
    const char* job = root["message"];
    strcpy(current_job_string, job);
    state = REMINDER_STATE;
  } else if ( strcmp( type, "alarm" ) == 0 ) {
    add_log("Running alarm");
    const char* job = root["message"];
    strcpy(current_job_string, job);
    state = ALARM_STATE;
  }
}

/// LOGGING

String format_time( int hour, int minute, int second ) {
  return String( hour < 10 ? "0" : "" ) + hour + ( minute < 10 ? ":0" : ":" ) + minute + ( second < 10 ? ":0" : ":" ) + second;
}

char* string2char(String s) {
  if (s.length() != 0) {
    char *p = const_cast<char*>(s.c_str());
    return p;
  }
}

char* int2char(int n) {
  char s[16];
  itoa(n, s, 10);
  return s;
}

void add_log( String message ) {
  Serial.println( message );
  log_history.push_back( message );
  while ( log_history.size() > 16 ) {
    log_history.erase( log_history.begin() );
  }
  if ( state == SHOW_LOG_STATE ) {
    GD.ClearColorRGB(COLOR_BLACK);
    GD.Clear();
    GD.ColorRGB(COLOR_BEIGE);
    show_log();
    GD.swap();
  }
}

void show_log() {
  int x = 0;
  for ( std::vector< String >::iterator i = log_history.begin(); i != log_history.end(); i++ ) {
    GD.cmd_text(5, 25 + x * 14, 20, 0, string2char(*i));
    x++;
  }
}


/// MQTT

void mqttOnlineCheck() {
  static long lastMQTTReconnectAttempt = 0;
  
  if (!client.connected()) {
    long now = millis();
    if (now - lastMQTTReconnectAttempt > 5000) {
      lastMQTTReconnectAttempt = now;
      client.setServer(mqttServer, mqttPort);
      client.setCallback(mqttCallback);

      add_log("Connecting to MQTT ...");

      if (client.connect(mqttClient)) {
        add_log("Connected to MQTT");
        bool ret = client.subscribe(mqttTopic);
        Serial.print("Subscribing :");
        Serial.println(ret);
      } else {
        add_log("MQTT connect failed");
        // add_log(client.state());
      }
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  add_log("Incoming MQTT message on topic:");
  add_log(topic);
  add_log("MQTT message:");
  char json[ length ];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    json[i] = (char)payload[i];
  }
  add_log( json );
  parse_command(json);
}

///SDCARD

#define ASSETS_END 886464UL
void load_jpgs()
{
  Serial.println("loadjpgs");
  for (int i = 0; i <= LAST_IMAGE_ID ; i++)
  {
    Serial.print("JPG ");
    Serial.println(i);
    GD.BitmapHandle(i);
    if (i == 0)
      GD.cmd_loadimage(0, 0);
    else
      GD.cmd_loadimage(-1, 0);

    char* file = "000.jpg";

    file[0] = char(48 + (i / 100));
    file[1] = char(48 + ((i % 100) / 10));
    file[2] = char(48 + (i % 10));

    Serial.println(file);
    
   
    byte ret = GD.load(file);

    Serial.print("ret ");
    Serial.println(ret);
  }
}


/// GRAPHICS
void statusbar( int hour, int minute, int second ) {

  GD.ColorA(128);

  // update WiFi status
  //if ( WiFi.status() == WL_CONNECTED ) GD.ColorRGB(COLOR_GREEN); else GD.ColorRGB(COLOR_RED);
  //GD.cmd_text(5, 5, 20, 0, "Wifi");
  // update MQTT status
  
  if ( client.connected() ) {
    GD.ColorRGB(COLOR_GREEN); 
    GD.cmd_text(5, 5, 20, 0, "Online");
  } else {
    GD.ColorRGB(COLOR_RED);
    GD.cmd_text(5, 5, 20, 0, "Offline");
  }
    
  GD.ColorRGB(COLOR_BEIGE);
  GD.cmd_text(GD.w / 2, 12, 20, OPT_CENTER, person);

  String time = format_time( hour, minute, second );
  GD.ColorRGB(COLOR_BEIGE);
  GD.cmd_text(GD.w - 65, 5, 20, 0, string2char(time));
  GD.ColorA(255);
}

String format_time_space( int hour, int minute, int second ) {
  return String( hour < 10 ? "0" : "" ) + hour + ( minute < 10 ? " : 0" : " : " ) + minute + ( second < 10 ? " : 0" : " : " ) + second;
}

String current_time() {
  DateTime now = rtc.now();
  return format_time_space  ( now.hour(), now.minute(), now.second() );
}

void show_clock() {
  static int animation_counter = 0;
  
  GD.Begin(POINTS);
  GD.ColorA(64);
  GD.ColorRGB(COLOR_BEIGE);
  for ( int i = 0; i < points; i++ ) {
    GD.PointSize(16 * sqrt( i + 1 ) * 5 );
    GD.Vertex2ii(
      100 * ( cos(animation_counter / float(100 + i * 73 % 101)) + cos(animation_counter / float(100 + i * 101 % 73)) ) + GD.w / 2,
      40 * ( sin(animation_counter / float(100 + i * 103 % 89)) + cos(animation_counter / float(100 + i * 89 % 103)) ) + GD.h / 2
    );
  }
  animation_counter++;

  String time = current_time();
  GD.ColorRGB(COLOR_BEIGE);
  GD.ColorA(128);
  GD.cmd_text(GD.w / 2, GD.h / 2, 31, OPT_CENTER, string2char(time));
  GD.ColorA(255);

  GD.get_inputs();
  switch (GD.inputs.tag) {
    case 210:
      state = UPCOMING_STATE;
      break;
  }

  if ( cache.size() > 0 ) {
    GD.ColorRGB(COLOR_BEIGE);
    GD.cmd_fgcolor(COLOR_BLACK);
    GD.ColorA(128);
    GD.Tag(210);
    GD.cmd_button( 20, GD.h - 40, 20, 20, 20, OPT_FLAT, int2char(cache.size()) );
    GD.ColorA(255);
  }
}

static void clock_ray(int centerx, int centery, int &x, int &y, int r, double i)
{
  uint16_t th = 0x8000 + 65536UL * ( i / 60.0 );
  GD.polar(x, y, r, th);
  x += 16 * centerx;
  y += 16 * centery;
}

void show_time_timer( double minutes, int centerx, int centery, int scale ) {
  int x, y;

  GD.Begin(POINTS);
  GD.ColorRGB(COLOR_BLACK);

  // time-timer labels
  /*const char *labels[] = {"0","55","50","45","40","35","30","25","20","15","10","5"};
    int align[12] = { OPT_CENTERX, 0, 0, 0, OPT_RIGHTX, OPT_RIGHTX, OPT_RIGHTX, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 12; i++) {
    clock_ray(centerx, centery, x, y, 16 * scale, i*5);
    GD.cmd_text(x >> 4, y >> 4, 26, OPT_CENTER, labels[i]);
    }*/
  clock_ray(centerx, centery, x, y, 16 * scale * 1.1, -minutes);
  char minute_str[16];
  itoa((int)(minutes + 0.99), minute_str, 10);
  GD.cmd_text(x >> 4, y >> 4, 28, OPT_CENTER, minute_str);

  // clockface
  GD.LineWidth(8);
  GD.Begin(LINES);
  for (int i = 0; i < 60; i++) {
    if ( i % 5 == 0 ) {
      clock_ray(centerx, centery, x, y, 16 * scale * 0.9, i);
      GD.Vertex2f(x, y);
      clock_ray(centerx, centery, x, y, 16 * scale * 0.8, i);
      GD.Vertex2f(x, y);
    } else {
      clock_ray(centerx, centery, x, y, 16 * scale * 0.9, i);
      GD.Vertex2f(x, y);
      clock_ray(centerx, centery, x, y, 16 * scale * 0.85, i);
      GD.Vertex2f(x, y);
    }
  }

  // time-timer
  if ( minutes > 0 ) {
    Poly po;
    po.begin();
    po.v(centerx * 16, centery * 16);
    clock_ray(centerx, centery, x, y, 16 * scale * 0.7, 60 - minutes);
    po.v(x, y);
    clock_ray(centerx, centery, x, y, 16 * scale * 0.7, (int)( 60 - minutes + 1 ));
    po.v(x, y);
    GD.ColorRGB(COLOR_RED);
    po.draw();
    for (int i = 60 - minutes + 1; i < 60; i++) {
      Poly po;
      po.begin();
      po.v(centerx * 16, centery * 16);
      clock_ray(centerx, centery, x, y, 16 * scale * 0.7, i + 1.1);
      po.v(x, y);
      clock_ray(centerx, centery, x, y, 16 * scale * 0.7, i - 0.1);
      po.v(x, y);
      GD.ColorRGB(COLOR_RED);
      po.draw();
    }
  }

}

void display_time_timer( double minutes ) {
  int jpg = current_job_img;
  if (strcmp(current_job_string, "") == 0) 
  {
    show_time_timer( minutes, GD.w / 2 - 10, GD.h / 2, 100 ); 
  } else {
    GD.Begin(BITMAPS);
    GD.ColorRGB(COLOR_BEIGE);
    GD.Vertex2ii(GD.w / 2 + GD.w / 4 - 50, GD.h / 2 - 50, jpg );
    GD.ColorRGB(COLOR_BLACK);
    GD.cmd_text(GD.w / 2 + GD.w / 4, GD.h / 2 + 60, 28, OPT_CENTER, current_job_string);
    show_time_timer( minutes, GD.w / 2 - GD.w / 4 + 50, GD.h / 2, 100 );
  }
}

void run_time_timer() {
  DateTime now = rtc.now();
  double minutes = (time_timer - now.unixtime()) / 60.0;
  display_time_timer( minutes );
  if ( now.unixtime() >= time_timer ) {
    state = FINISHED_STATE;
  }
}

void show_timer_finished() {
  DateTime now = rtc.now();

  if ( now.unixtime() >= next_alarm_bleep ) {
    next_alarm_bleep = now.unixtime() + 2;
    jingle(JINGLE_TIMERFINISHED);
  }

  GD.get_inputs();
  switch (GD.inputs.tag) {
    case 201:
      strcpy(current_job_string, "");
      points++;
      goed1 = rand() % 16;
      goed2 = rand() % 16;
      state = GOED_GEDAAN_STATE;
      delay(200);
      break;
  }

  GD.ColorRGB(COLOR_BLACK);
  GD.cmd_fgcolor(COLOR_GREEN);
  GD.Tag(201);
  GD.cmd_button( GD.w - 90, GD.h - 60, 80, 50, 27, OPT_FLAT, "OK" );

  display_time_timer( 0 );
  //  show_time_timer( 0, GD.w / 2, GD.h / 2, 100 );

}

void show_goed_gedaan() {

  GD.get_inputs();
  switch (GD.inputs.tag) {
    case 201:
      play_sample_next = true;
      state = CLOCK_STATE;
    break;
  }

  GD.ColorRGB(COLOR_BEIGE);
  GD.cmd_text(GD.w / 2, GD.h / 2 - 80, 28, OPT_CENTER, "Dat deed je");
  int r = rand() % GOED_AANTAL;
  GD.cmd_text(GD.w / 2, GD.h / 2 - 25, 31, OPT_CENTER, goed[ goed1 ]);
  GD.cmd_text(GD.w / 2, GD.h / 2 + 25, 31, OPT_CENTER, goed[ goed2 ]);

  GD.ColorRGB(COLOR_BLACK);
  GD.cmd_fgcolor(COLOR_BEIGE);
  GD.Tag(201);
  GD.cmd_button( GD.w / 2 - 200, GD.h - 60, 400, 50, 27, OPT_FLAT, "Graag gedaan!" );
}

void show_todo_icon( int x, int y, int job, int tag ) {
  if( todo_texts[ job ] != "" ) {
    GD.Begin(BITMAPS);
    GD.ColorRGB(COLOR_BEIGE);
    if ( todo_done[ job ] ) GD.ColorA(32); else GD.ColorA(255);
    GD.Tag(tag);
    if( todo_jpgs[ job ] != -1 )
      GD.Vertex2ii(x, y, todo_jpgs[ job ] );
    else
      GD.Vertex2ii(x, y, 0 );
    GD.ColorRGB(COLOR_BLACK);
    GD.cmd_text(x + 50, y + 110, 28, OPT_CENTER, string2char( todo_texts[ job ] ));
    if ( todo_done[ job ] ) {
      GD.Begin(LINE_STRIP);
      GD.ColorA(255);
      GD.LineWidth(16 * 10);
      GD.ColorRGB(COLOR_GREEN);
      GD.Vertex2ii(x + 20, y + 50);
      GD.Vertex2ii(x + 50, y + 80);
      GD.Vertex2ii(x + 80, y + 10);
    }
    GD.ColorA( 255 );
  }
}

void show_todo_icons() {
  show_todo_icon( 93, 10, 0, 100 );
  show_todo_icon( 287, 10, 1, 101 );
  show_todo_icon( 93, 140, 2, 102 );
  show_todo_icon( 287, 140, 3, 103 );

  GD.get_inputs();
  if ( GD.inputs.tag > 0 ) {
    if( !todo_done[ GD.inputs.tag - 100 ] )
      play_sample_next = true;
    todo_done[ GD.inputs.tag - 100 ] = not todo_done[ GD.inputs.tag - 100 ];
    delay(300);
  }

  if ( todo_done[0] && todo_done[1] && todo_done[2] && todo_done[3] ) {
    points += 4;
    goed1 = rand() % GOED_AANTAL;
    goed2 = rand() % GOED_AANTAL;
    state = GOED_GEDAAN_STATE;
  }
}

void show_todo() {
  DateTime now = rtc.now();

  if ( now.unixtime() >= next_alarm_bleep ) {
    next_alarm_bleep = now.unixtime() + 30;
    jingle(2);
  }

  show_todo_icons( );
}


/// SETUP

void setup() {
  // configure serial for debugging
  Serial.begin(115200);

  // set audio enable pin
  pinMode( 15, OUTPUT );

  Serial.println("Starting gameduino ...");
  GD.begin(GD_STORAGE);
  //GD.begin();
  GD.play( MUTE );
  GD.cmd_setrotate(0);
  GD.cmd_regwrite(REG_VOL_PB, 255);
  GD.cmd_regwrite(REG_VOL_SOUND, 255);

  add_log("Gameduino started");

  add_log("Loading JPGs ...");
  load_jpgs();
  add_log("JPGs loaded");

  WiFi.begin( ssid, password );
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    add_log("Connecting to Wifi ...");
  }
  add_log("Connected to WiFi network:");
  add_log(WiFi.SSID());

  mqttOnlineCheck();

  add_log("Starting RTC ...");
  if (! rtc.begin()) {
    add_log("Couldn't find RTC");
    while (1);
  } else {
    add_log("RTC started");
  }

  // set clock to compile time
  // only uncomment if you want to set this date
  // rtc.adjust(DateTime(__DATE__, __TIME__));

  add_log( "MiFlo has booted successfully!" );

  state = CLOCK_STATE;
}

void loop() {

  
  DateTime now = rtc.now();
  double minutes = (time_timer - now.unixtime()) / 60.0;
  int current_h = now.hour();
  int current_m = now.minute();
  int current_s = now.second();

  // GRAPHICS
  // clear screen
  uint32_t bgcolor = COLOR_BEIGE;
  switch (state) {
    case CLOCK_STATE:
    case SLEEP_STATE:
    case SHOW_LOG_STATE:
      bgcolor = COLOR_BLACK;
      break;
    case REMINDER_STATE:
      bgcolor = COLOR_YELLOW;
      break;
    case ALARM_STATE:
      bgcolor = COLOR_RED;
      break;
    case GOED_GEDAAN_STATE:
      bgcolor = COLOR_GREEN;
      break;
  }
  GD.ClearColorRGB(bgcolor);
  GD.Clear();

  // always show the statusbar
  if (state != SLEEP_STATE)
    statusbar( current_h, current_m, current_s );

  // dependent on state, show contents
  switch (state) {
    case FINISHED_STATE:
      show_timer_finished();
      break;
    case REMINDER_STATE:
      //show_reminder( current_job_string );
      break;
    case ALARM_STATE:
      //show_alarm( current_job_string );
      break;
    case TIME_TIMER_STATE:
      run_time_timer();
      break;
    case UPCOMING_STATE:
      //show_upcoming_events();
        break;
    case SHOW_LOG_STATE:
      GD.ColorRGB(COLOR_BEIGE);
      show_log();
      break;
    case CLOCK_STATE:
      int cache_time;
      cache_time = current_h * 10000 + current_m * 100 + current_s;
      if ( cache.count( cache_time ) ) {
        parse_command(string2char(cache[cache_time]));
        cache.erase(cache_time);
      }
      show_clock();
      if( current_h == 1 ) points = 1;
      break;
    case TODO_STATE:
      show_todo();
      break;
    case GOED_GEDAAN_STATE:
      show_goed_gedaan();
      break;
  }

  // bring the contents to the front
  GD.swap();

  // COMMUNICATION
  // make sure we're still connected to MQTT
  mqttOnlineCheck();
  client.loop();
  // keep generating random numbers to mess with the seed
  rand();

  if (!gotanswerafterboot) {
      client.publish(mqttTopic_status, "booted", false);
      delay(1000);
  }

  if (state == SLEEP_STATE) {
    delay(1000);
  }
}
