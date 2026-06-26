#include "controle.h"
#include "miros.h"
#include "main.h"

#include "stm32g4xx_hal.h"
#include <cstdint>

void Controle::init(){
    VL53L4CD_SensorInit(sensor);
    VL53L4CD_StartRanging(sensor);
}

void Controle::TaskControle(){

    float e_k = set_point - dist_sensor;
    float u_k = u_k1 + (q0 * e_k) + (q1 * e_k1);
    //saturação
    if(u_k > U_MAX){
        u_k = U_MAX;
    } else if(u_k < 0.0f){
        u_k = 0.0f;
    }
    float u_sat = u_k;
    if(u_sat > 0.0f && u_sat < 0.20f){
        u_sat = 0.20f;
    }
    u_k1 = u_sat;
    e_k1 = e_k;

    tensao_pwm = u_sat;

    return;
}

void Controle::TaskLeitor(){
    uint8_t ready = 0;
    VL53L4CD_ResultsData_t result;

    VL53L4CD_CheckForDataReady(sensor, &ready);

    if(ready){ 
        VL53L4CD_GetResult(sensor, &result);
        dist_sensor = result.distance_mm;
        VL53L4CD_SensorInit(sensor);
    }
    return;
}

void Controle::SetSetPoint(float setpoint){
    if(set_point + setpoint > 600){
        set_point = (int) (set_point + setpoint) % 600;
    } else {set_point += setpoint;}
}
