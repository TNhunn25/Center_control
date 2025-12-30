#include <Arduino.h>

// Input buttons (nên dùng INPUT_PULLUP, nút kéo xuống GND)
static const uint8_t BTN_PINS[4] = {16, 17, 18, 19};

// Output relays / SSR / MOSFET driver
static const uint8_t OUT_PINS[4] = {25, 26, 27, 32};

// Status LEDs
static const uint8_t LED_OK_PIN = 2;  // xanh
static const uint8_t LED_ERR_PIN = 4; // đỏ

// Nếu relay board active-low thì để true, còn active-high thì false
static const bool OUT_ACTIVE_LOW = false;

// ====== Debounce settings ======
static const uint32_t DEBOUNCE_MS = 35;

// ====== State ======
struct ButtonState
{
    bool stableLevel; // mức ổn định hiện tại (true=HIGH)
    bool lastReading;
    uint32_t lastChangeMs;
    bool pressedEvent; // event nhấn (edge)
};

ButtonState btn[4];
bool outState[4] = {false, false, false, false}; // true=ON

// ====== Helpers ======
void writeOutput(uint8_t ch, bool on)
{
    outState[ch] = on;
    bool pinLevel = on;
    if (OUT_ACTIVE_LOW)
        pinLevel = !pinLevel;
    digitalWrite(OUT_PINS[ch], pinLevel ? HIGH : LOW);
}

void toggleOutput(uint8_t ch)
{
    writeOutput(ch, !outState[ch]);
}

bool debounceButton(uint8_t i)
{
    // returns true nếu có event "nhấn"
    bool reading = digitalRead(BTN_PINS[i]); // INPUT_PULLUP: nhấn = LOW
    uint32_t now = millis();

    if (reading != btn[i].lastReading)
    {
        btn[i].lastChangeMs = now;
        btn[i].lastReading = reading;
    }

    if (now - btn[i].lastChangeMs > DEBOUNCE_MS)
    {
        if (btn[i].stableLevel != reading)
        {
            btn[i].stableLevel = reading;

            // Detect press event: HIGH -> LOW (vì pull-up)
            if (btn[i].stableLevel == LOW)
            {
                return true;
            }
        }
    }
    return false;
}

void setStatusLed(bool ok, bool err)
{
    digitalWrite(LED_OK_PIN, ok ? HIGH : LOW);
    digitalWrite(LED_ERR_PIN, err ? HIGH : LOW);
}

// ====== Setup/Loop ======
void setup()
{
    Serial.begin(115200);

    // Inputs
    for (int i = 0; i < 4; i++)
    {
        pinMode(BTN_PINS[i], INPUT_PULLUP);
        btn[i].stableLevel = digitalRead(BTN_PINS[i]);
        btn[i].lastReading = btn[i].stableLevel;
        btn[i].lastChangeMs = millis();
    }

    // Outputs
    for (int i = 0; i < 4; i++)
    {
        pinMode(OUT_PINS[i], OUTPUT);
        writeOutput(i, false); // OFF
    }

    // LEDs
    pinMode(LED_OK_PIN, OUTPUT);
    pinMode(LED_ERR_PIN, OUTPUT);
    setStatusLed(true, false);

    Serial.println("Controller started.");
}

void loop()
{
    // 1) đọc input -> tạo event
    for (int i = 0; i < 4; i++)
    {
        if (debounceButton(i))
        {
            Serial.printf("Button %d pressed -> toggle output %d\n", i, i);
            toggleOutput(i);
        }
    }

    // 2) ví dụ logic: nếu output nào ON thì LED_OK nháy
    bool anyOn = false;
    for (int i = 0; i < 4; i++)
        if (outState[i])
            anyOn = true;
    setStatusLed(anyOn, false);

    delay(5);
}