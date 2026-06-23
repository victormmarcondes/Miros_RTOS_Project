#include "controle.h"
#include "miros.h"
#include <cstdint>

void Controle::ProximaAtuacao(){
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

void Controle::TaskControle(void* parametros){
    //const uint32_t periodo_40ms = 40; //tick
    uint32_t tempo_inicio = rtos::global_tick;

    //ler
    /*__disable_irq();
    float alvo = set_point;
    float medida_sensor = dist_sensor;
    __enable_irq();
    //calculo*/
    ProximaAtuacao();

    /*__disable_irq();
    tensao_pwm = nova_tensao;
    __enable_irq();*/
        
    uint32_t tempo_atual = rtos::global_tick;
    uint32_t tempo_gasto = tempo_atual - tempo_inicio;
    if(rtos::OS_curr->deadline < 40){
        rtos::OS_delay(40 - tempo_gasto);
    }
    else{
        rtos::OS_delay(1U);
    }
    tempo_inicio = rtos::global_tick;
    return;
}

void Controle::SetSetPoint(float setpoint){
    if(set_point + setpoint > 600){
        set_point = (int) (set_point + setpoint) % 600;
    } else {set_point += setpoint;}
}
