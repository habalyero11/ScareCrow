/*
 * ScareCrow MINIMAL TEST v2
 * 
 * Testing without the brownout disable register code
 * (which may be incompatible with newer ESP32 Arduino core)
 */

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\n\n====================================");
    Serial.println("   MINIMAL TEST v2 - NO BOD DISABLE");
    Serial.println("====================================");
    Serial.println("If you see this, the basic ESP32 works.");
    Serial.println("====================================\n");
}

void loop() {
    Serial.println("Tick...");
    delay(2000);
}
