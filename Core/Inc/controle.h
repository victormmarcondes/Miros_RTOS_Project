#pragma once

extern "C"{
    #include "VL53L4CD_api.h"
}

class Controle{
    public:
    static inline const float KR = 1.00f; //kp
    static inline const float TI = 1.80f; //tempo integrador
    static inline const float TD = 0.00f; //tempo derivador, 0 pq o sensor do arduino tem muito erro.
    static inline const float T = 0.04f; // do matheus é 50ms, mas o do gian está pedindo a cada 40ms

    static inline const float U_MAX = 4.00f; //saturação saida

    //coeficientes
    static inline const float q0 = KR * (1.0f + (T/TI));
    static inline const float q1 = KR * (-1.0f);

    private:
    static inline volatile float set_point = 200.0f; //altura
    static inline volatile float dist_sensor = 0.0f; //leitura sensor
    static inline volatile float tensao_pwm = 0.0f;  //pwm
    static inline Dev_t sensor;

    //passado
    static inline float e_k1;
    static inline float u_k1;

    public:
    static void init();
    static void TaskControle();
    static void TaskAtuador();
    static void SetSetPoint(float setpoint);
    static void SetDistancia(float distancia){dist_sensor += distancia;}
    static float GetTension(){return tensao_pwm;}
    static void TaskLeitor();
};
