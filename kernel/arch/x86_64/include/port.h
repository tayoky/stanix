#ifndef PORT_H
#define PORT_H

#include <stdint.h>

/// @brief read an given port
/// @param port the port to read
/// @return the data on the given port
uint8_t in_byte(uint16_t port);

/// @brief out on a given port
/// @param port the port to out
/// @param data the data to out
void out_byte(uint16_t port,uint8_t data);

uint16_t in_word(uint16_t port);
void out_word(uint16_t port,uint16_t data);

uint32_t in_long(uint16_t port);

void out_long(uint16_t port, uint32_t data);

#endif