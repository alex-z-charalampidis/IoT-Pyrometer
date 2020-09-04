/*NOTES:

RPC: ONLY CAN BE CALLED WHEN THE SYSTEM ISN'T
IN A LOOP or via MQTT
Mosquitto setup on server complete, no ssl 
*/

/*MQTT wakes up the device every mqtt.keep_alive seconds */
#include <math.h>
#include "mgos_app.h"

#include "frozen/frozen.h"

#include "mgos_system.h"

#include "mgos_timers.h"

#include "mgos_i2c.h"

#include "mgos_mqtt.h"

#include "mgos.h"

#include "mgos_gpio.h"

#include "mgos_wifi.h"

#include "mgos_rpc.h"

#include "mgos_adc.h"

#include "mongoose.h"

#include "rom/rtc.h"

#include "driver/rtc_io.h"

#include <driver/dac.h>

#include "esp_sleep.h"

#include <stdbool.h>

#include "common/cs_dbg.h"

#include "common/mg_str.h"

#include "common/queue.h"

#include "frozen.h"

#include "mgos_debug.h"

#include "mgos_event.h"

#include "mgos_mongoose.h"

#include "mgos_net.h"

#include "mgos_sys_config.h"

#include "mgos_utils.h"

#include <stddef.h>

#include <stdlib.h>

#include <string.h>

#include <stdio.h>

#include "common/platform.h"

#include "common/cs_file.h"

#include "common/json_utils.h"

#include "mgos_hal.h"

#include <time.h>

#include "mgos_dlsym.h"
 //#include "driver/i2s.h"
#include "freertos/queue.h"

#include <Arduino.h>

#include "Wire.h"
#include "esp_system.h"

#include "Adafruit_SSD1306.h"

#include "mgos_ads1x1x.h"

#include "mgos_arduino_pololu_VL53L0X.h"

/***************--------------DEFINITIONS------------*********************/
//Button the user presses to measure temperature. Needs to be RTC button
#define  PUSHBUTTON_GPIO (gpio_num_t) 25
#define ADC_ENABLE_PIN 26
//ESP-ADC PINS 
#define  ADC_PIN 32
//Buffer used to store adc results changed to
// mgos_sys_config_get_pyrometer_adc_size() function call
/*Configs LOG,WDT,RPC,ADC,MQTT*/
void init_esp(void);
/*Configs Display,Push button switch*/
void init_peripherals(void);
float adc_median_mv(int adc_pin);
//RPC Callback. RPCs work only out of loops. 
void adc_median_mv_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void reboot_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void powerout_reason_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void battery_voltage_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void change_mode_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void rssi_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void distance_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void distance_setter_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
void mlx_av_setter_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args);
//ESP sleep
void esp_dsleep_setup(void);
//Touchpad setup
void esp_touch(void);
//Gaussian filter
float gaussian_filter_mlx(void);
//Battery voltage average value

float adc_average(int adc_pin);
//MQTT subscribe callback
static void mqtt_sub_cb(struct mg_connection * nc, int ev, void * ev_data MG_UD_ARG(void * ud));
//MQTT Configuration
void init_mqtt();
void adc_mqtt_pub(int adc_pin);
//Net event handler
void net_event_cb(int ev, void * ev_data, void * args);
/*Reset reasoning & wakeup reasoning -- as seen from Ben Rockwood @ 
 *https://github.com/benr/esp32-deepsleep-c/blob/master/src/main.c
 */
char * why_reset();
void why_wake();
void esp_sleep(void);

//mgos_mlx90640 is a C library therefore extern C is needed because the main code is compiled as C++
extern "C" {
  #include "mgos_mlx90640.h"
}

VL53L0X * sens = mgos_VL53L0X_create();
void init_vl53l0x(){
  mgos_VL53L0X_begin(sens);
  mgos_VL53L0X_setTimeout(sens,500);
  if(mgos_VL53L0X_init_2v8(sens)){
    LOG(LL_INFO,("VL53L0X initiallized"));
  } else {
    LOG(LL_INFO,("VL53L0X failed to initialize"));
  }
}

/* SCREEN STUFF TESTED, WORKS ON PINS 22 AND 23*/
/***************---------------SCREEN STUFF-----------*********************/
//Adafruit library for OLED 
Adafruit_SSD1306 * display = nullptr;

static void setup_display(void) {
  display = new Adafruit_SSD1306(-1 /* NO RST GPIO */ , Adafruit_SSD1306::RES_128_64);
  if (display != nullptr) {
    display->begin(SSD1306_SWITCHCAPVCC, 0x3C, true /* reset */ );
    display->clearDisplay();
    display->display();
  } else {
    LOG(LL_ERROR, ("ERROR: Display failed to initialize"));
  }
}

class signalBar {
  /*By default the style width is 2 (columns). If we change the the column style to another number then the signal bar should get wider (aka fatter). ToBeImplemented(TBI)*/
  //uint8_t STYLE= 2;
  /*RSSI of given signal. RSSI is expected to be a negative int or 0 if no signal is available*/
  private:
    int _RSSI = NULL;
    int _bars = NULL;
    int _dot = NULL;
  public:
    Adafruit_SSD1306 * _displayPtr = nullptr;
  signalBar(int rssi, Adafruit_SSD1306 * d_ptr, int bars) {
    _RSSI = rssi;
    _displayPtr = d_ptr;
    _bars = bars;
    evaluate_rssi();
  }
  signalBar(int rssi, Adafruit_SSD1306 * d_ptr) {
    _RSSI = rssi;
    _displayPtr = d_ptr;
    _bars = 0;
    evaluate_rssi();
  }
  signalBar(Adafruit_SSD1306 * d_ptr) {
    _RSSI = 0;
    _displayPtr = d_ptr;
    _bars = 0;
    evaluate_rssi();
  }
  signalBar() {
    _RSSI = 0;
    _bars = 0;
    _displayPtr = nullptr;
  }
  /*Display object*/
  /*Signal bards currently displayed*/
  void set_rssi(int rssi) {
    _RSSI = rssi;
    evaluate_rssi();
  }
  /* Sets private _rssi.
  We expect a negative RSSI value in the range of [0,-140] */

  bool set_bars(int bars) {
    if ((int) bars >= 0) {
      if ((int) bars <= 3) {
        _bars = bars;
        return 1;
      }
    }
    //need to return something for evaluation reasons
    return 0;
  }
  int get_bars() {
    return _bars;
  }

  /*TBI*/
  /*void set_style(uint8_t style){
   *STYLE = style
   */
  /* Draws a bar given width,height,ssd1306 object and color*/
  void drawOne(uint8_t width_end, uint8_t width_start, uint8_t height_start, uint8_t height_end, uint16_t color) {
    for (uint8_t x = width_start; x <= width_end; x++) {
      for (uint8_t y = height_start; y <= height_end; y++) {
        _displayPtr->drawPixel((int16_t) x, (int16_t) y, (int16_t) color);
        _displayPtr->display();
      }
    }
  }
  int get_rssi() {
    return _RSSI;
  }
  void drawDots() {
    drawOne(122,118,6,10,WHITE);
    _dot = 1;
  }
  void deleteDot(){
    drawOne(122,118,6,10,BLACK);
    _dot = 0;
  }
  /*Expects number of bars to be drawn at the screen */
  bool drawBars(uint8_t numberOfBars) {
    uint8_t bar[3][4] = {
      {
        118,
        117,
        5,
        10
      },
      {
        122,
        121,
        2,
        10
      },
      {
        126,
        125,
        0,
        10
      }
    };
    if (numberOfBars == 3) {
      for (int row = 0; row < 3; row++) {
        drawOne(bar[row][0], bar[row][1], bar[row][2], bar[row][3], WHITE);
      }
      return 1;
    } else if (numberOfBars == 2) {
      for (int row = 0; row < 2; row++) {
        drawOne(bar[row][0], bar[row][1], bar[row][2], bar[row][3], WHITE);
      }
      drawOne(bar[2][0], bar[2][1], bar[2][2], bar[2][3], BLACK);
      return 1;
    } else if (numberOfBars == 1) {
      for (int row = 0; row < 2; row++) {
        drawOne(bar[row][0], bar[row][1], bar[row][2], bar[row][3], WHITE);
      }
      drawOne(bar[2][0], bar[2][1], bar[2][2], bar[2][3], BLACK);
      drawOne(bar[1][0], bar[1][1], bar[1][2], bar[1][3], BLACK);
      return 1;
    }
    //If 0 is returned as numberOfBars then we turn of the bar sings
    else if (numberOfBars <= 0) {
      for (int row = 0; row < 3; row++) {
        drawOne(bar[row][0], bar[row][1], bar[row][2], bar[row][3], BLACK);
      }
      return 1;
    }
    return 0;
  }
  void evaluate_rssi() {
    int rssi = get_rssi();
    LOG(LL_INFO,("RSSI IS:%d",rssi));
    if (rssi!=0){
      if (_dot == 1){
        deleteDot();
      }
      if (rssi>=-55){
        if(get_bars()!=3){
        set_bars(3);
        drawBars(3);
      }} else if(rssi>=-70){
        if(get_bars()!=2){
        set_bars(2);
        drawBars(2);
      }} else if (rssi>=-80){
        if(get_bars()!=1){
        set_bars(1);
        drawBars(1);
      }}
    } else {
      drawDots();
    }
  }
};

signalBar * wifi = nullptr;

static void setup_wifi_bar(void) {
  wifi = new signalBar(0, display, 0);
}

static void display_wifi() {
  wifi->set_rssi(mgos_wifi_sta_get_rssi());
}

//Send info to be displayed. wifi data will only be refreshed and displayed when wifi signal changes
static void display_screen(Adafruit_SSD1306 * d, float t, float b,int distance) {
  d->clearDisplay();
  d->setTextSize(2);
  d->setTextColor(WHITE);
  d->setCursor(d->width() / 18, d->height() / 2.4);
  d->printf("Temp:%.1fC", t);
  d->setCursor(d->width() / 18, d->height() / 1.3);
  if (b<=2){
    d->printf("Batt:NC");
  }
  else{
    d->printf("Batt:%.2f",b);
  }
  d->setCursor(d->width()/18,d->height()/13);
  if(distance>=mgos_sys_config_get_distance_sens_min()){
    if(distance<=mgos_sys_config_get_distance_sens_max()){
        d->printf("D:OK");
    }
  } 
  if(distance<mgos_sys_config_get_distance_sens_min()){
    d->printf("D:CLOSE");
  }
  if(distance>mgos_sys_config_get_distance_sens_max()){
    if(distance<=9000){
    d->printf("D:FAR");
    }
  }
  if(distance==65535){
    d->printf("D:NC");
  }
  display_wifi();
  d->display();
}

/*Shut down display. Mainly used for power saving */
void shutdownDisplay(Adafruit_SSD1306 * d) {
  d->ssd1306_command(SSD1306_DISPLAYOFF);
}

//Initialize screen and display temperature
//static void init_display_temp(float i)
static void init_display_temp(float temp) {
  setup_display();
}
/***************---------------SCREEN STUFF-----------*********************/

/*Returns average temperature read by thermopile */
float mlx90640_av_temp() {

  //Read frame from thermal camera
  float * framePtr = getMLX90640FramePtr();
  float averageTemp = 0;
  for (int i = 0; i < MLX90640_FRAME_BUFFER_SIZE; i++) {
    averageTemp += framePtr[i];
  }
  LOG(LL_INFO, ("Average temperature:%f", averageTemp / (MLX90640_FRAME_BUFFER_SIZE)));
  //Return average of all the pixels
  return (averageTemp / (MLX90640_FRAME_BUFFER_SIZE));
}

/*Returns single (middle) point read by thermopile*/
float mlx90640_temp(){
 float temp=0;
 for (int i=0;i<mgos_sys_config_get_mlxav_numb();i++){
  float * framePtr = getMLX90640FramePtr();
  temp += *(framePtr+191);
 }
  return (temp/mgos_sys_config_get_mlxav_numb());
}

/*Reads and displays mxl temperature,battery voltage and publishes mlx temperature on MQTT server configured*/
float prev_value = 0;
static void mlx90640Read_cb(void * arg) {
  char buffer[8];
  float temperature =0;
  if(mgos_sys_config_get_pyrometer_select_filter() == 1){
      temperature = mlx90640_temp();
    if(abs(prev_value-temperature) < 0.6){
      if (temperature > prev_value){
        temperature = prev_value + (temperature/24);
      }
      if (temperature < prev_value){
        temperature = prev_value - (temperature/24);
      }
      prev_value = temperature;
    }
  }
  
  if(mgos_sys_config_get_pyrometer_select_filter() == 2){
    temperature = gaussian_filter_mlx();
    if(abs(prev_value-temperature) < 0.6){
      if (temperature > prev_value){
        temperature = prev_value + (temperature/24);
      }
      if (temperature < prev_value){
        temperature = prev_value - (temperature/24);
      }
      prev_value = temperature;
    }
  }
  
  if(mgos_sys_config_get_pyrometer_select_filter() == 3){
     if((temperature < (float)mgos_sys_config_get_pyrometer_max()) && (temperature > (float)mgos_sys_config_get_pyrometer_max())){
      uint32_t select = esp_random()%4 + 1;
      if (select == 1){
        temperature = 36.6;
      }
      if (select == 2){
        temperature = 36.7;
      }
      if (select == 3){
        temperature = 36.8;
      }
      if (select == 4){
        temperature = 36.9;
      }
      if (select == 5){
        temperature = 37.0;
      }
      uint32_t point = esp_random()%899 +100;
      temperature = temperature + ((float)point /10000);
      LOG(LL_INFO,("Temperature:%f",temperature));
  }
  }
  
  int ret = snprintf(buffer, sizeof buffer, "%5f", temperature);
  mgos_mqtt_pub("temp", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
  /*
  char buf_dist[4];
  int * dist = mgos_VL53L0X_readRangeSingleMillimeters(sens);
  int distance = sprintf(buf_dist,sizeof buf_dist,"%2d",dist);
  */
  int distance = mgos_VL53L0X_readRangeSingleMillimeters(sens);
  LOG(LL_INFO,("Range is:%d",distance));
  LOG(LL_INFO,("Temp is:%.4f",temperature));
  
  display_screen(display, temperature, ((adc_median_mv(32) / 1000) * 2),distance);
  //LOG(LL_INFO,("Distance measured:%d",mgos_VL53L0X_readRangeSingleMillimeters(sens)));

}

static void mlx90640Read_cb2(void * arg) {
  char buffer[8];
  float temperature = 0;
  if(mgos_sys_config_get_pyrometer_select_filter() == 1){
      temperature = mlx90640_temp();
    if(abs(prev_value-temperature) < 0.4){
      if (temperature > prev_value){
        temperature = prev_value + (temperature/2);
      }
      if (temperature < prev_value){
        temperature = prev_value - (temperature/2);
      }
      prev_value = temperature;
    }
  }
  
  if(mgos_sys_config_get_pyrometer_select_filter() == 2){
    temperature = gaussian_filter_mlx();
    if(abs(prev_value-temperature) < 0.4){
      if (temperature > prev_value){
        temperature = prev_value + (temperature/2);
      }
      if (temperature < prev_value){
        temperature = prev_value - (temperature/2);
      }
      prev_value = temperature;
    }
  }
  
  if(mgos_sys_config_get_pyrometer_select_filter() == 3){
     if((temperature < (float)mgos_sys_config_get_pyrometer_max()) && (temperature > (float)mgos_sys_config_get_pyrometer_max())){
      uint32_t select = esp_random()%4 + 1;
      if (select == 1){
        temperature = 36.6;
      }
      if (select == 2){
        temperature = 36.7;
      }
      if (select == 3){
        temperature = 36.8;
      }
      if (select == 4){
        temperature = 36.9;
      }
      if (select == 5){
        temperature = 37.0;
      }
      uint32_t point = esp_random()%899 +100;
      temperature = temperature + ((float)point /10000);
      LOG(LL_INFO,("Temperature:%f",temperature));
     //}
  }
  }
  
  int ret = snprintf(buffer, sizeof buffer, "%5f", temperature);
  mgos_mqtt_pub("temp", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
  int distance = mgos_VL53L0X_readRangeSingleMillimeters(sens);
  LOG(LL_INFO,("Range is:%d",distance));
  display_screen(display, temperature, ((adc_median_mv(32) / 1000) * 2),distance);
  //If the user has released the pushbutton go to sleep
  if (mgos_gpio_read(PUSHBUTTON_GPIO) == 1) {
    LOG(LL_INFO,("GPIO SLEEP READ:%d",mgos_gpio_read(PUSHBUTTON_GPIO)));
    esp_sleep();
  }
}

/*Battery voltage average value*/
float adc_average(int adc_pin) {
  float voltage = 0;
  for (int i = 0; i < 10; i++) {
    voltage += mgos_adc_read(adc_pin);
  }
  return (voltage / 10);
}

/*Gravity OLED-2864 Address = 0x3C*/
//First we setup the screen

//Then we display data

/* Logic to be added:
 *Stationary mode: Wifi is connected (!need to check that!)->sleep(50s)->wakeup & send info
 *Mobile mode: Wakeup from deep sleep 
 */

enum mgos_app_init_result mgos_app_init(void) {
  LOG(LL_INFO, ("----------MAIN  APP----------"));
  //Need to configure esp wakeup from deep sleep
  init_esp();
  init_peripherals();
  /*If mode = 1 then we are in automatic mode. That means no user interraction is needed. Else mode = 0 then pushbutton is needed*/
  if (mgos_sys_config_get_pyrometer_mode() == 1) {
    mgos_set_timer(mgos_sys_config_get_mlxav_loop_timer(), MGOS_TIMER_REPEAT, mlx90640Read_cb, NULL);
  } else {
    //While pushbutton is pushed,work as normal
    mgos_set_timer(mgos_sys_config_get_mlxav_loop_timer(), MGOS_TIMER_REPEAT, mlx90640Read_cb2, NULL);
  }
  return MGOS_APP_INIT_SUCCESS;
}

/* TESTED OK*/
/*Configs Display,Push button switch*/
void init_peripherals(void) {
  setup_display();
  setup_wifi_bar();
  if (mgos_gpio_setup_input(PUSHBUTTON_GPIO, MGOS_GPIO_PULL_UP)) {
    LOG(LL_DEBUG, ("PUSHBUTTON GPIO:%d SETUP SUCESS", PUSHBUTTON_GPIO));
  } else {
    LOG(LL_ERROR, ("ERROR: PUSHBUTTON GPIO:%d SETUP FAIL", PUSHBUTTON_GPIO));

  }
  /*esp_touch(); */
  init_vl53l0x();
  LOG(LL_DEBUG, ("INIT PERIPHERAL EXECUTED"));
  return;
}
/* Only ADC1 GPIO<32:39> to be used, otherwise WiFi related errors occur
 * https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/adc.html
 * TESTED OK
 */
/*Configs LOG,WDT,RPC,ADC,MQTT*/
void init_esp(void) {
  //This always needs to run on boot.
  rtc_gpio_deinit(PUSHBUTTON_GPIO);
  //Serial port will print only LL_XXXX marked LOGS
  cs_log_set_level(LL_DEBUG);
  //WDT disabled so as to disable reboots due to wdt timeout
  mgos_wdt_disable();
  //RPC Config
  mg_rpc_add_handler(mgos_rpc_get_global(), "ADC_median", "{pin: %d}", adc_median_mv_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "reboot", "{timer: %d}", reboot_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "reset_reason", "{a: %d}", powerout_reason_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "battery_voltage", "{a: %d}", battery_voltage_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "change_mode", "{mode: %d, adc_size: %d, gaussian_filter: %d}", change_mode_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "rssi", "{a: %d}", rssi_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "distance", "{a: %d}", distance_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "distance_set", "{min: %d, max: %d}", distance_setter_cb, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "av", "{avg_filter: %d, loop_timer: %d}, select_filter: %d", mlx_av_setter_cb, NULL);

  //ESP ADC Config
  if (mgos_adc_enable(ADC_PIN)) {
    LOG(LL_DEBUG, ("ADC ENABLED"));
  } else {
    LOG(LL_ERROR, ("ERROR: ADC ENABLE FAILED"));
  }
  //ADC GPIO 
  mgos_gpio_setup_output(ADC_ENABLE_PIN,0);
  //MQTT config  
  init_mqtt();
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_event_cb, NULL);
  LOG(LL_DEBUG, ("INIT ESP EXECUTED"));
  return;
}

/* Samples and returns rolling average
 * mainly used to reject any wifi burst noise & gaussian noise and improve adc results
 * due to some non-linearity
 */
float adc_median_mv(int adc_pin) {
  //Sample & hold in buffer.
  mgos_gpio_write(ADC_ENABLE_PIN,1);
  mgos_msleep(50);
  int adc_buf[mgos_sys_config_get_pyrometer_adc_size()] = {
    0
  };
  for (int i = 0; i < mgos_sys_config_get_pyrometer_adc_size(); i++) {
    adc_buf[i] = mgos_adc_read_voltage(adc_pin);
  }
  //ShellShort the data. Code adapted from geeksforgeeks.org/shellshort
  for (int gap = mgos_sys_config_get_pyrometer_adc_size() / 2; gap > 0; gap /= 2) {
    for (int i = gap; i < mgos_sys_config_get_pyrometer_adc_size(); i += 1) {
      int temp = adc_buf[i];
      int j;
      for (j = i; j >= gap && adc_buf[j - gap] > temp; j -= gap) {
        adc_buf[j] = adc_buf[j - gap];
      }
      adc_buf[j] = temp;
    }
  }
  //Return median mv
  LOG(LL_DEBUG, ("adc_median_mv EXECUTED"));
  float returns = (adc_buf[((mgos_sys_config_get_pyrometer_adc_size() - 1) / 2) - 1] + adc_buf[((mgos_sys_config_get_pyrometer_adc_size() - 1) / 2) + 1] + adc_buf[((mgos_sys_config_get_pyrometer_adc_size() - 1) / 2)]) / 3;
  return returns;
}
/*Gaussian filter */
 float gaussian_filter_mlx(){
 
   //Record the data
   float mlx_buf[mgos_sys_config_get_pyrometer_adc_size()]= {0};
   for(int i=0;i<mgos_sys_config_get_pyrometer_adc_size();i++){
     float *temp = getMLX90640FramePtr();
     mlx_buf[i] = *(temp +191);
   }
   //Shellshort them
   for(int gap = mgos_sys_config_get_pyrometer_adc_size() / 2; gap > 0; gap /= 2) {
 
     for (int i = gap; i < mgos_sys_config_get_pyrometer_adc_size(); i += 1) {
 
       float temp = mlx_buf[i];
 
       int j;
 
       for (j = i; j >= gap && mlx_buf[j - gap] > temp; j -= gap) {
 
         mlx_buf[j] = mlx_buf[j - gap];
 
       }
 
       mlx_buf[j] = temp;
 
     }
 
   };
 
   //Calculate mean
 
   float mean=0;
 
   for(int i=0;i<mgos_sys_config_get_pyrometer_adc_size();i++){
 
     mean +=mlx_buf[i];
 
   };
 
   mean = mean/mgos_sys_config_get_pyrometer_adc_size();
   //LOG(LL_INFO,("Mean:%f",mean));
   //Calculate sum of squares
 
   float temp =0;

   for(int i=0;i<mgos_sys_config_get_pyrometer_adc_size();i++){
 
     temp+=((mlx_buf[i]-mean)*(mlx_buf[i]-mean));
 
   };
 
   float variance = temp/(mgos_sys_config_get_pyrometer_adc_size()-1);
   //LOG(LL_INFO,("Variance:%f",variance));
   //Calculate standard deviation 
 
   float std_deviation = sqrt(variance);
   //LOG(LL_INFO,("std_deviation:%f",std_deviation));
   //Average filter. Max and min are there to calculate how many standard deviations are to be taken into account
 
   float max = mean+(std_deviation*mgos_sys_config_get_pyrometer_gaussian_filter());
 
   float min = mean-(std_deviation*mgos_sys_config_get_pyrometer_gaussian_filter());
   //LOG(LL_INFO,("min,max:%f %f",min,max));
   temp = 0;
 
   int count=0;
 
   for(int i=1;i<(mgos_sys_config_get_pyrometer_adc_size()-1);i++){
 
     if (!(mlx_buf[i]> max) || (mlx_buf[i]<min)){
 
       temp +=mlx_buf[i];
 
       count++;
 
     }
 
   };
 
   return (temp/count);
 }

/* RPC Callbacks work only if one serial line is used
* for example you cant send serial data and call an rpc
* TESTED OK
THIS FUNCTION IS NOT USED IN THESIS
*/

void adc_median_mv_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  //JavaScript represents "Number" as double but int works fine for pin numbers
  int pin = 0;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, & pin) == 1) {
    //Check that pin is used for ADC
    if (pin == 36) {
      mg_rpc_send_responsef(ri, "%d", adc_median_mv((int) pin));

      LOG(LL_DEBUG, ("REQUEST ANSWERED"));
    } else {
      mg_rpc_send_errorf(ri, -1, "Bad Request.Invalid pin");
    }
  } else {
    LOG(LL_ERROR, ("ERROR: RPC REQUEST ERROR IN ADC MEDIAN FUNCTION"));
    mg_rpc_send_errorf(ri, -1, "Bad Request. Expected {\"pin\":Number}");
  }
  (void) cb_arg;
  (void) fi;
  LOG(LL_DEBUG, ("ADC MEDIAN SAMPLING EXECUTED"));
  return;
}

void distance_setter_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  //JavaScript represents "Number" as double but int works fine for pin numbers
  int min = 0;
  int max = 0;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, & min, & max) == 2) {
    if (min != 0) {
      mgos_sys_config_set_distance_sens_min(min);
    } else {
      mg_rpc_send_errorf(ri, -1, "Bad Request.");
    }
    if(max !=0) {
      mgos_sys_config_set_distance_sens_max(max);
    } else{
      mg_rpc_send_errorf(ri,-1,"Bad request");
    }
    save_cfg( & mgos_sys_config, NULL); /* Writes conf9.json */
    mgos_system_restart_after(5);
  } else {
    mg_rpc_send_errorf(ri, -1, "Bad Request.");
  }
  (void) cb_arg;
  (void) fi;
  LOG(LL_DEBUG, ("Distance setter called"));
  return;
}

void mlx_av_setter_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  //JavaScript represents "Number" as double but int works fine for pin numbers
  int avg_filter = 0;
  int loop_timer = 0;
  int select_filter = 0;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, &avg_filter, &loop_timer, &select_filter) == 3) {
    if (avg_filter != 0) {
      mgos_sys_config_set_mlxav_numb(avg_filter);
    } else {
      mg_rpc_send_errorf(ri, -1, "Bad Request.");
    }
    if (loop_timer != 0) {
      mgos_sys_config_set_mlxav_loop_timer(loop_timer);
    } else {
      mg_rpc_send_errorf(ri, -1, "Bad Request.");
    }
    if (select_filter != 0) {
      mgos_sys_config_set_pyrometer_select_filter(select_filter);
    } else {
      mg_rpc_send_errorf(ri, -1, "Bad Request.");
    }
    save_cfg( & mgos_sys_config, NULL); /* Writes conf9.json */
    mgos_system_restart_after(5);
  } else {
    mg_rpc_send_errorf(ri, -1, "Bad Request.");
  }
  (void) cb_arg;
  (void) fi;
  LOG(LL_DEBUG, ("mlxav setter called"));
  return;
}

void reboot_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  int timer = 0;
  char * buffer = "Rebooting";
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, & timer) == 1) {
    //Check that pin is used for ADC
    mg_rpc_send_responsef(ri, "%d", timer);
    mgos_mqtt_pub("ir-iot/reboot", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
    LOG(LL_DEBUG, ("REQUEST ANSWERED"));
    mgos_system_restart_after(timer * 1000);

  } else {
    LOG(LL_ERROR, ("ERROR: RPC REQUEST ERROR IN REBOOT CB"));
    mg_rpc_send_errorf(ri, -1, "Bad Request. Expected {\"timer\":Number}");
  }
  (void) cb_arg;
  (void) fi;
}

void powerout_reason_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  char * buffer = why_reset();
  int dummy = -1;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, & dummy) == 1) {
    //Check that pin is used for ADC
    mgos_mqtt_pub("ir-iot/powerout", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
    LOG(LL_DEBUG, ("REQUEST ANSWERED"));
  } else {
    LOG(LL_ERROR, ("ERROR: RPC REQUEST ERROR IN POWEROUT_REASON CB"));
    mg_rpc_send_errorf(ri, -1, "Bad Request.");
  }
  (void) cb_arg;
  (void) fi;
}

void battery_voltage_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  char helper[] = "0";
  sprintf(helper, "%f", ((adc_median_mv(32) / 1000) * 2));
  char * buffer = helper;
  int dummy = -1;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, & dummy) == 1) {
    //Check that pin is used for ADC
    mgos_mqtt_pub("ir-iot/battery", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
    LOG(LL_DEBUG, ("REQUEST ANSWERED"));
  } else {
    LOG(LL_ERROR, ("ERROR: RPC REQUEST ERROR IN BATTERY VOLTAGE CB"));
    mg_rpc_send_errorf(ri, -1, "Bad Request.");
  }
  (void) cb_arg;
  (void) fi;
}

void change_mode_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  char * buffer = "0";
  int mode = 0;
  int adc_size = 0;
  int gaussian_filter = 0;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, &mode, &adc_size, &gaussian_filter) == 3) {
    //Check that pin is used for ADC
    if (mode == 1) {
      buffer = "Device set to Auto mode";
      mgos_sys_config_set_pyrometer_mode(1);
    } else {
      buffer = "Device set to Manual mode";
      mgos_sys_config_set_pyrometer_mode(0);
    }
    if (adc_size !=0) {
      mgos_sys_config_set_pyrometer_adc_size(adc_size);
    }
    if (gaussian_filter !=0) {
      mgos_sys_config_set_pyrometer_gaussian_filter(gaussian_filter);
    }

    mgos_mqtt_pub("ir-iot/mode", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
    LOG(LL_DEBUG, ("REQUEST ANSWERED"));
    save_cfg( & mgos_sys_config, NULL); /* Writes conf9.json */
    mgos_system_restart_after(100);
  } else {
    LOG(LL_ERROR, ("ERROR: RPC REQUEST ERROR IN CHNAGE MODE FUNCTION"));
    mg_rpc_send_errorf(ri, -1, "Bad Request. Expected {\"mode\":Number}");
  }
  (void) cb_arg;
  (void) fi;
}

void rssi_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  char helper[] = "0";
  sprintf(helper, "%d", mgos_wifi_sta_get_rssi());
  char * buffer = helper;
  int dummy = -1;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, & dummy) == 1) {
    //Check that pin is used for ADC
    mgos_mqtt_pub("ir-iot/rssi", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
    LOG(LL_DEBUG, ("REQUEST ANSWERED"));
  } else {
    LOG(LL_ERROR, ("ERROR: RPC REQUEST ERROR IN BATTERY VOLTAGE CB"));
    mg_rpc_send_errorf(ri, -1, "Bad Request.");
  }
  (void) cb_arg;
  (void) fi;
}

void distance_cb(struct mg_rpc_request_info * ri, void * cb_arg, struct mg_rpc_frame_info * fi, struct mg_str args) {
  char helper[] = "0";
  sprintf(helper, "%d",mgos_VL53L0X_readRangeSingleMillimeters(sens));
  char * buffer = helper;
  int dummy = -1;
  //json_scanf(arg.parameter,arg.length,req info, &saveinfo1..) = number of elements scanned
  if (json_scanf(args.p, args.len, ri->args_fmt, & dummy) == 1) {
    //Check that pin is used for ADC
    mgos_mqtt_pub("ir-iot/distance", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
    LOG(LL_DEBUG, ("REQUEST ANSWERED"));
  } else {
    LOG(LL_ERROR, ("ERROR: RPC REQUEST ERROR IN BATTERY VOLTAGE CB"));
    mg_rpc_send_errorf(ri, -1, "Bad Request.");
  }
  (void) cb_arg;
  (void) fi;
}

/* Timer deep sleep for MQTT, ext0 deep sleep for button push
 * DEEP SLEEP CAUSES REBOOT.
 * Before Deep Sleep WiFi needs to be turned off gracefully 
 * ONLY RTC_DATA_ATTR type are held in memory after reboot
 */
void esp_dsleep_setup(void) {
  //Deep sleep external setup -- TESTED OK. On PUSHBUTTON_GPIO->GND we have reset
  if (esp_sleep_enable_ext0_wakeup(PUSHBUTTON_GPIO, 0) == ESP_OK) {
    LOG(LL_DEBUG, ("EXT0 WAKEUP SETUP SUCESS"));
  } else {
    LOG(LL_ERROR, ("ERROR: EXT0 WAKEUP SETUP FAILED"));
  }
  return;
}

void esp_sleep(void) {
  //Before sleep we disconnect from wifi grcefully
  esp_dsleep_setup();
  if (mgos_wifi_get_status() == MGOS_WIFI_CONNECTED) {
    if (mgos_wifi_disconnect()) {
      LOG(LL_DEBUG, ("WIFI DISCONNECTED"));
    } else {
      LOG(LL_ERROR, ("WIFI DISCONNECTION FAILED"));
    }
  }
  //And we shutdown the display
  shutdownDisplay(display);

  LOG(LL_INFO, ("ENTERING DEEP SLEEP"));
  esp_deep_sleep_start();
}

/*MQTT subscribSUBACK = 209opic, handle data to mqtt_temp_sub callback function
 * Callback receives SUBACK,PUBLISH,PUBACK,MG_EV_CLOSE when connection is closed
 * BASE = 200
 * CMD: MG_MQTT_CMD_PUBLISH = 203
 * CMD: MG_MQTT_CMD_SUBACK = 209
 * CMD: MG_MQTT_CMD_PUBACK = 204
 * FINAL CODE = BASE + CMD
 */

static void mqtt_sub_cb(struct mg_connection * nc, int ev, void * ev_data MG_UD_ARG(void * ud)) {
  switch (ev) {
    //In case we received SUBACK therefore we are connected to the server (due to previous sub to <device_id>/rpc) 
  case MG_EV_MQTT_SUBACK:
    LOG(LL_INFO,("Received Suback"));
    adc_mqtt_pub(2);
  }
  (void) nc;
  (void) ud;
  return;
}

//Configs MQTT max QOS, 
void init_mqtt() {
  /*MQTT subscribe to 'sensor/temp' topic, handle data to mqtt_sub_cb callback function
   * Callback receives SUBACK,PUBLISH,PUBACK,MG_EV_CLOSE when connection is closed
   */
  //mgos_mqtt_set_max_qos(2);
  //Defines MQTT subscribe handler
  mgos_mqtt_global_subscribe(mg_mk_str("temp"), mqtt_sub_cb, NULL);
  //Test if MQTT is connected 
  if (mgos_mqtt_global_is_connected()) {
    LOG(LL_INFO, ("MQTT SERVER CONNECTED"));
  }
  LOG(LL_INFO, ("MQTT INIT EXECUTED"));
  return;
}

void adc_mqtt_pub(int adc_pin) {
  char buffer[4];
  itoa(adc_median_mv(adc_pin), buffer, 10);
  //Publish ADC data->sensors/temp
  mgos_mqtt_pub("test", buffer, (strlen(buffer) * sizeof(char)), 1, 0);
  LOG(LL_INFO, ("MQTT ADC SEND"));
}

//Defines what happens when internet gets connected
void net_event_cb(int ev, void * ev_data, void * args) {
  switch (ev) {
    //When IP is aquired connect to the MQTT server if not already connected
  case MGOS_NET_EV_IP_ACQUIRED:
    LOG(LL_INFO, ("INTERNET CONNECTED"));
    if (!mgos_mqtt_global_is_connected()) {
      init_mqtt();
    }
    break;
  case MGOS_NET_EV_DISCONNECTED:
  mgos_mqtt_global_disconnect();
  break;
  }
}

char * why_reset() {
  int reset_reason = rtc_get_reset_reason(0);
  printf("Reset Reason (%d): ", reset_reason);
  switch (reset_reason) {
  case 1:
    return "Vbat power on reset";
    break;
  case 3:
    return "Software reset digital core";
    break;
  case 4:
    return "Legacy watch dog reset digital core";
    break;
  case 5:
    return "Deep Sleep reset digital core";
    break;
  case 6:
    return "Reset by SLC module, reset digital core";
    break;
  case 7:
    return "Timer Group0 Watch dog reset digital core";
    break;
  case 8:
    return "Timer Group1 Watch dog reset digital core";
    break;
  case 9:
    return "RTC Watch dog Reset digital core";
    break;
  case 10:
    return "Instrusion tested to reset CPU";
    break;
  case 11:
    return "Time Group reset CPU";
    break;
  case 12:
    return "Software reset CPU";
    break;
  case 13:
    return "RTC Watch dog Reset CPU";
    break;
  case 14:
    return "for APP CPU, reseted by PRO CPU";
    break;
  case 15:
    return "Reset when the vdd voltage is not stable";
    break;
  case 16:
    return "RTC Watch dog reset digital core and rtc module";
    break;
  default:
    return "NO_MEAN";
  }
  printf("\n");
  LOG(LL_INFO, ("WHY_RESET EXECUTED"));
}

void why_wake() {
  int wake_cause = esp_sleep_get_wakeup_cause();
  printf("Wake Cause (%d): ", wake_cause);
  switch (wake_cause) {
  case 1:
    printf("Wakeup caused by external signal using RTC_IO");
  case 2:
    printf("Wakeup caused by external signal using RTC_CNTL");
  case 3:
    printf("Wakeup caused by timer");
  case 4:
    printf("Wakeup caused by touchpad");
  case 5:
    printf("Wakeup caused by ULP program");
  default:
    printf("Undefined.  In case of deep sleep, reset was not caused by exit from deep sleep.");
  }
  LOG(LL_INFO, ("WHY_WAKE EXECUTED"));
}
