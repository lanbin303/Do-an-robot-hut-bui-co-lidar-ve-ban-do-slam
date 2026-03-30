#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "driver/uart.h" 
#include "driver/ledc.h" 
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define WIFI_SSID "Robot_Cua_Lan"
#define WIFI_PASS "12345678"
#define IN1 27
#define IN2 26
#define IN3 25
#define IN4 33
#define UART_NUM UART_NUM_2

uint16_t lidar_map[360] = {0};
volatile int front_dist = 9999;
volatile bool auto_mode = false;

// --- GIAO DIEN WEB RADAR ---
const char html_page[] = "<!DOCTYPE html><html><head><meta charset='UTF-8'><style>"
"body{text-align:center;font-family:sans-serif;background:#111;color:#0f0;margin:0;}"
"canvas{background:#000;border:2px solid #0f0;border-radius:50%;margin-top:10px;}"
".btn{background:#222;border:2px solid #0f0;color:#0f0;padding:10px;margin:5px;width:100px;font-weight:bold;}"
"</style></head><body>"
"<h3>📡 RADAR MAP 2D</h3>"
"<canvas id='r' width='300' height='300'></canvas><br>"
"<button class='btn' onclick='f(\"/auto\")'>TỰ ĐỘNG</button><br>"
"<button class='btn' onclick='f(\"/fwd\")'>TIẾN</button><br>"
"<button class='btn' onclick='f(\"/left\")'>TRÁI</button>"
"<button class='btn' onclick='f(\"/stop\")' style='color:red;border-color:red'>DỪNG</button>"
"<button class='btn' onclick='f(\"/right\")'>PHẢI</button><br>"
"<button class='btn' onclick='f(\"/bwd\")'>LÙI</button>"
"<script>"
"function f(u){fetch(u);}"
"const c=document.getElementById('r');const x=c.getContext('2d');"
"setInterval(()=>{fetch('/map').then(r=>r.text()).then(d=>{"
"const p=d.split(',');x.clearRect(0,0,300,300);"
"x.strokeStyle='#030';x.beginPath();x.arc(150,150,140,0,7);x.stroke();"
"x.fillStyle='red';"
"for(let a=0;a<360;a++){let dist=parseInt(p[a]);if(dist>50&&dist<3000){"
"let rad=(a-90)*0.0174;let px=150+(dist/15)*Math.cos(rad);let py=150+(dist/15)*Math.sin(rad);"
"x.fillRect(px,py,2,2);}}x.fillStyle='#0f0';x.fillRect(148,148,4,4);"
"});},400);</script></body></html>";

// --- MOTOR & UART ---
void init_motors(){
    ledc_timer_config_t t={LEDC_LOW_SPEED_MODE,LEDC_TIMER_8_BIT,LEDC_TIMER_0,5000,LEDC_AUTO_CLK};
    ledc_timer_config(&t);
    int p[4]={IN1,IN2,IN3,IN4};
    for(int i=0;i<4;i++){
        ledc_channel_config_t c={p[i],LEDC_LOW_SPEED_MODE,(ledc_channel_t)i,LEDC_INTR_DISABLE,LEDC_TIMER_0,0,0};
        ledc_channel_config(&c);
    }
}
void set_motor(int sl,int sr){
    ledc_set_duty(LEDC_LOW_SPEED_MODE,0,sl>0?sl:0);ledc_set_duty(LEDC_LOW_SPEED_MODE,1,sl<0?-sl:0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE,2,sr>0?sr:0);ledc_set_duty(LEDC_LOW_SPEED_MODE,3,sr<0?-sr:0);
    for(int i=0;i<4;i++)ledc_update_duty(LEDC_LOW_SPEED_MODE,(ledc_channel_t)i);
}

void auto_task(void*a){
    while(1){
        if(auto_mode){
            if(front_dist<250){
                set_motor(0,0);vTaskDelay(200/portTICK_PERIOD_MS);
                set_motor(-120,-120);vTaskDelay(400/portTICK_PERIOD_MS);
                set_motor(130,-130);vTaskDelay(800/portTICK_PERIOD_MS);
                front_dist=9999;
            }else set_motor(120,120);
        }vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void lidar_task(void*a){
    uint8_t d[128],pk[22];int idx=0;bool inp=false;int m=9999;
    while(1){
        int l=uart_read_bytes(UART_NUM,d,128,20/portTICK_PERIOD_MS);
        if(l>0){
            for(int i=0;i<l;i++){
                if(!inp&&d[i]==0xFA){inp=true;idx=0;}
                if(inp){pk[idx++]=d[i];if(idx==22){inp=false;
                    if(pk[1]>=0xA0&&pk[1]<=0xF9){
                        int ba=(pk[1]-0xA0)*4;
                        for(int j=0;j<4;j++){
                            uint16_t dist=(pk[5+j*4]<<8)|pk[4+j*4];
                            int ang=(ba+j)%360;lidar_map[ang]=dist;
                            if(ang>=330||ang<=30)if(dist>50&&dist<m)m=dist;
                            if(ang==45){front_dist=m;m=9999;}
                        }
                    }
                }}
            }
        }
    }
}

// --- HTTP SERVER ---
esp_err_t h_map(httpd_req_t*r){
    char*b=malloc(3000);int c=0;
    for(int i=0;i<360;i++)c+=sprintf(b+c,"%d,",lidar_map[i]);
    httpd_resp_send(r,b,strlen(b));free(b);return ESP_OK;
}
esp_err_t h_root(httpd_req_t*r){return httpd_resp_send(r,html_page,HTTPD_RESP_USE_STRLEN);}
esp_err_t h_cmd(httpd_req_t*r){
    if(strstr(r->uri,"/auto"))auto_mode=true;
    else if(strstr(r->uri,"/stop")){auto_mode=false;set_motor(0,0);}
    else if(strstr(r->uri,"/fwd")){auto_mode=false;set_motor(130,130);}
    else if(strstr(r->uri,"/left")){auto_mode=false;set_motor(-130,130);}
    else if(strstr(r->uri,"/right")){auto_mode=false;set_motor(130,-130);}
    else if(strstr(r->uri,"/bwd")){auto_mode=false;set_motor(-130,-130);}
    return httpd_resp_send(r,"OK",2);
}

void app_main(){
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG,0);nvs_flash_init();init_motors();
    uart_config_t uc={115200,UART_DATA_8_BITS,UART_PARITY_DISABLE,UART_STOP_BITS_1,UART_HW_FLOWCTRL_DISABLE};
    uart_driver_install(UART_NUM,2048,0,0,NULL,0);uart_param_config(UART_NUM,&uc);
    uart_set_pin(UART_NUM,17,16,-1,-1);
    const char*s="startlds$";uart_write_bytes(UART_NUM,s,9);
    xTaskCreate(lidar_task,"l",4096,NULL,10,NULL);xTaskCreate(auto_task,"a",2048,NULL,5,NULL);
    esp_netif_init();esp_event_loop_create_default();esp_netif_create_default_wifi_ap();
    wifi_init_config_t cf=WIFI_INIT_CONFIG_DEFAULT();esp_wifi_init(&cf);
    wifi_config_t wc={.ap={.ssid=WIFI_SSID,.ssid_len=strlen(WIFI_SSID),.password=WIFI_PASS,.max_connection=4,.authmode=WIFI_AUTH_WPA2_PSK}};
    esp_wifi_set_mode(WIFI_MODE_AP);esp_wifi_set_config(WIFI_IF_AP,&wc);esp_wifi_start();
    httpd_handle_t sv=NULL;httpd_config_t c=HTTPD_DEFAULT_CONFIG();
    if(httpd_start(&sv,&c)==ESP_OK){
        httpd_uri_t u1={"/",HTTP_GET,h_root,0},u2={"/map",HTTP_GET,h_map,0},u3={"/auto",HTTP_GET,h_cmd,0},
        u4={"/stop",HTTP_GET,h_cmd,0},u5={"/fwd",HTTP_GET,h_cmd,0},u6={"/left",HTTP_GET,h_cmd,0},
        u7={"/right",HTTP_GET,h_cmd,0},u8={"/bwd",HTTP_GET,h_cmd,0};
        httpd_register_uri_handler(sv,&u1);httpd_register_uri_handler(sv,&u2);httpd_register_uri_handler(sv,&u3);
        httpd_register_uri_handler(sv,&u4);httpd_register_uri_handler(sv,&u5);httpd_register_uri_handler(sv,&u6);
        httpd_register_uri_handler(sv,&u7);httpd_register_uri_handler(sv,&u8);
    }
}