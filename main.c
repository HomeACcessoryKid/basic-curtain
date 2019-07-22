/*  (c) 2018 HomeAccessoryKid
 *  This example drives a basic curtain motor.
 *  It uses any ESP8266 with as little as 1MB flash. 
 *  GPIO-0 reads a button for manual instructions
 *  GPIO-2 instructs a relay to drive the motor
 *  GPIO-4 instructs the direction=polarity of the motor by means of two relays: up or down
 *  obviously your own motor setup might be using different ways of providing these functions
 *  a HomeKit custom integer value can be set to define the time needed for 100% travel
 *  this will be interpolated to set values between 0% and 100%
 *  UDPlogger is used to have remote logging
 *  LCM is enabled in case you want remote updates
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
//#include <espressif/esp_system.h> //for timestamp report only
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <string.h>
#include "lwip/api.h"

#include <udplogger.h>
#include "button.h"
#ifndef BUTTON_PIN
#error BUTTON_PIN is not specified
#endif
#ifndef MOVE_PIN
#error MOVE_PIN is not specified
#endif
#ifndef DIR_PIN
#error DIR_PIN is not specified
#endif

#define BEAT 500  //ms

/* ============== BEGIN HOMEKIT CHARACTERISTIC DECLARATIONS =============================================================== */
int transittime=14, intervalk;

// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");

// next use these two lines before calling homekit_server_init(&config);
//    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
//                                      &model.value.string_value,&revision.value.string_value);
//    config.accessories[0]->config_number=c_hash;
// end of OTA add-in instructions

homekit_value_t target_get();
void target_set(homekit_value_t value);
homekit_characteristic_t target       = HOMEKIT_CHARACTERISTIC_(TARGET_POSITION,  0, .getter=target_get, .setter=target_set);
homekit_characteristic_t current      = HOMEKIT_CHARACTERISTIC_(CURRENT_POSITION, 0);
homekit_characteristic_t state        = HOMEKIT_CHARACTERISTIC_(POSITION_STATE,   2);


homekit_value_t target_get() {
    return HOMEKIT_UINT8(target.value.int_value);
}
void target_set(homekit_value_t value) {
    if (value.format != homekit_format_uint8) {
        UDPLGP("Invalid target-value format: %d\n", value.format);
        return;
    }
    UDPLGP("T:%3d\n",value.int_value);
    target.value=value;
}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_TRANSIT HOMEKIT_CUSTOM_UUID("F0000006")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_TRANSIT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_TRANSIT, \
    .description = "TransitTime (s)", \
    .format = homekit_format_uint8, \
    .min_value=(float[])  {1}, \
    .max_value=(float[]) {60}, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

homekit_value_t transit_get() {
    return HOMEKIT_UINT8(transittime);
}
void transit_set(homekit_value_t value) {
    if (value.format != homekit_format_uint8) {
        UDPLGP("Invalid transit-value format: %d\n", value.format);
        return;
    }
    transittime = value.int_value;
    UDPLGP("Transit: %d\n", transittime);
    intervalk=100*BEAT/transittime; //unit is 0.001% because BEAT is in ms and transittime in s
}
homekit_characteristic_t transit=HOMEKIT_CHARACTERISTIC_(CUSTOM_TRANSIT,0,.getter=transit_get,.setter=transit_set);

// void identify_task(void *_args) {
//     vTaskDelete(NULL);
// }

void identify(homekit_value_t _value) {
    UDPLGP("Identify\n");
//    xTaskCreate(identify_task, "identify", 256, NULL, 2, NULL);
}

/* ============== END HOMEKIT CHARACTERISTIC DECLARATIONS ================================================================= */


void state_task(void *argv) {
    int direction=0,move=0,currentk,deltak=0;
    
    vTaskDelay(500); //TODO prevent that this works before homekit is initialized
    currentk=current.value.int_value*1000;
    while(1) {
        vTaskDelay(BEAT/portTICK_PERIOD_MS);
        UDPLOG("Dk=%7d, Ck=%7d, C=%3d, T=%3d, S=%d, move=%d, dir=%2d\n",deltak,currentk,current.value.int_value,target.value.int_value,state.value.int_value,move,direction);
        if (current.value.int_value!=target.value.int_value) { //need to move
            direction=current.value.int_value<target.value.int_value ? 1 : -1;
            deltak=target.value.int_value*1000-currentk;
            if ((direction*deltak)>intervalk) currentk+=intervalk*direction; else currentk=target.value.int_value*1000;
            current.value.int_value=(currentk+500)/1000;
            move=1;
            state.value.int_value=direction>0 ? 1 : 0;
            homekit_characteristic_notify(&state,  HOMEKIT_UINT8(  state.value.int_value));
            homekit_characteristic_notify(&current,HOMEKIT_UINT8(current.value.int_value));
        } else { //arrived
            move=0;
            direction=0;
            if (state.value.int_value!=2){
                state.value.int_value =2; //stopped
                homekit_characteristic_notify(&state,  HOMEKIT_UINT8(  state.value.int_value));
                homekit_characteristic_notify(&current,HOMEKIT_UINT8(current.value.int_value));
            }            
        }
        gpio_write( DIR_PIN, direction>0 ? 1 : 0);
        gpio_write(MOVE_PIN, move ? 1 : 0);
    }
}

void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            UDPLGP("single press\n");
            target.value.int_value=0;//set target=0
            homekit_characteristic_notify(&target,HOMEKIT_UINT8(target.value.int_value));
            break;
        case button_event_double_press:
            UDPLGP("double press\n");
            target.value.int_value=100;//set target=100
            homekit_characteristic_notify(&target,HOMEKIT_UINT8(target.value.int_value));
            break;
        case button_event_long_press:
            UDPLGP("long press\n");
            target.value.int_value=current.value.int_value;//set target=current
            homekit_characteristic_notify(&target,HOMEKIT_UINT8(target.value.int_value));
            break;
        default:
            UDPLGP("unknown button event: %d\n", event);
    }
}

void motor_init() {
    if (button_create(BUTTON_PIN, button_callback)) UDPLGP("Failed to initialize button\n");
    gpio_enable(MOVE_PIN, GPIO_OUTPUT); gpio_write(MOVE_PIN, 0);
    gpio_enable( DIR_PIN, GPIO_OUTPUT); gpio_write( DIR_PIN, 0);
    intervalk=100*BEAT/transittime;
    xTaskCreate(state_task, "State", 512, NULL, 1, NULL);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_window_covering,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "basic-curtain"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Curtain"),
                    &target,
                    &current,
                    &state,
                    &transit,
                    &ota_trigger,
                    NULL
                }),
            NULL
        }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void user_init(void) {
    uart_set_baud(0, 230400);
    udplog_init(3);
    UDPLOG("\n\n\n\nBasic Curtain Motor\n");

    motor_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=3; revision.value.string_value="0.0.3"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}
