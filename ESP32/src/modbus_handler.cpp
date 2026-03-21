#include "modbus_handler.h"
#include "logger.h"

namespace ModbusHandler {

ModbusMaster node;

float battery_P = 0, battery_soc = 0, battery_I = 0, grid_I = 0;
String status_msg = "Inicializace Modbus...";

// Interní pomocné funkce pro přepínání DE/RE
void preTransmission() { 
    digitalWrite(DE_RE_PIN, HIGH); 
    delayMicroseconds(500); 
}
void postTransmission() { 
    delayMicroseconds(500);
    digitalWrite(DE_RE_PIN, LOW); 
}

// Šablona pro čtení registrů (používá se pro SOC a případně jiné jednotlivé čtení)
template<typename T>
bool readRegister(uint16_t reg_addr, uint8_t reg_count, T &value, const char* name)
{
    uint8_t res = node.readHoldingRegisters(reg_addr, reg_count);
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

// Hlavní čtení všech dat najednou
bool readBatteryData()
{
    bool ok = true;
    
    // Blok 1: 30258-30263 (Výkon, Proud baterie, Proud sítě)
    uint8_t res = node.readHoldingRegisters(REG_BATTERY_POWER, 6);
    if (res == node.ku8MBSuccess) {
        int32_t raw_P = (int32_t)(((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1));
        battery_P = raw_P / 1000.0;
        
        int16_t rawI_bat = (int16_t)node.getResponseBuffer(2);
        battery_I = (rawI_bat / 10.0) * -1.0;
        
        int32_t raw_Grid = (int32_t)(((uint32_t)node.getResponseBuffer(4) << 16) | node.getResponseBuffer(5));
        grid_I = (raw_Grid / 1000.0) * -1.0;
    } else {
        webLog("Chyba bloku 30k: 0x" + String(res, HEX));
        ok = false; 
    }

    delay(100); // Pauza mezi požadavky

    // Blok 2: SOC (33000)
    uint16_t rawSOC = 0;
    if (readRegister<uint16_t>(REG_SOC, 1, rawSOC, "SOC")) {
        battery_soc = rawSOC / 100.0;
    } else {
        ok = false;
    }

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
