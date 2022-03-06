#include "esphome.h"
#include "TickTwo.h"

#define STATE_UNKNOWN 0
#define STATE_CLEANING 1
#define STATE_RETURNING 2
#define STATE_DOCKED 3
#define STATE_IDLE 4
#define STATE_MOVING 5
#define STATE_STOP 6
#define RETURN_BATTERY_PERCENT 50

#define CHARGE_NONE 0           // Not charging
#define CHARGE_RECONDITIONING 1 // Reconditioning
#define CHARGE_BULK 2           // Full Charging
#define CHARGE_TRICKLE 3        // Trickle Charging
#define CHARGE_WAITING 4        // Waiting
#define CHARGE_FAULT 5          // Charging Fault Condition

// Hold the Roomba status
struct Roomba_State
{
  uint8_t state;
  uint8_t charge_state;
  uint16_t battery_voltage;
  int16_t battery_current;
  uint8_t battery_temp;
  uint16_t battery_charge;
  uint16_t battery_capacity;
  uint8_t battery_percent;
  uint8_t num_restarts;
  uint8_t num_timeouts;
};

struct Roomba_Speed
{
  int16_t speed;
  int16_t left;
  int16_t right;
  int16_t radius;
};

struct Roomba_Motors
{
  int16_t main;
  int16_t vacuum;
  int16_t lateral;
};

class RoombaComponent : public PollingComponent,
                        public UARTDevice,
                        public CustomAPIDevice
{
  // Sensor *chargeSensor{nullptr};
  uint8_t noSleepPin;

  struct Roomba_State Roomba;
  struct Roomba_Speed Speed;
  struct Roomba_Motors Motors;

  RoombaComponent(UARTComponent *parent, uint32_t updateInterval, uint8_t noSleepPin, uint32_t fastUpdateInterval) : PollingComponent(updateInterval), UARTDevice(parent), ticker{[this]()
                                                                                                                                                     {
                                                                                                                                                       //ESP_LOGD("custom", "FAST TRICKER");
                                                                                                                                                       this->getSensors();
                                                                                                                                                     },
                                                                                                                                                     fastUpdateInterval}
  {
    this->noSleepPin = noSleepPin;
   // ticker.start();
  }

public:
  Sensor *battery_charge = new Sensor();
  Sensor *battery_temp = new Sensor();
  TextSensor *roombaState = new TextSensor();
  Sensor *speed = new Sensor();
  Sensor *radius = new Sensor();
  Sensor *mainSpeed = new Sensor();
  Sensor *lateralSpeed = new Sensor();
  Sensor *vacuumSpeed = new Sensor();
  TextSensor *chargeState = new TextSensor();
  TickTwo ticker;

  static RoombaComponent *instance(UARTComponent *parent, uint32_t updateInterval, uint8_t noSleepPin,  uint32_t fastUpdateInterval)
  {
    static RoombaComponent *INSTANCE = new RoombaComponent(parent, updateInterval, noSleepPin, fastUpdateInterval);
    return INSTANCE;
  }

  void loop() override {
    if (Roomba.state != STATE_DOCKED && Roomba.state != STATE_UNKNOWN)
    {
      //ESP_LOGD("custom", "TICKER: %d", ticker.state());
      if (ticker.state() != RUNNING)
      {
        ticker.start();
      }
      ticker.update();
    }else{
      ticker.stop();
    }
  }

  void setup() override
  {
    // Setup the GPIO for pulsing the BRC pin.
   // ticker.pause();
    pinMode(noSleepPin, OUTPUT);
    digitalWrite(noSleepPin, HIGH);

    register_service(&RoombaComponent::startCleaning, "startCleaning");
    register_service(&RoombaComponent::stopCleaning, "stopCleaning");
    register_service(&RoombaComponent::startSpot, "spotCleaning");
    register_service(&RoombaComponent::return_to_base, "dock");
    register_service(&RoombaComponent::drive, "drive", {"velocity", "radius"});
    // register_service(&RoombaComponent::driveDirect, "driveDirect", {"left", "right"});
    register_service(&RoombaComponent::stopMove, "stopMoving");

    delay(1000);
    resetRoomba();
    delay(1000);
    getSensors();
  }

  void update() override
  {
    if (Roomba.num_timeouts > 5)
    {
      resetRoomba();
    }
    // startCleaning();
    getSensors();
  }

  void stayAwake()
  {
    ESP_LOGD("custom", "Pulsing the noSleep pin");
    digitalWrite(noSleepPin, LOW);
    delay(50);
    digitalWrite(noSleepPin, HIGH);
  }

  void getSensors()
  {
    // stayAwake();
    char buffer[10];
    ESP_LOGD("custom", "getSensors - start");
    int i = 0;
    while (available() > 0)
    {
      read();
      i++;
      delay(1);
    }
    if (i > 0)
    {
      ESP_LOGD("custom", "Dumped %f bytes", i);
    }
    // Ask for sensor group 3.
    // 21 (1 byte reply) - charge state
    // 22 (2 byte reply) - battery voltage
    // 23 (2 byte reply) - battery_current
    // 24 (1 byte reply) - battery_temp
    // 25 (2 byte reply) - battery charge
    // 26 (2 byte reply) - battery capacity
    if (Roomba.state == STATE_MOVING)
    {
      byte command[] = {/*128,*/ 149, 1, 3};
      sendCommandList(command, 3);
    }
    else
    {
      byte command[] = {128, 149, 1, 3};
      sendCommandList(command, 4);
    }

    // Allow 25ms for processing...
    delay(25);

    // We should get 10 bytes back.
    i = 0;
    // ESP_LOGD("custom", "RX:");
    while (available() > 0)
    {
      buffer[i] = read();
      // ESP_LOGD("custom", "buffer[%d] %f", i, buffer[i]);
      i++;
      delay(1);
    }
    // Handle if the Roomba stops responding.
    if (i == 0)
    {
      Roomba.num_timeouts++;

      if (Roomba.num_timeouts > 10)
      {
        Roomba.state = STATE_UNKNOWN;
      }
      sendState();
      return;
    }
    else
    {
      Roomba.num_timeouts = 0;
    }

    // Handle an incomplete packet (too much or too little data)
    // if (i != 10)
    // {
    //   status_log += "ERROR: Incomplete packet recieved ";
    //   status_log += i;
    //   status_log += " bytes.\n";
    //   return;
    // }

    // Parse the buffer...
    Roomba.charge_state = buffer[0];
    Roomba.battery_voltage = (uint16_t)word(buffer[1], buffer[2]);
    Roomba.battery_current = (int16_t)word(buffer[3], buffer[4]);
    Roomba.battery_temp = buffer[5];
    Roomba.battery_charge = (uint16_t)word(buffer[6], buffer[7]);
    Roomba.battery_capacity = (uint16_t)word(buffer[8], buffer[9]);

    ESP_LOGD("custom", "Roomba: charge %s, volt: %d, curr: %d, temp %d, charge %d, capacity %d", mapChargeState(Roomba.charge_state).c_str(), Roomba.battery_voltage, Roomba.battery_current, Roomba.battery_temp, Roomba.battery_charge, Roomba.battery_capacity);

    // Sanity check some data...
    if (Roomba.charge_state > 6)
    {
      return;
    } // Values should be 0-6
    if (Roomba.battery_capacity == 0)
    {
      return;
    } // We should never get this - but we don't want to divide by zero!
    if (Roomba.battery_capacity > 6000)
    {
      return;
    } // Usually around 2050 or so.
    if (Roomba.battery_charge > 6000)
    {
      return;
    } // Can't be greater than battery_capacity
    if (Roomba.battery_voltage > 18000)
    {
      return;
    } // Should be about 17v on charge, down to ~13.1v when flat.

    uint8_t new_battery_percent = 100 * Roomba.battery_charge / Roomba.battery_capacity;
    if (new_battery_percent > 100)
    {
      return;
    }
    Roomba.battery_percent = new_battery_percent;

    // Reset num_restarts if current draw is over 300mA
    if (Roomba.battery_current < -300)
    {
      Roomba.num_restarts = 0;
    }

    // Set to a STATE_UNKNOWN when 5 restarts have failed.
    if (Roomba.num_restarts >= 5)
    {
      Roomba.state = STATE_UNKNOWN;
    }

    // // The next two states restart cleaning if battery current is too low do be doing anything.
    // if (Roomba.state == STATE_CLEANING)
    // {
    //   if (Roomba.battery_percent > 10 && Roomba.battery_current > -300)
    //   {
    //     Roomba.num_restarts++;
    //     startCleaning();
    //   }
    // }
    // if (Roomba.state == STATE_RETURNING)
    // {
    //   if (Roomba.battery_percent > 10 && Roomba.battery_current > -300)
    //   {
    //     Roomba.num_restarts++;
    //     return_to_base();
    //   }
    // }

    // The following will only be run if we're in Reconditioning, Bulk charge, or Trickle charge.
    if (Roomba.charge_state >= CHARGE_RECONDITIONING && Roomba.charge_state <= CHARGE_WAITING)
    {
      if (Roomba.state != STATE_CLEANING && Roomba.state != STATE_MOVING)
      {
        if (Roomba.battery_percent > 10 && Roomba.battery_current > -300 && Roomba.battery_current <-100)
        {
          Roomba.state = STATE_IDLE;
        }
        else
        {
          Roomba.state = STATE_DOCKED;
        }
      }
    }

    // Start seeking the dock if battery gets to RETURN_BATTERY_PERCENT % or below and we're still in STATE_CLEANING
    if (Roomba.state == STATE_CLEANING && Roomba.battery_percent <= RETURN_BATTERY_PERCENT)
    {
      return_to_base();
    }

    sendState();
    ESP_LOGD("custom", "getSensors - success");
  }

  void sendState()
  {
    this->battery_charge->publish_state(Roomba.battery_percent);
    this->battery_temp->publish_state(Roomba.battery_temp);
    this->speed->publish_state(Speed.speed);
    this->radius->publish_state(Speed.radius);
    this->roombaState->publish_state(translateState().c_str());
    this->mainSpeed->publish_state(Motors.main);
    this->lateralSpeed->publish_state(Motors.lateral);
    this->vacuumSpeed->publish_state(Motors.vacuum);
    this->chargeState->publish_state(mapChargeState(Roomba.charge_state).c_str());
  }

  String translateState()
  {
    String state;
    if (Roomba.state == STATE_CLEANING)
    {
      state = "cleaning";
    };
    if (Roomba.state == STATE_DOCKED)
    {
      state = "docked";
    };
    if (Roomba.state == STATE_IDLE)
    {
      state = "idle";
    };
    if (Roomba.state == STATE_RETURNING)
    {
      state = "returning";
    };
    if (Roomba.state == STATE_UNKNOWN)
    {
      state = "error";
    };
    if (Roomba.state == STATE_MOVING)
    {
      state = "moving";
    };
    if (Roomba.state == STATE_STOP)
    {
      state = "stop";
    };
    return state;
  }

  // Send commands:
  // 135 = Clean
  // 136 = Start Max Cleaning Mode
  void startCleaning()
  {
    stayAwake();
    Roomba.state = STATE_CLEANING;
    byte command[] = {128, 131, 135};
    sendCommandList(command, 3);
    sendState();
  }

  void startSpot()
  {
    stayAwake();
    Roomba.state = STATE_CLEANING;
    byte command[] = {128, 131, 134};
    sendCommandList(command, 3);
    sendState();
  }

  // Send commands:
  // 143 = Seek Dock
  void return_to_base()
  {
    stayAwake();
    // IF we're already cleaning, sending this once will only stop the Roomba.
    // We have to send twice to actually do something...
    if (Roomba.state == STATE_CLEANING || Roomba.state == STATE_MOVING)
    {
      byte command[] = {128, 131, 143};
      sendCommandList(command, 3);
      delay(250);
    }
    Roomba.state = STATE_RETURNING;
    byte command[] = {128, 131, 143};
    sendCommandList(command, 3);
    sendState();
  }

  // Send commands:
  // 133 = Power Down
  // 128 + 131 = Start OI, Enter Safe Mode
  void stopCleaning()
  {
    stayAwake();
    Roomba.state = STATE_IDLE;
    byte command[] = {128, 131, 133, 128, 131};
    sendCommandList(command, 5);
    sendState();
  }

  void sendCommandList(byte *commands, byte len)
  {
    // ESP_LOGD("custom", "TX:");
    for (int i = 0; i < len; i++)
    {
      // ESP_LOGD("custom", "byte %hhx", commands[i]);
      write(commands[i]);
    }
  }

  void resetRoomba()
  {
    Roomba.num_timeouts = 0;
    byte command[] = {128, 129, 11, 7};

    // Send factory reset in 19200 baud (sometimes we get stuck in this baud?!)
    stayAwake();
    Serial.begin(19200);
    sendCommandList(command, 4);

    delay(100);

    // Send factory reset at 115200 baud (we should always be in this - but sometimes it bugs out.)
    stayAwake();
    Serial.begin(115200);
    sendCommandList(command, 4);
  }

  void disconnect()
  {
    byte command[] = {173};
    stayAwake();
    sendCommandList(command, 1);
  }

  void drive(int velocity, int radius)
  {
    stayAwake();
    if (Roomba.state != STATE_MOVING || Roomba.state != STATE_STOP)
    {
    //full mode
      byte command[] = {128, 132};
      sendCommandList(command, 2);
      stopMove();
    }
    Speed.speed = velocity;
    Speed.radius = radius;
    write(137);
    write((velocity & 0xff00) >> 8);
    write(velocity & 0xff);
    write((radius & 0xff00) >> 8);
    write(radius & 0xff);
    Roomba.state = STATE_MOVING;
    sendState();
  }

  void driveForward()
  {
    if (Speed.radius != 0)
    {
      drive(Speed.speed, 0);
    }
    else
    {

      drive(Speed.speed + 60, 0);
    }
  }

  void driveBackwards()
  {
    drive(Speed.speed - 60, 0);
  }

  void driveCW()
  {
    if (Roomba.state == STATE_MOVING && Speed.radius != -1)
    {
      Speed.speed = 0;
      Speed.right = 0;
    }
    drive(Speed.speed + 30, -1);
  }

  void driveCCW()
  {
    if (Roomba.state == STATE_MOVING && Speed.radius != 1)
    {
      Speed.speed = 0;
      Speed.right = 0;
    }
    drive(Speed.speed + 30, 1);
  }

  void driveLeft()
  {
    if (Speed.radius <= 0 || Speed.radius == -1)
    {
      Speed.radius = 500;
    }
    if (Speed.speed == 0)
    {
      Speed.speed = 180;
    }
    drive(Speed.speed, Speed.radius - 100);
  }

  void driveRight()
  {
    if (Speed.radius >= 0 || Speed.radius == -1)
    {
      Speed.radius = -500;
    }
    if (Speed.speed == 0)
    {
      Speed.speed = 180;
    }
    drive(Speed.speed, Speed.radius + 100);
  }

  void stopMove()
  {
    Speed.speed = 0;
    Speed.right = 0;
    Speed.radius = 0;
    write(145);
    write(0);
    write(0);
    write(0);
    write(0);
    Roomba.state = STATE_STOP;
    sendState();
  }

  void driveDirect(int left, int right)
  {
    stayAwake();
    byte command[] = {128, 131};
    sendCommandList(command, 2);
    Speed.left = left;
    Speed.right = right;
    Speed.radius = 0;
    write(145);
    write((right & 0xff00) >> 8);
    write(right & 0xff);
    write((left & 0xff00) >> 8);
    write(left & 0xff);
    Roomba.state = STATE_MOVING;
    sendState();
  }

  void motors(int main, int lateral, int vacuum)
  {
    stayAwake();
    byte command[] = {128, 132};
    sendCommandList(command, 2);
    Motors.main = main;
    Motors.lateral = lateral;
    Motors.vacuum = vacuum;
    write(144);
    write(main);
    write(lateral);
    write(vacuum);
    sendState();
  }

  void mainBrush(int main)
  {
    motors(main, Motors.lateral, Motors.vacuum);
  }
  void lateralBrush(int lateral)
  {
    motors(Motors.main, lateral, Motors.vacuum);
  }
  void vacuum(int vacuum)
  {
    motors(Motors.main, Motors.lateral, vacuum);
  }

  void undock()
  {
    drive(-30, 0);
    delay(2000);
    stopMove();
  }

  String mapChargeState(int state)
  {
    String s;
    switch (state)
    {
    case 0:
      s = "Not charging";
      break;
    case 1:
      s = "Reconditioning Charging";
      break;
    case 2:
      s = "Full Charging";
      break;
    case 3:
      s = "Trickle Charging";
      break;
    case 4:
      s = "Waiting";
      break;
    case 5:
      s = "Charging Fault Condition";
      break;
    }
    return s;
  }
};