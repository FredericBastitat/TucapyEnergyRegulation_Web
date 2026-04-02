#pragma once
#include <Arduino.h>
#include <ModbusMaster.h>

namespace ModbusHandler {

// Comunication pins
#define MODBUS_RX_PIN 16
#define MODBUS_TX_PIN 17
#define DE_RE_PIN 18 

// Registry
#define SLAVE_ID 247
#define REG_BATTERY_POWER 30258
#define REG_BATTERY_I     30255
#define REG_GRID_I        11000
#define REG_SOC           33000

// Data
extern float battery_P;
extern float battery_soc;
extern float battery_I;
extern float grid_I;
extern String status_msg;

// Funkce
void setup();
bool update();

} // namespace ModbusHandler
