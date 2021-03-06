#include <stdint.h>
#include "pano_time.h"
#include "i2c.h"

#define DEBUG_LOGGING
#include "log.h"

#define REG_WR(reg, wr_data)       *((volatile uint8_t *)reg) = (wr_data)
#define REG_RD(reg)                *((volatile uint8_t *)reg)

// Slow I2C devices need 4.7 microseconds ... but the Pano doesn't have
// any slow devices, but be safe.
#define I2C_DLY() delay_us(5)

static void i2c_set_scl(int Port, int bit)
{
   REG_WR(Port + SCL_OFFSET,bit);
}

static void i2c_set_sda(int Port, int bit)
{
   REG_WR(Port + SDA_OFFSET,bit);
}


static int i2c_get_scl(int Port)
{
   return REG_RD(Port + SCL_OFFSET);
}

static int i2c_get_sda(int Port)
{
   return REG_RD(Port + SDA_OFFSET);
}

void i2c_init(int Port)
{
   i2c_set_sda(Port, 1);
   i2c_set_scl(Port, 1);
   I2C_DLY();
}


void i2c_start(int Port)
{
   i2c_set_sda(Port, 1);             // i2c start bit sequence
   I2C_DLY();
   i2c_set_scl(Port, 1);
   I2C_DLY();
   i2c_set_sda(Port, 0);
   I2C_DLY();
   i2c_set_scl(Port, 0);
   I2C_DLY();
}

void i2c_stop(int Port)
{
   i2c_set_sda(Port, 0);             // i2c stop bit sequence
   I2C_DLY();
   i2c_set_scl(Port, 1);
   I2C_DLY();
   i2c_set_sda(Port, 1);
   I2C_DLY();
}

unsigned char i2c_rx(int Port, char ack)
{
   char x, d=0;

   i2c_set_sda(Port, 1);

   for(x=0; x<8; x++) {
      d <<= 1;

      i2c_set_scl(Port, 1);
      I2C_DLY();

      // wait for any i2c_set_scl clock stretching
      while(i2c_get_scl(Port) == 0);

      d |= i2c_get_sda(Port);
      i2c_set_scl(Port, 0);
      I2C_DLY();
   }

   if(ack) {
      i2c_set_sda(Port, 0);
   }
   else {
      i2c_set_sda(Port, 1);
   }

   i2c_set_scl(Port, 1);
   I2C_DLY();         // send (N)ACK bit

   i2c_set_scl(Port, 0);
   I2C_DLY();         // send (N)ACK bit

   i2c_set_sda(Port, 1);
   return d;
}

// return 1: ACK, 0: NACK
int i2c_tx(int Port, unsigned char d)
{
   char x;
   int bit;

   for(x=8; x; x--) {
      i2c_set_sda(Port, (d & 0x80)>>7);
      d <<= 1;
      I2C_DLY();
      i2c_set_scl(Port, 1);
      I2C_DLY();
      i2c_set_scl(Port, 0);
   }
   i2c_set_sda(Port, 1);
   I2C_DLY();
   I2C_DLY();
   bit = i2c_get_sda(Port);         // possible ACK bit
   i2c_set_scl(Port, 1);
   I2C_DLY();

   i2c_set_scl(Port, 0);
   I2C_DLY();

   return !bit;
}

// return 1: ACK, 0: NACK
int i2c_write_buf(int Port, uint8_t ADR, uint8_t* data, int len)
{
   int ack;

   i2c_start(Port);
   ack = i2c_tx(Port, ADR);
   if(!ack) {
      i2c_stop(Port);
      ELOG("Error: No ack on adr\n");
      return 0;
   }


   int i;
   for(i=0;i<len;++i) {
      ack = i2c_tx(Port, data[i]);
      if(!ack) {
         ELOG("Error: No data byte %d\n",i);
         i2c_stop(Port);
         return 0;
      }
   }

   i2c_stop(Port);

   return 1;
}

int i2c_read_buf(int Port, uint8_t ADR, uint8_t *data, int len)
{
   int ack;

   i2c_start(Port);

   ack = i2c_tx(Port, ADR | 1);
   if(!ack) {
      i2c_stop(Port);
      return 0;
   }

   int i;
   for(i=0; i < len; ++i) {
      data[i] = i2c_rx(Port, i != len-1);
   }
   i2c_stop(Port);

   return 1;
}

int i2c_write_reg_nr(int Port, uint8_t ADR, uint8_t reg_nr)
{
   return i2c_write_buf(Port, ADR, &reg_nr, 1);
}

// return 1: ACK, 0: NACK
int i2c_write_reg(int Port, uint8_t ADR, uint8_t reg_nr, uint8_t value)
{
   uint8_t data[2] = { reg_nr, value };

   return i2c_write_buf(Port, ADR, data, 2);
}

int i2c_write_regs(int Port, uint8_t ADR, uint8_t reg_nr, uint8_t *values, int len)
{
   int ack;

   i2c_start(Port);

   ack = i2c_tx(Port, ADR);
   if(!ack) {
      i2c_stop(Port);
      return 0;
   }

   ack = i2c_tx(Port, reg_nr);
   if(!ack) {
      i2c_stop(Port);
      return 0;
   }

   int i;
   for(i=0;i<len;++i) {
      ack = i2c_tx(Port, values[i]);
      if(!ack) {
         i2c_stop(Port);
         return 0;
      }
   }

   i2c_stop(Port);

   return 1;
}


int i2c_read_reg(int Port, uint8_t ADR, uint8_t reg_nr, uint8_t *value)
{
   int result;

   // Set ADRess to read
   result = i2c_write_buf(Port, ADR, &reg_nr, 1);
   if(!result)
      return 0;

   result = i2c_read_buf(Port, ADR, value, 1);
   if(!result)
      return 0;

   return 1;
}

int i2c_read_regs(int Port, uint8_t ADR, uint8_t reg_nr, uint8_t *values, int len)
{
   int result;

   // Set ADRess to read
   result = i2c_write_buf(Port, ADR, &reg_nr, 1);
   if(!result) {
      return 0;
   }

   result = i2c_read_buf(Port, ADR, values, len);
   if(!result) {
      return 0;
   }

   return 1;
}

