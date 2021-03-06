#include <Wire.h>

#include "ms5611.h"

// Constants

// MS5611 device address
#define MS5611_ADDR 0x77 // 0b1110111

// MS5611 device commands
#define MS5611_RESET_COMMAND 0x1E
#define MS5611_START_PRESSURE_ADC_CONVERSION 0x40
#define MS5611_START_TEMPERATURE_ADC_CONVERSION 0x50
#define MS5611_READ_ADC 0x00

#define MS5611_CONVERSION_OSR_MASK 0x0F

// MS5611 commands
#define MS5611_PROM_ADDRESS_READ_ADDRESS_0 0xA0
#define MS5611_PROM_ADDRESS_READ_ADDRESS_1 0xA2
#define MS5611_PROM_ADDRESS_READ_ADDRESS_2 0xA4
#define MS5611_PROM_ADDRESS_READ_ADDRESS_3 0xA6
#define MS5611_PROM_ADDRESS_READ_ADDRESS_4 0xA8
#define MS5611_PROM_ADDRESS_READ_ADDRESS_5 0xAA
#define MS5611_PROM_ADDRESS_READ_ADDRESS_6 0xAC
#define MS5611_PROM_ADDRESS_READ_ADDRESS_7 0xAE

// Coefficients indexes for temperature and pressure computation
#define MS5611_CRC_INDEX 7
#define MS5611_PRESSURE_SENSITIVITY_INDEX 1 
#define MS5611_PRESSURE_OFFSET_INDEX 2
#define MS5611_TEMP_COEFF_OF_PRESSURE_SENSITIVITY_INDEX 3
#define MS5611_TEMP_COEFF_OF_PRESSURE_OFFSET_INDEX 4
#define MS5611_REFERENCE_TEMPERATURE_INDEX 5
#define MS5611_TEMP_COEFF_OF_TEMPERATURE_INDEX 6


/**
* \brief Class constructor
*
*/
ms5611::ms5611(void) {}

/**
 * \brief Perform initial configuration. Has to be called once.
 */
void ms5611::begin(void) {
  Wire.begin();
}

/**
* \brief Check whether MS5611 device is connected
*
* \return bool : status of MS5611
*       - true : Device is present
*       - false : Device is not acknowledging I2C address
*/
boolean ms5611::is_connected(void) {
  Wire.beginTransmission((uint8_t)MS5611_ADDR);
  return (Wire.endTransmission() == 0);
}

/**
* \brief Writes the MS5611 8-bits command with the value passed
*
* \param[in] uint8_t : Command value to be written.
*
* \return ms5611_status : status of MS5611
*       - ms5611_status_ok : I2C transfer completed successfully
*       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
*/
enum ms5611_status ms5611::write_command(uint8_t cmd) {
  uint8_t i2c_status;

  Wire.beginTransmission((uint8_t)MS5611_ADDR);
  Wire.write(cmd);
  i2c_status = Wire.endTransmission();

  /* Do the transfer */
  if (i2c_status == ms5611_STATUS_ERR_OVERFLOW)
    return ms5611_status_no_i2c_acknowledge;
  if (i2c_status != ms5611_STATUS_OK)
    return ms5611_status_i2c_transfer_error;

  return ms5611_status_ok;
}

/**
* \brief Set  ADC resolution.
*
* \param[in] ms5611_resolution_osr : Resolution requested
*
*/
void ms5611::set_resolution(enum ms5611_resolution_osr res) {
  ms5611_resolution_osr = res;
}

/**
* \brief Reset the MS5611 device
*
* \return ms5611_status : status of MS5611
*       - ms5611_status_ok : I2C transfer completed successfully
*       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
*/
enum ms5611_status ms5611::reset(void) {
  return write_command(MS5611_RESET_COMMAND);
}

/**
* \brief Reads the ms5611 EEPROM coefficient stored at address provided.
*
* \param[in] uint8_t : Address of coefficient in EEPROM
* \param[out] uint16_t* : Value read in EEPROM
*
* \return ms5611_status : status of MS5611
*       - ms5611_status_ok : I2C transfer completed successfully
*       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
*       - ms5611_status_crc_error : CRC check error on the coefficients
*/
enum ms5611_status ms5611::read_eeprom_coeff(uint8_t command, uint16_t *coeff) {
  uint8_t buffer[2];
  uint8_t i;
  uint8_t i2c_status;

  buffer[0] = 0;
  buffer[1] = 0;

  /* Read data */
  Wire.beginTransmission((uint8_t)MS5611_ADDR);
  Wire.write(command);
  i2c_status = Wire.endTransmission();

  Wire.requestFrom((uint8_t)MS5611_ADDR, 2U);
  for (i = 0; i < 2; i++) {
    buffer[i] = Wire.read();
  }
  // Send the conversion command
  if (i2c_status == ms5611_STATUS_ERR_OVERFLOW)
    return ms5611_status_no_i2c_acknowledge;

  *coeff = (buffer[0] << 8) | buffer[1];

  return ms5611_status_ok;
}

/**
* \brief Reads the ms5611 EEPROM coefficients to store them for computation.
*
* \return ms5611_status : status of MS5611
*       - ms5611_status_ok : I2C transfer completed successfully
*       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
*       - ms5611_status_crc_error : CRC check error on the coefficients
*/
enum ms5611_status ms5611::read_eeprom(void) {
  enum ms5611_status status;
  uint8_t i;

  for (i = 0; i < MS5611_COEFFICIENT_NUMBERS; i++) {
    status = read_eeprom_coeff(MS5611_PROM_ADDRESS_READ_ADDRESS_0 + i * 2,
                               eeprom_coeff + i);
    if (status != ms5611_status_ok)
      return status;
  }
  if( !crc_check( eeprom_coeff, eeprom_coeff[MS5611_CRC_INDEX] & 0x000F ) )
    return ms5611_status_crc_error;

  coeff_read = true;

  return ms5611_status_ok;
}

/**
* \brief CRC check
*
* \param[in] uint16_t *: List of EEPROM coefficients
* \param[in] uint8_t : crc to compare with
*
* \return bool : TRUE if CRC is OK, FALSE if KO
*/
boolean ms5611::crc_check(uint16_t *n_prom, uint8_t crc) {
  uint8_t cnt, n_bit; 
  uint16_t n_rem; 
  uint16_t crc_read;

  n_rem = 0x00;
  crc_read = n_prom[7]; 
  n_prom[7] = (0xFF00 & (n_prom[7])); 
  for (cnt = 0; cnt < 16; cnt++) 
  {
    if (cnt % 2 == 1) 
      n_rem ^= (unsigned short) ((n_prom[cnt>>1]) & 0x00FF);
    else 
      n_rem ^= (unsigned short) (n_prom[cnt>>1]>>8);

    for (n_bit = 8; n_bit > 0; n_bit--)
    {
      if (n_rem & (0x8000))
        n_rem = (n_rem << 1) ^ 0x3000;
      else
        n_rem = (n_rem << 1);
    }
  }
  n_rem = (0x000F & (n_rem >> 12)); 
  n_prom[7] = crc_read;
  n_rem ^= 0x00;
        
  return (n_rem == crc);
}

/**
* \brief Triggers conversion and read ADC value
*
* \param[in] uint8_t : Command used for conversion (will determine Temperature
* vs Pressure and osr)
* \param[out] uint32_t* : ADC value.
*
* \return ms5611_status : status of MS5611
*       - ms5611_status_ok : I2C transfer completed successfully
*       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
*/
enum ms5611_status ms5611::conversion_and_read_adc(uint8_t cmd, uint32_t *adc) {
  enum ms5611_status status;
  uint8_t i2c_status;
  uint8_t buffer[3];
  uint8_t i;

  /* Read data */
  Wire.beginTransmission((uint8_t)MS5611_ADDR);
  Wire.write((uint8_t)cmd);
  Wire.endTransmission();

  delay(conversion_time[(cmd & MS5611_CONVERSION_OSR_MASK) / 2]);

  Wire.beginTransmission((uint8_t)MS5611_ADDR);
  Wire.write((uint8_t)0x00);
  i2c_status = Wire.endTransmission();

  Wire.requestFrom((uint8_t)MS5611_ADDR, 3U);
  for (i = 0; i < 3; i++) {
    buffer[i] = Wire.read();
  }

  // delay conversion depending on resolution
  if (status != ms5611_status_ok)
    return status;

  // Send the read command
  // status = ms5611_write_command(MS5611_READ_ADC);
  if (status != ms5611_status_ok)
    return status;

  if (i2c_status == ms5611_STATUS_ERR_OVERFLOW)
    return ms5611_status_no_i2c_acknowledge;
  if (i2c_status != ms5611_STATUS_OK)
    return ms5611_status_i2c_transfer_error;

  *adc = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];

  return status;
}

/**
* \brief Reads the temperature and pressure ADC value and compute the
* compensated values.
*
* \param[out] float* : Celsius Degree temperature value
* \param[out] float* : mbar pressure value
*
* \return ms5611_status : status of MS5611
*       - ms5611_status_ok : I2C transfer completed successfully
*       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
*       - ms5611_status_crc_error : CRC check error on the coefficients
*/
enum ms5611_status ms5611::read_temperature_and_pressure(float *temperature,
                                                         float *pressure) {
  enum ms5611_status status = ms5611_status_ok;
  uint32_t adc_temperature, adc_pressure;
  int32_t dT, TEMP;
  int64_t OFF, SENS, P, T2, OFF2, SENS2;
  uint8_t cmd;

  // If first time adc is requested, get EEPROM coefficients
  if (coeff_read == false)
    status = read_eeprom();

  if (status != ms5611_status_ok)
    return status;

  // First read temperature
  cmd = ms5611_resolution_osr * 2;
  cmd |= MS5611_START_TEMPERATURE_ADC_CONVERSION;
  status = conversion_and_read_adc(cmd, &adc_temperature);
  if (status != ms5611_status_ok)
    return status;

  // Now read pressure
  cmd = ms5611_resolution_osr * 2;
  cmd |= MS5611_START_PRESSURE_ADC_CONVERSION;
  status = conversion_and_read_adc(cmd, &adc_pressure);
  if (status != ms5611_status_ok)
    return status;

  if (adc_temperature == 0 || adc_pressure == 0)
    return ms5611_status_i2c_transfer_error;

  // Difference between actual and reference temperature = D2 - Tref
  dT = (int32_t)adc_temperature -
       ((int32_t)eeprom_coeff[MS5611_REFERENCE_TEMPERATURE_INDEX] << 8);

  // Actual temperature = 2000 + dT * TEMPSENS
  TEMP = 2000 +
         ((int64_t)dT *
              (int64_t)eeprom_coeff[MS5611_TEMP_COEFF_OF_TEMPERATURE_INDEX] >>
          23);

  // Second order temperature compensation
  if (TEMP < 2000) {
    T2 = (3 * ((int64_t)dT * (int64_t)dT)) >> 33;
    OFF2 = 61 * ((int64_t)TEMP - 2000) * ((int64_t)TEMP - 2000) / 16;
    SENS2 = 29 * ((int64_t)TEMP - 2000) * ((int64_t)TEMP - 2000) / 16;

    if (TEMP < -1500) {
      OFF2 += 17 * ((int64_t)TEMP + 1500) * ((int64_t)TEMP + 1500);
      SENS2 += 9 * ((int64_t)TEMP + 1500) * ((int64_t)TEMP + 1500);
    }
  } else {
    T2 = (5 * ((int64_t)dT * (int64_t)dT)) >> 38;
    OFF2 = 0;
    SENS2 = 0;
  }

  // OFF = OFF_T1 + TCO * dT
  OFF = ((int64_t)(eeprom_coeff[MS5611_PRESSURE_OFFSET_INDEX]) << 16) +
        (((int64_t)(eeprom_coeff[MS5611_TEMP_COEFF_OF_PRESSURE_OFFSET_INDEX]) *
          dT) >>
         7);
  OFF -= OFF2;

  // Sensitivity at actual temperature = SENS_T1 + TCS * dT
  SENS =
      ((int64_t)eeprom_coeff[MS5611_PRESSURE_SENSITIVITY_INDEX] << 15) +
      (((int64_t)eeprom_coeff[MS5611_TEMP_COEFF_OF_PRESSURE_SENSITIVITY_INDEX] *
        dT) >>
       8);
  SENS -= SENS2;

  // Temperature compensated pressure = D1 * SENS - OFF
  P = (((adc_pressure * SENS) >> 21) - OFF) >> 15;

  *temperature = ((float)TEMP - T2) / 100;
  *pressure = (float)P / 100;

  return status;
}
