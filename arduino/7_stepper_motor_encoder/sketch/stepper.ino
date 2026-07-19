#include <AccelStepper.h>

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ============================================================
// Pin configuration
// ============================================================

constexpr uint8_t STEP_PIN = 9;
constexpr uint8_t DIR_PIN = 8;
constexpr uint8_t ENABLE_PIN = 7;

constexpr uint8_t ENCODER_A_PIN = 2;
constexpr uint8_t ENCODER_B_PIN = 3;

// Arduino Mega 2560:
// Digital pin 2 = PE4
// Digital pin 3 = PE5
constexpr uint8_t ENCODER_A_BIT = PE4;
constexpr uint8_t ENCODER_B_BIT = PE5;

// ============================================================
// Driver configuration
// ============================================================

constexpr bool ENABLE_ACTIVE_LOW = true;

// Change to true if motor direction is opposite to the command.
constexpr bool INVERT_MOTOR_DIRECTION = false;

// Change through SET_ENCODER_SIGN if encoder direction is reversed.
int8_t encoderDirectionSign = 1;

// Conservative DM542 STEP pulse width.
constexpr unsigned int STEP_PULSE_WIDTH_US = 5;

// ============================================================
// Mechanical configuration
// ============================================================

// Must match the DM542 microstep DIP-switch configuration.
long motorPulsesPerRevolution = 800;

// E38S6G5-1000B:
// 1000 pulses/revolution × 4 quadrature transitions
// = normally 4000 counts/revolution.
long encoderCountsPerRevolution = 4000;

// Maximum final position difference in encoder counts.
long positionErrorTolerance = 40;

float maximumRpm = 600.0F;
float motorAcceleration = 2000.0F;

// ============================================================
// Serial configuration
// ============================================================

constexpr uint32_t SERIAL_BAUD_RATE = 115200;

constexpr size_t COMMAND_BUFFER_SIZE = 100;

char commandBuffer[COMMAND_BUFFER_SIZE];
size_t commandIndex = 0;

unsigned long telemetryIntervalMs = 100;
unsigned long previousTelemetryTime = 0;

// ============================================================
// Motor object
// ============================================================

AccelStepper stepper(
    AccelStepper::DRIVER,
    STEP_PIN,
    DIR_PIN
);

// ============================================================
// Encoder variables
// ============================================================

volatile long encoderRawCount = 0;
volatile uint8_t previousEncoderState = 0;
volatile unsigned long encoderInvalidTransitions = 0;

// Quadrature transition table.
//
// Index:
// previous AB state in bits 3:2
// current AB state in bits 1:0
//
// Valid transitions return +1 or -1.
// Invalid transitions return 0.
constexpr int8_t QUADRATURE_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

// ============================================================
// Controller state
// ============================================================

enum class ControllerState {
    IDLE,
    RUNNING,
    STOPPING,
    ESTOPPED,
    FAULT
};

ControllerState controllerState = ControllerState::IDLE;

bool driverEnabled = false;
bool movementWasActive = false;
bool telemetryEnabled = true;

float requestedRpm = 0.0F;
float measuredRpm = 0.0F;

long requestedMotorPulses = 0;
long finalPositionError = 0;

char requestedDirection[5] = "CW";

// Encoder-speed calculation.
long previousEncoderCount = 0;
unsigned long previousSpeedTime = 0;

// ============================================================
// Encoder functions
// ============================================================

// Fast direct read of Arduino Mega pins 2 and 3.
//
// Pin 2 = PE4 = channel A
// Pin 3 = PE5 = channel B
inline uint8_t readEncoderStateFast()
{
    const uint8_t portValue = PINE;

    const uint8_t channelA =
        (portValue & _BV(ENCODER_A_BIT)) ? 1U : 0U;

    const uint8_t channelB =
        (portValue & _BV(ENCODER_B_BIT)) ? 1U : 0U;

    return static_cast<uint8_t>(
        (channelA << 1U) | channelB
    );
}

void encoderInterrupt()
{
    const uint8_t currentState = readEncoderStateFast();

    const uint8_t transition =
        static_cast<uint8_t>(
            (previousEncoderState << 2U) |
            currentState
        );

    const int8_t movement =
        QUADRATURE_TABLE[transition];

    encoderRawCount += movement;

    // A two-bit state change is physically impossible during
    // normal quadrature operation. It usually indicates noise,
    // a missed interrupt, or a wiring problem.
    const uint8_t changedBits =
        previousEncoderState ^ currentState;

    if (changedBits == 0b11U) {
        encoderInvalidTransitions++;
    }

    previousEncoderState = currentState;
}

long readRawEncoderCount()
{
    noInterrupts();

    const long count = encoderRawCount;

    interrupts();

    return count;
}

long readEncoderPosition()
{
    return readRawEncoderCount() * encoderDirectionSign;
}

void writeEncoderPosition(long position)
{
    noInterrupts();

    encoderRawCount =
        position * static_cast<long>(encoderDirectionSign);

    previousEncoderState = readEncoderStateFast();

    interrupts();

    previousEncoderCount = position;
}

unsigned long readInvalidTransitionCount()
{
    noInterrupts();

    const unsigned long count =
        encoderInvalidTransitions;

    interrupts();

    return count;
}

void clearInvalidTransitionCount()
{
    noInterrupts();

    encoderInvalidTransitions = 0;

    interrupts();
}

// ============================================================
// General utility functions
// ============================================================

const char *stateToText(ControllerState state)
{
    switch (state) {
        case ControllerState::IDLE:
            return "IDLE";

        case ControllerState::RUNNING:
            return "RUNNING";

        case ControllerState::STOPPING:
            return "STOPPING";

        case ControllerState::ESTOPPED:
            return "ESTOPPED";

        case ControllerState::FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}

void convertToUppercase(char *text)
{
    while (*text != '\0') {
        *text = static_cast<char>(
            toupper(static_cast<unsigned char>(*text))
        );

        ++text;
    }
}

float rpmToPulsesPerSecond(float rpm)
{
    return (
        rpm *
        static_cast<float>(motorPulsesPerRevolution)
    ) / 60.0F;
}

float encoderCountsToRevolutions(long counts)
{
    if (encoderCountsPerRevolution <= 0) {
        return 0.0F;
    }

    return (
        static_cast<float>(counts) /
        static_cast<float>(encoderCountsPerRevolution)
    );
}

float motorPulsesToRevolutions(long pulses)
{
    if (motorPulsesPerRevolution <= 0) {
        return 0.0F;
    }

    return (
        static_cast<float>(pulses) /
        static_cast<float>(motorPulsesPerRevolution)
    );
}

long expectedEncoderPosition()
{
    if (motorPulsesPerRevolution <= 0) {
        return 0;
    }

    const double expected =
        static_cast<double>(stepper.currentPosition()) *
        static_cast<double>(encoderCountsPerRevolution) /
        static_cast<double>(motorPulsesPerRevolution);

    return static_cast<long>(lround(expected));
}

long calculatePositionError()
{
    return readEncoderPosition() -
           expectedEncoderPosition();
}

long absoluteLong(long value)
{
    return value < 0 ? -value : value;
}

// ============================================================
// Driver control
// ============================================================

void enableDriver()
{
    digitalWrite(
        ENABLE_PIN,
        ENABLE_ACTIVE_LOW ? LOW : HIGH
    );

    driverEnabled = true;
}

void disableDriver()
{
    digitalWrite(
        ENABLE_PIN,
        ENABLE_ACTIVE_LOW ? HIGH : LOW
    );

    driverEnabled = false;
}

// ============================================================
// Encoder speed measurement
// ============================================================

void updateMeasuredSpeed()
{
    const unsigned long now = millis();
    const unsigned long elapsedMs =
        now - previousSpeedTime;

    if (elapsedMs < telemetryIntervalMs) {
        return;
    }

    const long currentEncoderCount =
        readEncoderPosition();

    const long encoderDifference =
        currentEncoderCount - previousEncoderCount;

    if (
        encoderCountsPerRevolution > 0 &&
        elapsedMs > 0
    ) {
        measuredRpm =
            static_cast<float>(encoderDifference) *
            60000.0F /
            (
                static_cast<float>(
                    encoderCountsPerRevolution
                ) *
                static_cast<float>(elapsedMs)
            );
    } else {
        measuredRpm = 0.0F;
    }

    previousEncoderCount = currentEncoderCount;
    previousSpeedTime = now;
}

// ============================================================
// Telemetry
// ============================================================

void printTelemetry()
{
    const long commandPosition =
        stepper.currentPosition();

    const long targetPosition =
        stepper.targetPosition();

    const long encoderPosition =
        readEncoderPosition();

    const long expectedPosition =
        expectedEncoderPosition();

    const long positionError =
        encoderPosition - expectedPosition;

    Serial.print(F("DATA"));

    Serial.print(F(" STATE="));
    Serial.print(stateToText(controllerState));

    Serial.print(F(" CMD_POS="));
    Serial.print(commandPosition);

    Serial.print(F(" TARGET="));
    Serial.print(targetPosition);

    Serial.print(F(" ENC_POS="));
    Serial.print(encoderPosition);

    Serial.print(F(" EXPECTED_ENC="));
    Serial.print(expectedPosition);

    Serial.print(F(" ERROR="));
    Serial.print(positionError);

    Serial.print(F(" CMD_REV="));
    Serial.print(
        motorPulsesToRevolutions(commandPosition),
        4
    );

    Serial.print(F(" ENC_REV="));
    Serial.print(
        encoderCountsToRevolutions(encoderPosition),
        4
    );

    Serial.print(F(" CMD_RPM="));
    Serial.print(requestedRpm, 2);

    Serial.print(F(" ENC_RPM="));
    Serial.print(measuredRpm, 2);

    Serial.print(F(" DISTANCE="));
    Serial.print(stepper.distanceToGo());

    Serial.print(F(" ENABLED="));
    Serial.print(driverEnabled ? 1 : 0);

    Serial.print(F(" INVALID="));
    Serial.print(readInvalidTransitionCount());

    Serial.print(F(" TOLERANCE="));
    Serial.println(positionErrorTolerance);
}

void updateTelemetry()
{
    if (!telemetryEnabled) {
        return;
    }

    const unsigned long now = millis();

    if (
        now - previousTelemetryTime <
        telemetryIntervalMs
    ) {
        return;
    }

    previousTelemetryTime = now;

    printTelemetry();
}

// ============================================================
// Motion control
// ============================================================

void startMovement(
    long pulses,
    const char *direction,
    float rpm
)
{
    if (controllerState == ControllerState::FAULT) {
        Serial.println(
            F("ERR clear fault before starting movement")
        );
        return;
    }

    if (stepper.isRunning()) {
        Serial.println(
            F("ERR motor is already running")
        );
        return;
    }

    if (pulses <= 0) {
        Serial.println(
            F("ERR pulses must be greater than zero")
        );
        return;
    }

    if (rpm <= 0.0F || rpm > maximumRpm) {
        Serial.print(
            F("ERR rpm must be between 0 and ")
        );
        Serial.println(maximumRpm, 2);
        return;
    }

    bool clockwise;

    if (strcmp(direction, "CW") == 0) {
        clockwise = true;
    } else if (strcmp(direction, "CCW") == 0) {
        clockwise = false;
    } else {
        Serial.println(
            F("ERR direction must be CW or CCW")
        );
        return;
    }

    if (INVERT_MOTOR_DIRECTION) {
        clockwise = !clockwise;
    }

    enableDriver();

    requestedRpm = rpm;
    requestedMotorPulses = pulses;

    strncpy(
        requestedDirection,
        direction,
        sizeof(requestedDirection) - 1
    );

    requestedDirection[
        sizeof(requestedDirection) - 1
    ] = '\0';

    const float pulseFrequency =
        rpmToPulsesPerSecond(rpm);

    stepper.setMaxSpeed(pulseFrequency);
    stepper.setAcceleration(motorAcceleration);

    const long signedPulses =
        clockwise ? pulses : -pulses;

    stepper.move(signedPulses);

    controllerState = ControllerState::RUNNING;
    movementWasActive = true;
    finalPositionError = 0;

    Serial.print(F("OK MOVE"));

    Serial.print(F(" PULSES="));
    Serial.print(pulses);

    Serial.print(F(" DIR="));
    Serial.print(direction);

    Serial.print(F(" RPM="));
    Serial.print(rpm, 2);

    Serial.print(F(" FREQUENCY="));
    Serial.println(pulseFrequency, 2);
}

void requestControlledStop()
{
    if (!stepper.isRunning()) {
        controllerState = ControllerState::IDLE;

        Serial.println(F("OK ALREADY_STOPPED"));
        return;
    }

    stepper.stop();

    controllerState = ControllerState::STOPPING;

    Serial.println(F("OK STOPPING"));
}

void emergencyStop()
{
    stepper.setCurrentPosition(
        stepper.currentPosition()
    );

    requestedRpm = 0.0F;
    measuredRpm = 0.0F;

    movementWasActive = false;
    controllerState = ControllerState::ESTOPPED;

    Serial.println(F("OK ESTOP"));
}

void evaluateCompletedMovement()
{
    if (!movementWasActive) {
        return;
    }

    if (stepper.isRunning()) {
        return;
    }

    movementWasActive = false;
    requestedRpm = 0.0F;

    finalPositionError = calculatePositionError();

    if (
        absoluteLong(finalPositionError) >
        positionErrorTolerance
    ) {
        controllerState = ControllerState::FAULT;

        Serial.print(F("FAULT POSITION_ERROR"));

        Serial.print(F(" CMD_POS="));
        Serial.print(stepper.currentPosition());

        Serial.print(F(" ENC_POS="));
        Serial.print(readEncoderPosition());

        Serial.print(F(" EXPECTED_ENC="));
        Serial.print(expectedEncoderPosition());

        Serial.print(F(" ERROR="));
        Serial.print(finalPositionError);

        Serial.print(F(" TOLERANCE="));
        Serial.println(positionErrorTolerance);

        return;
    }

    controllerState = ControllerState::IDLE;

    Serial.print(F("DONE"));

    Serial.print(F(" CMD_POS="));
    Serial.print(stepper.currentPosition());

    Serial.print(F(" ENC_POS="));
    Serial.print(readEncoderPosition());

    Serial.print(F(" EXPECTED_ENC="));
    Serial.print(expectedEncoderPosition());

    Serial.print(F(" ERROR="));
    Serial.println(finalPositionError);
}

// ============================================================
// Position and fault handling
// ============================================================

void zeroPositions()
{
    if (stepper.isRunning()) {
        Serial.println(
            F("ERR cannot zero while motor is running")
        );
        return;
    }

    stepper.setCurrentPosition(0);
    writeEncoderPosition(0);

    clearInvalidTransitionCount();

    finalPositionError = 0;
    requestedRpm = 0.0F;
    measuredRpm = 0.0F;

    controllerState = ControllerState::IDLE;

    Serial.println(F("OK ZERO"));
}

void clearFault()
{
    if (stepper.isRunning()) {
        Serial.println(
            F("ERR cannot clear fault while running")
        );
        return;
    }

    controllerState = ControllerState::IDLE;
    finalPositionError = calculatePositionError();

    Serial.print(F("OK FAULT_CLEARED ERROR="));
    Serial.println(finalPositionError);
}

// ============================================================
// Configuration output
// ============================================================

void printConfiguration()
{
    Serial.print(F("CONFIG MOTOR_PPR="));
    Serial.print(motorPulsesPerRevolution);

    Serial.print(F(" ENCODER_CPR="));
    Serial.print(encoderCountsPerRevolution);

    Serial.print(F(" ENCODER_SIGN="));
    Serial.print(encoderDirectionSign);

    Serial.print(F(" ACCEL="));
    Serial.print(motorAcceleration, 2);

    Serial.print(F(" MAX_RPM="));
    Serial.print(maximumRpm, 2);

    Serial.print(F(" TOLERANCE="));
    Serial.print(positionErrorTolerance);

    Serial.print(F(" TELEMETRY_MS="));
    Serial.println(telemetryIntervalMs);
}

void printHelp()
{
    Serial.println(F("COMMANDS:"));
    Serial.println(F("MOVE <pulses> <CW|CCW> <rpm>"));
    Serial.println(F("STOP"));
    Serial.println(F("ESTOP"));
    Serial.println(F("ENABLE"));
    Serial.println(F("DISABLE"));
    Serial.println(F("ZERO"));
    Serial.println(F("STATUS"));
    Serial.println(F("CONFIG"));
    Serial.println(F("CLEAR_FAULT"));
    Serial.println(F("CLEAR_ENCODER_ERRORS"));

    Serial.println(
        F("SET_ACCEL <pulses_per_second_squared>")
    );

    Serial.println(
        F("SET_TOLERANCE <encoder_counts>")
    );

    Serial.println(
        F("SET_MOTOR_PPR <pulses_per_revolution>")
    );

    Serial.println(
        F("SET_ENCODER_CPR <counts_per_revolution>")
    );

    Serial.println(
        F("SET_ENCODER_SIGN <1|-1>")
    );

    Serial.println(F("SET_MAX_RPM <rpm>"));
    Serial.println(F("STREAM <ON|OFF>"));

    Serial.println(
        F("SET_STREAM_MS <milliseconds>")
    );
}

// ============================================================
// Parsing helpers
// ============================================================

bool parseLongValue(
    const char *text,
    long &result
)
{
    if (text == nullptr || *text == '\0') {
        return false;
    }

    char *end = nullptr;
    const long value = strtol(text, &end, 10);

    if (end == text || *end != '\0') {
        return false;
    }

    result = value;
    return true;
}

bool parseFloatValue(
    const char *text,
    float &result
)
{
    if (text == nullptr || *text == '\0') {
        return false;
    }

    char *end = nullptr;
    const float value = strtod(text, &end);

    if (end == text || *end != '\0') {
        return false;
    }

    result = value;
    return true;
}

// ============================================================
// Command processor
// ============================================================

void processCommand(char *line)
{
    while (*line == ' ' || *line == '\t') {
        ++line;
    }

    if (*line == '\0') {
        return;
    }

    convertToUppercase(line);

    char *command = strtok(line, " \t");

    if (command == nullptr) {
        return;
    }

    // --------------------------------------------------------
    // MOVE
    // --------------------------------------------------------

    if (strcmp(command, "MOVE") == 0) {
        char *pulsesText = strtok(nullptr, " \t");
        char *directionText = strtok(nullptr, " \t");
        char *rpmText = strtok(nullptr, " \t");
        char *extra = strtok(nullptr, " \t");

        if (
            pulsesText == nullptr ||
            directionText == nullptr ||
            rpmText == nullptr ||
            extra != nullptr
        ) {
            Serial.println(
                F("ERR usage: MOVE <pulses> <CW|CCW> <rpm>")
            );
            return;
        }

        long pulses;
        float rpm;

        if (!parseLongValue(pulsesText, pulses)) {
            Serial.println(F("ERR invalid pulse value"));
            return;
        }

        if (!parseFloatValue(rpmText, rpm)) {
            Serial.println(F("ERR invalid rpm value"));
            return;
        }

        startMovement(
            pulses,
            directionText,
            rpm
        );

        return;
    }

    // --------------------------------------------------------
    // Basic commands
    // --------------------------------------------------------

    if (strcmp(command, "STOP") == 0) {
        requestControlledStop();
        return;
    }

    if (strcmp(command, "ESTOP") == 0) {
        emergencyStop();
        return;
    }

    if (strcmp(command, "ENABLE") == 0) {
        enableDriver();
        Serial.println(F("OK ENABLED"));
        return;
    }

    if (strcmp(command, "DISABLE") == 0) {
        if (stepper.isRunning()) {
            Serial.println(
                F("ERR cannot disable while running")
            );
            return;
        }

        disableDriver();
        Serial.println(F("OK DISABLED"));
        return;
    }

    if (strcmp(command, "ZERO") == 0) {
        zeroPositions();
        return;
    }

    if (strcmp(command, "STATUS") == 0) {
        printTelemetry();
        return;
    }

    if (strcmp(command, "CONFIG") == 0) {
        printConfiguration();
        return;
    }

    if (strcmp(command, "CLEAR_FAULT") == 0) {
        clearFault();
        return;
    }

    if (
        strcmp(command, "CLEAR_ENCODER_ERRORS") == 0
    ) {
        clearInvalidTransitionCount();

        Serial.println(
            F("OK ENCODER_ERRORS_CLEARED")
        );
        return;
    }

    if (strcmp(command, "HELP") == 0) {
        printHelp();
        return;
    }

    // --------------------------------------------------------
    // SET_ACCEL
    // --------------------------------------------------------

    if (strcmp(command, "SET_ACCEL") == 0) {
        char *valueText = strtok(nullptr, " \t");
        float value;

        if (
            !parseFloatValue(valueText, value) ||
            value <= 0.0F
        ) {
            Serial.println(
                F("ERR invalid acceleration")
            );
            return;
        }

        motorAcceleration = value;
        stepper.setAcceleration(value);

        Serial.print(F("OK ACCEL="));
        Serial.println(value, 2);
        return;
    }

    // --------------------------------------------------------
    // SET_TOLERANCE
    // --------------------------------------------------------

    if (strcmp(command, "SET_TOLERANCE") == 0) {
        char *valueText = strtok(nullptr, " \t");
        long value;

        if (
            !parseLongValue(valueText, value) ||
            value < 0
        ) {
            Serial.println(
                F("ERR invalid tolerance")
            );
            return;
        }

        positionErrorTolerance = value;

        Serial.print(F("OK TOLERANCE="));
        Serial.println(positionErrorTolerance);
        return;
    }

    // --------------------------------------------------------
    // SET_MOTOR_PPR
    // --------------------------------------------------------

    if (strcmp(command, "SET_MOTOR_PPR") == 0) {
        char *valueText = strtok(nullptr, " \t");
        long value;

        if (
            !parseLongValue(valueText, value) ||
            value <= 0
        ) {
            Serial.println(
                F("ERR invalid motor pulses per revolution")
            );
            return;
        }

        motorPulsesPerRevolution = value;

        Serial.print(F("OK MOTOR_PPR="));
        Serial.println(motorPulsesPerRevolution);
        return;
    }

    // --------------------------------------------------------
    // SET_ENCODER_CPR
    // --------------------------------------------------------

    if (strcmp(command, "SET_ENCODER_CPR") == 0) {
        char *valueText = strtok(nullptr, " \t");
        long value;

        if (
            !parseLongValue(valueText, value) ||
            value <= 0
        ) {
            Serial.println(
                F("ERR invalid encoder counts per revolution")
            );
            return;
        }

        encoderCountsPerRevolution = value;

        Serial.print(F("OK ENCODER_CPR="));
        Serial.println(encoderCountsPerRevolution);
        return;
    }

    // --------------------------------------------------------
    // SET_ENCODER_SIGN
    // --------------------------------------------------------

    if (strcmp(command, "SET_ENCODER_SIGN") == 0) {
        char *valueText = strtok(nullptr, " \t");
        long value;

        if (
            !parseLongValue(valueText, value) ||
            (value != 1 && value != -1)
        ) {
            Serial.println(
                F("ERR encoder sign must be 1 or -1")
            );
            return;
        }

        const long currentPosition =
            readEncoderPosition();

        encoderDirectionSign =
            static_cast<int8_t>(value);

        writeEncoderPosition(currentPosition);

        Serial.print(F("OK ENCODER_SIGN="));
        Serial.println(encoderDirectionSign);
        return;
    }

    // --------------------------------------------------------
    // SET_MAX_RPM
    // --------------------------------------------------------

    if (strcmp(command, "SET_MAX_RPM") == 0) {
        char *valueText = strtok(nullptr, " \t");
        float value;

        if (
            !parseFloatValue(valueText, value) ||
            value <= 0.0F
        ) {
            Serial.println(
                F("ERR invalid maximum rpm")
            );
            return;
        }

        maximumRpm = value;

        Serial.print(F("OK MAX_RPM="));
        Serial.println(maximumRpm, 2);
        return;
    }

    // --------------------------------------------------------
    // STREAM
    // --------------------------------------------------------

    if (strcmp(command, "STREAM") == 0) {
        char *stateText = strtok(nullptr, " \t");

        if (stateText == nullptr) {
            Serial.println(
                F("ERR usage: STREAM <ON|OFF>")
            );
            return;
        }

        if (strcmp(stateText, "ON") == 0) {
            telemetryEnabled = true;
            Serial.println(F("OK STREAM=ON"));
            return;
        }

        if (strcmp(stateText, "OFF") == 0) {
            telemetryEnabled = false;
            Serial.println(F("OK STREAM=OFF"));
            return;
        }

        Serial.println(
            F("ERR stream value must be ON or OFF")
        );
        return;
    }

    // --------------------------------------------------------
    // SET_STREAM_MS
    // --------------------------------------------------------

    if (strcmp(command, "SET_STREAM_MS") == 0) {
        char *valueText = strtok(nullptr, " \t");
        long value;

        if (
            !parseLongValue(valueText, value) ||
            value < 20 ||
            value > 5000
        ) {
            Serial.println(
                F("ERR stream interval must be 20..5000 ms")
            );
            return;
        }

        telemetryIntervalMs =
            static_cast<unsigned long>(value);

        Serial.print(F("OK STREAM_MS="));
        Serial.println(telemetryIntervalMs);
        return;
    }

    Serial.print(F("ERR unknown command: "));
    Serial.println(command);
}

// ============================================================
// Serial receiver
// ============================================================

void readSerialCommands()
{
    while (Serial.available() > 0) {
        const char received =
            static_cast<char>(Serial.read());

        if (received == '\r') {
            continue;
        }

        if (received == '\n') {
            commandBuffer[commandIndex] = '\0';

            processCommand(commandBuffer);

            commandIndex = 0;
            continue;
        }

        if (commandIndex < COMMAND_BUFFER_SIZE - 1) {
            commandBuffer[commandIndex++] = received;
        } else {
            commandIndex = 0;

            Serial.println(
                F("ERR command too long")
            );
        }
    }
}

// ============================================================
// Arduino setup
// ============================================================

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    pinMode(ENABLE_PIN, OUTPUT);

    pinMode(ENCODER_A_PIN, INPUT_PULLUP);
    pinMode(ENCODER_B_PIN, INPUT_PULLUP);

    disableDriver();

    stepper.setPinsInverted(
        INVERT_MOTOR_DIRECTION,
        false,
        false
    );

    stepper.setMinPulseWidth(
        STEP_PULSE_WIDTH_US
    );

    stepper.setAcceleration(
        motorAcceleration
    );

    stepper.setMaxSpeed(
        rpmToPulsesPerSecond(120.0F)
    );

    stepper.setCurrentPosition(0);

    previousEncoderState =
        readEncoderStateFast();

    writeEncoderPosition(0);

    attachInterrupt(
        digitalPinToInterrupt(ENCODER_A_PIN),
        encoderInterrupt,
        CHANGE
    );

    attachInterrupt(
        digitalPinToInterrupt(ENCODER_B_PIN),
        encoderInterrupt,
        CHANGE
    );

    previousSpeedTime = millis();
    previousTelemetryTime = millis();

    Serial.println(
        F("READY Arduino DM542 encoder controller")
    );

    Serial.println(
        F("ENCODER A=PIN2 B=PIN3 INTERNAL_DECODER=YES")
    );

    Serial.println(
        F("Protocol: MOVE <pulses> <CW|CCW> <rpm>")
    );

    printConfiguration();
}

// ============================================================
// Main loop
// ============================================================

void loop()
{
    readSerialCommands();

    // Must run as frequently as possible for AccelStepper.
    stepper.run();

    updateMeasuredSpeed();
    evaluateCompletedMovement();
    updateTelemetry();
}
