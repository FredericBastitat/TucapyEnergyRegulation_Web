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
    delayMicroseconds(1000);
    digitalWrite(DE_RE_PIN, LOW); 
}

// Šablona pro čtení registrů (podporuje Holding 0x03 i Input 0x04)
template<typename T>
bool readRegister(uint16_t reg_addr, uint8_t reg_count, T &value, const char* name, bool isInput = false)
{
    uint8_t res = isInput ? node.readInputRegisters(reg_addr, reg_count) : node.readHoldingRegisters(reg_addr, reg_count);
    if (res == node.ku8MBSuccess)
    {
        if (reg_count == 1)
            value = (T)node.getResponseBuffer(0);
        else if (reg_count == 2)
            value = (T)(((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1));
        return true;
    }
    webLog("Chyba " + String(name) + ": 0x" + String(res, HEX));
    value = 0;
    return false;
}

bool readBatteryData()
{
    bool ok = true;

    // 1. Výkon (Holding)
    int32_t raw_P = 0;
    if (readRegister<int32_t>(REG_BATTERY_POWER, 2, raw_P, "BP")) {
        battery_P = raw_P / 1000.0;
    } else ok = false;

    delay(100); 

    // 2. SOC (Holding - změněno z Input kvůli chybě 0xe2)
    uint16_t rawSOC = 0;
    if (readRegister<uint16_t>(REG_SOC, 1, rawSOC, "SOC")) { 
        battery_soc = rawSOC / 1.0; // Většina měničů vrací přímo %
    } else ok = false;

    delay(100);

    // 3. Proud baterie (Holding)
    int16_t rawI = 0;
    if (readRegister<int16_t>(REG_BATTERY_I, 1, rawI, "BI")) {
        battery_I = (rawI / 100.0) * -1.0;
    } else ok = false;

    delay(100);

    // 4. Proud sítě (Holding - změněno z Input kvůli chybě 0xe2)
    int32_t rawGrid = 0;
    if (readRegister<int32_t>(REG_GRID_I, 2, rawGrid, "GI")) {
        grid_I = (rawGrid / 1000.0) * -1.0;
    } else ok = false;

    return ok;
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
        
        webLog("Zkouším číst měnič...");

        if (readBatteryData()) {
            status_msg = "SPOJENO OK.";
            webLog(">>> Modbus komunikace OK!");
            webLog("P: " + String(battery_P) + " kW, SOC: " + String(battery_soc) + " %, I: " + String(battery_I) + " A, Grid: " + String(grid_I) + " A");
            return true;
        } else {
            status_msg = "Chyba komunikace";
            webLog("Neodpovídá. Zkontrolovat zapojeni.");
            return false;
        }
    }
    return false; // OK, ale bez nového čtení
}

} // namespace ModbusHandler
