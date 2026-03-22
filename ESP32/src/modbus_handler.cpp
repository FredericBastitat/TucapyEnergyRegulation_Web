#include "modbus_handler.h"
#include "logger.h"

namespace ModbusHandler {

ModbusMaster node;

float battery_P = 0, battery_soc = 0, battery_I = 0, grid_I = 0;
String status_msg = "Inicializace Modbus...";

// Interní pomocné funkce pro přepínání DE/RE
void preTransmission() { 
    digitalWrite(DE_RE_PIN, HIGH); 
    delayMicroseconds(1000); 
}
void postTransmission() { 
    Serial2.flush();
    delayMicroseconds(1000);
    digitalWrite(DE_RE_PIN, LOW); 
}

// Čtení registru s retry (max 2 pokusy)
template<typename T>
bool readRegister(uint16_t reg_addr, uint8_t reg_count, T &value, const char* name)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        uint8_t res = node.readHoldingRegisters(reg_addr, reg_count);
        if (res == node.ku8MBSuccess) {
            if (reg_count == 1)
                value = (T)node.getResponseBuffer(0);
            else if (reg_count == 2)
                value = (T)(((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1));
            return true;
        }
        if (attempt == 0) {
            delay(50); // Pauza před retry
        } else {
            webLog("Chyba " + String(name) + ": 0x" + String(res, HEX));
        }
    }
    // NEMAZEME value – ponecháme poslední známou hodnotu
    return false;
}

bool readBatteryData()
{
    int errors = 0;

    // 1. Výkon
    int32_t raw_P = 0;
    if (readRegister<int32_t>(REG_BATTERY_POWER, 2, raw_P, "BP")) {
        battery_P = (raw_P / 1000.0) * -1.0;
    } else errors++;

    delay(100); 

    // 2. SOC
    uint16_t rawSOC = 0;
    if (readRegister<uint16_t>(REG_SOC, 1, rawSOC, "SOC")) { 
        battery_soc = rawSOC / 100.0;
    } else errors++;

    delay(100);

    // 3. Proud baterie
    int16_t rawI = 0;
    if (readRegister<int16_t>(REG_BATTERY_I, 1, rawI, "BI")) {
        battery_I = (rawI / 100.0) * -1.0;
    } else errors++;

    delay(100);

    // 4. Proud sítě
    int32_t rawGrid = 0;
    if (readRegister<int32_t>(REG_GRID_I, 2, rawGrid, "GI")) {
        grid_I = (rawGrid / 1000.0) * -1.0;
    } else errors++;

    // OK pokud se podařilo přečíst alespoň 1 registr
    return errors < 4;
}


void setup() {
    Serial2.begin(9600, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);
  
    node.begin(SLAVE_ID, Serial2);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
}

bool update() {
    static unsigned long lastRead = 0;
    if (millis() - lastRead > 3000) {
        lastRead = millis();

        if (readBatteryData()) {
            status_msg = "SPOJENO OK.";
            webLog("P: " + String(battery_P) + " kW, SOC: " + String(battery_soc) + " %, I: " + String(battery_I) + " A, Grid: " + String(grid_I) + " A");
            return true;
        } else {
            status_msg = "Odpojeno";
            webLog("Vse selhalo. Zkontrolovat zapojeni.");
            return false;
        }
    }
    return false;
}

} // namespace ModbusHandler
