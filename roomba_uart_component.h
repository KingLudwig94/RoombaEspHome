#include "esphome.h"

class RoombaComponent : public PollingComponent, public UARTDevice
{
    Sensor *chargeSensor {nullptr};
public:
    RoombaComponent(UARTComponent *parent, uint32_t updateInterval, Sensor *sensor1) : PollingComponent(updateInterval),UARTDevice(parent), chargeSensor(sensor1) {}

    void setup() override
    {

   }
    void update() override
    {
      int16_t distance;
      uint16_t voltage;
      int16_t current;
      uint16_t charge;
      uint16_t capacity;
      uint8_t charging;
      int16_t temperature;      
    }
};