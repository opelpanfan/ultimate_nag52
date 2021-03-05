//
// Created by ashcon on 2/20/21.
//

#include "car_sim.h"
#include "../ecus/can_frame.h"
#include "esp32_forwarder.h"

#include <utility>
#include <cstring>

#include "EZS_A5.h"
EZS_A5 ezsa5; // For variant testing!

std::string fmt_frame(CAN_FRAME *f) {
    char buf[150] = {0x00};
    int pos = sprintf(buf, "0x%04X - [", f->id);
    for (int i = 0; i < f->length; i++) {
        pos += sprintf(buf+pos, "%02X ", f->data.bytes[i]);
    }
    buf[pos-1] = ']';
    return std::string(buf);
}

void CAR_SIMULATOR::init() {
    this->esp_ecu = new class abs_esp();
    this->nag52_ecu = new class nag52();
    this->eng_ecu = new class engine();
    this->ewm_ecu = new class ewm();

    this->ecus.push_back((abstract_ecu*)this->esp_ecu);
    this->ecus.push_back((abstract_ecu*)this->nag52_ecu);
    this->ecus.push_back((abstract_ecu*)this->eng_ecu);
    this->ecus.push_back((abstract_ecu*)this->ewm_ecu);

    for (auto x : this->ecus){
        x->setup();
    }

    printf("Init with %lu ecus!\n", ecus.size());
    this->thread_exec = true;
    this->can_thread = std::thread(&CAR_SIMULATOR::can_sim_thread, this);
    this->sim_thread = std::thread(&CAR_SIMULATOR::ecu_sim_thread, this);
}

void CAR_SIMULATOR::can_sim_thread() {
    printf("CAN sender thread starting!\n");
    CAN_FRAME tx;
    ezsa5.raw = 0x020024062D18181A;
    art258.raw =0x0000000000000000;

    ezsa5.set_NAG_VH(true);
    ezsa5.set_KSG_VH(false);
    //art258.set_ART_ERR(0);
    //art258.set_ART_INFO(true);
    ////art258.set_ART_WT(true);
    //art258.set_S_OBJ(true);
    //art258.set_ART_SEG_EIN(true);
    ////art258.set_ART_EIN(true);
    //art258.set_ASSIST_FKT_AKT(3);
    //art258.set_ASSIST_ANZ_V2(13);
    //art258.set_V_ART(50);
    //art258.set_ART_DSPL_EIN(true);
    //art258.set_OBJ_ERK(true);
    ////art258.set_ABST_R_OBJ(100); // In yards
    //art258.set_V_ZIEL(10);
    //art258.set_ART_ABW_AKT(true);
    //art258.set_AAS_LED_BL(true);
    //art258.set_OBJ_AGB(true);
    //art258.set_CAS_ERR_ANZ_V2(1);

    int ticks = 0;
    while(this->thread_exec) {
        ticks++;
        if (ticks >= 10) {
            ticks = 0;
            // Send EZS frame to keep IC thinking engine is running
            TX_FRAME(ezsa5)
        }
        // Send frames in order of ID
        TX_FRAME(ms210)
        TX_FRAME(ms212)
        TX_FRAME(ms268)
        TX_FRAME(ms2F3)
        TX_FRAME(ms308)
        //TX_FRAME(ms312)
        TX_FRAME(ms608)
        TX_FRAME(gs218)
        TX_FRAME(gs338)
        TX_FRAME(gs418)
        TX_FRAME(bs200)
        TX_FRAME(bs208)
        TX_FRAME(bs270)
        TX_FRAME(bs300)
        TX_FRAME(bs328)
        TX_FRAME(art258)
        TX_FRAME(ewm230)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CAR_SIMULATOR::ecu_sim_thread() {
    printf("ECU Simulator thread starting!\n");
    while(this->thread_exec) {
        for (auto ecu: this->ecus) {
            ecu->simulate_tick();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CAR_SIMULATOR::terminate() {
    this->thread_exec = false;
    this->can_thread.join();
    this->sim_thread.join();
    if (this->send_to_esp) {
        this->esp32.close_port();
    }
    CLEAR_FRAME(ms210)
    CLEAR_FRAME(ms212)
    CLEAR_FRAME(ms268)
    CLEAR_FRAME(ms2F3)
    CLEAR_FRAME(ms308)
    CLEAR_FRAME(ms312)
    CLEAR_FRAME(ms608)
    CLEAR_FRAME(gs218)
    CLEAR_FRAME(gs338)
    CLEAR_FRAME(gs418)
    CLEAR_FRAME(bs200)
    CLEAR_FRAME(bs208)
    CLEAR_FRAME(bs270)
    CLEAR_FRAME(bs300)
    CLEAR_FRAME(bs328)
}

void CAR_SIMULATOR::bcast_frame(CAN_FRAME *f) {
    if (this->send_to_esp) {
        this->esp32.send_frame(f);
    }
}

CAR_SIMULATOR::CAR_SIMULATOR(char *port_name) {
    if (port_name != nullptr) {
        this->esp32 = esp32_forwarder(port_name);
        if (this->esp32.is_port_open()) {
            this->send_to_esp = true;
        }
    } else {
        this->send_to_esp = false;
    }
}

abs_esp* CAR_SIMULATOR::get_abs() {
    return this->esp_ecu;
}

ewm* CAR_SIMULATOR::get_ewm() {
    return this->ewm_ecu;
}

engine* CAR_SIMULATOR::get_engine() {
    return this->eng_ecu;
}

nag52* CAR_SIMULATOR::get_nag52() {
    return this->nag52_ecu;
}