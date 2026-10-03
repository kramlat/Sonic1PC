#include "Joypad.h"

//Backend input interface
uint8_t Input_GetState1(void);
uint8_t Input_GetState2(void);
uint8_t Input_GetExtState1(void);
void Input_SetTextInputMode(bool enable);

//Joypad information
uint8_t Joypad_GetState1(void) {
	return Input_GetState1();
}

uint8_t Joypad_GetState2(void) {
	return Input_GetState2();
}

uint8_t Joypad_GetExtState1(void) {
	return Input_GetExtState1();
}

void Joypad_SetTextInputMode(bool enable) {
	Input_SetTextInputMode(enable);
}
