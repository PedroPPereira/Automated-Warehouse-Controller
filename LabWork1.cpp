#include<stdafx.h> // or replace with pch.h
#include <stdio.h>
#include <string>
#include <iostream>
#include <conio.h>
#include <windows.h>  //for Sleep function
#include <time.h>
#include <list>
#include <stdbool.h>
#include <inttypes.h>

extern "C" {
	// observe your project contents.  We are mixing C files with cpp ones.
	// Therefore, inside cpp files, we need to tell which functions are written in C.
	// That is why we use extern "C"  directive
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <interface.h> 
}
//Automated Warehouse site: http://localhost:8081/parking.html 

//constants
#define COST 5  //cost per second of the parking warehouse

//structures
typedef struct {
	int x;
	int z;
} TPosition;

typedef struct {
	int money_earned;    //total money earned that day in the warehouse
	bool bool_restart;
	uInt8 stop_state;
	std::list<std::string> list_car_finished; //list of finished car services
} Warehouse;

typedef struct {
	bool inside;
	int plate_number;
	time_t time_parked; //time of when the car was put inside
} CarPallete;

Warehouse warehouse;
CarPallete car[3][3];

//mailboxes
xQueueHandle        mbx_x;        //for goto_x
xQueueHandle        mbx_z;        //for goto_z
xQueueHandle        mbx_xz;       //for goto_xz
xQueueHandle		mbx_flash1;   //for flash_led1_task
xQueueHandle		mbx_flash2;   //for flash_led2_task
xQueueHandle        mbx_put_get;  //for switch_get_put_task
xQueueHandle        mbx_input;

//semahores
xSemaphoreHandle  sem_x_done;
xSemaphoreHandle  sem_z_done;
xSemaphoreHandle  sem_sw_12;
xSemaphoreHandle  sem_sw_es_ON;
xSemaphoreHandle  sem_sw_es_OFF;
xSemaphoreHandle  sem_me_sw;



/*********************************************************************FUNCTIONS*******************************************************/

/******************************************************************************
* Function Name: getBitValue
* Description: Reads the sensor and returns the value of the bit
* Type: Integer
* Input: uInt8 value(sensor vector) / uInt8 n_bit(position of the sensor vector)
* Output: 1(true) / 0(false)
*******************************************************************************/
int getBitValue(uInt8 value, uInt8 n_bit)
{
	return(value & (1 << n_bit));
}

/******************************************************************************
* Function Name: setBitValue
* Description: Activates the actuators and sets the value of the bit
* Type: Void
* Input: uInt8  *variable(pointer of the sensor vector) / int n_bit(position of the sensor vector) / int new_value_bit(value to be set)
* Output: none
*******************************************************************************/
void setBitValue(uInt8  *variable, int n_bit, int new_value_bit)
// given a byte value, set the n bit to value
{
	uInt8  mask_on = (uInt8)(1 << n_bit);
	uInt8  mask_off = ~mask_on;
	if (new_value_bit)  *variable |= mask_on;
	else                *variable &= mask_off;
}

/******************************************************************************
* Function Name: erase_car
* Description: Erases a car in the pallete matrix
* Type: Void
* Input: int x(X coordinate position) /int z(Z coordinate position)
* Output: none
*******************************************************************************/
void erase_car(int x, int z) {
	car[x - 1][z - 1].inside = false;
	car[x - 1][z - 1].time_parked = time(NULL);
	car[x - 1][z - 1].plate_number = (-1);
}

/******************************************************************************
* Function Name: inic_Warehouse
* Description: Initializes the structures
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void inic_Warehouse() {
	for (int x = 1; x <= 3; x++) {
		for (int z = 1; z <= 3; z++) {
			erase_car(x, z);
		}
	}
	warehouse.money_earned = 0;
	warehouse.bool_restart = true;
}

/******************************************************************************
* Function Name: new_car
* Description: Stores a new car in the pallete matrix
* Type: Void
* Input: int x(X coordinate position) /int z(Z coordinate position)  / time_t now (time the car was parked) / int plate_number
* Output: none
*******************************************************************************/
void add_car(int x, int z, time_t now, int plate_number) {
	car[x - 1][z - 1].inside = true;
	car[x - 1][z - 1].time_parked = now;
	car[x - 1][z - 1].plate_number = plate_number;
}

/******************************************************************************
* Function Name: is_car_inside
* Description: Sees if there is a car in the specific position
* Type: Bool
* Input: int x(X coordinate position) /int z(Z coordinate position)
* Output: true (there is a car) / false (there is no car)
*******************************************************************************/
bool is_car_inside(int x, int z) {
	return car[x - 1][z - 1].inside;
}

/******************************************************************************
* Function Name: is_car_inside
* Description: Sees if there is a car with the plate number in the warehouse
* Type: Bool
* Input: int plate
* Output: true (there is a car) / false (there is no car)
*******************************************************************************/
bool is_plate_number_inside(int plate) {
	for (int i = 0; i <= 2; i++) {
		for (int j = 0; j <= 2; j++) {
			if (car[i][j].inside == true && car[i][j].plate_number == plate) return true;	
		}
	}
	return false;
}

/******************************************************************************
* Function Name: car_in_cage
* Description: Sees if there is a car i the cage
* Type: Bool
* Input: none
* Output: true (there is a car) / false (there is no car)
*******************************************************************************/
bool car_in_cage() {
	uInt8 p = ReadDigitalU8(1);
	return (getBitValue(p, 4));
}

/******************************************************************************
* Function Name: move_x_left
* Description: Moves the cursor left in the coordinate X
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void move_x_left()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2);  //  read port 2
	setBitValue(&p, 6, 1); //  set bit 6 to high level
	setBitValue(&p, 7, 0); //set bit 7 to low level
	WriteDigitalU8(2, p);  //  update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: move_x_right
* Description: Moves the cursor right in the coordinate X
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void move_x_right()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //read port 2
	setBitValue(&p, 6, 0);    //  set bit 6 to  low level
	setBitValue(&p, 7, 1);      //set bit 7 to high level
	WriteDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: move_y_inside
* Description: Moves the cursor inside in the coordinate Y
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void move_y_inside()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //  read port 2
	setBitValue(&p, 4, 0);     //  set bit 4 to low level
	setBitValue(&p, 5, 1);      //set bit 5 to high level
	WriteDigitalU8(2, p); //  update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: move_y_outside
* Description: Moves the cursor outside in the coordinate Y
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void move_y_outside()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //read port 2
	setBitValue(&p, 4, 1);    //  set bit 4 to high level
	setBitValue(&p, 5, 0);      //set bit 5 to low level
	WriteDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: move_z_up
* Description: Moves the cursor up in the coordinate Z
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void move_z_up()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //  read port 2
	setBitValue(&p, 2, 0);     //  set bit 2 to  low level
	setBitValue(&p, 3, 1);      //set bit 3 to high level
	WriteDigitalU8(2, p); //  update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: move_z_down
* Description: Moves the cursor down in the coordinate Z
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void move_z_down()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //read port 2
	setBitValue(&p, 2, 1);    //  set bit 2 to high level
	setBitValue(&p, 3, 0);      //set bit 3 to low level
	WriteDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: stop_x
* Description: Stops the cursor in the coordinate X
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void stop_x()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //read port 2
	setBitValue(&p, 6, 0);   //  set bit 6 to  low level
	setBitValue(&p, 7, 0);   //set bit 7 to low level
	WriteDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: stop_y
* Description: Stops the cursor in the coordinate Y
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void stop_y()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //read port 2
	setBitValue(&p, 4, 0);   //  set bit 4 to  low level
	setBitValue(&p, 5, 0);   //set bit 5 to low level
	WriteDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: stop_z
* Description: Stops the cursor in the coordinate Z
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void stop_z()
{
	uInt8 p;
	taskENTER_CRITICAL();  //No other tasks or int can run before exiting the critical code
	p = ReadDigitalU8(2); //read port 2
	setBitValue(&p, 2, 0);   //  set bit 2 to  low level
	setBitValue(&p, 3, 0);   //set bit 3 to low level
	WriteDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

/******************************************************************************
* Function Name: get_x_pos
* Description: Gets the position of the coordinate X
* Type: Int
* Input: none
* Output: int(X position)
*******************************************************************************/
int get_x_pos()
{
	uInt8 p = ReadDigitalU8(0);
	if (!getBitValue(p, 2))
		return 1;
	if (!getBitValue(p, 1))
		return 2;
	if (!getBitValue(p, 0))
		return 3;
	return(-1);
}

/******************************************************************************
* Function Name: get_y_pos
* Description: Gets the position of the coordinate Y
* Type: Int
* Input: none
* Output: int(Y position)
*******************************************************************************/
int get_y_pos()
{
	uInt8 p = ReadDigitalU8(0);
	if (!getBitValue(p, 5))
		return 1;
	if (!getBitValue(p, 4))
		return 2;
	if (!getBitValue(p, 3))
		return 3;
	return(-1);
}

/******************************************************************************
* Function Name: get_z_pos
* Description: Gets the position of the coordinate Z
* Type: Int
* Input: none
* Output: int(Z position)
*******************************************************************************/
int get_z_pos()
{
	uInt8 p0 = ReadDigitalU8(0);
	uInt8 p1 = ReadDigitalU8(1);
	if (!getBitValue(p1, 3))
		return 1;
	if (!getBitValue(p1, 1))
		return 2;
	if (!getBitValue(p0, 7))
		return 3;
	return(-1);
}

/******************************************************************************
* Function Name: valid_position
* Description: Sees if the cage is in a valid position
* Type: Bool
* Input: none
* Output: bool(true if (X,Y,Z) is different from (-1))
*******************************************************************************/
bool valid_position() {
	return (get_x_pos() != (-1) && get_y_pos() != (-1) && get_z_pos() != (-1));
}

/******************************************************************************
* Function Name: goto_x
* Description: Goes to the position given in the coordinate X
* Type: Void
* Input: x(X position)
* Output: none
*******************************************************************************/
void goto_x(int x) {
	if (get_x_pos() < x)
		move_x_right();
	else if (get_x_pos() > x)
		move_x_left();
	//   while position not reached
	while (get_x_pos() != x && warehouse.bool_restart)
		Sleep(1);
	stop_x();
}

void goto_x_task(void *param)
{
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			int x;
			xQueueReceive(mbx_x, &x, portMAX_DELAY);
			goto_x(x);
			xSemaphoreGive(sem_x_done);
		}
	}
}

/******************************************************************************
* Function Name: goto_y
* Description: Goes to the position given in the coordinate Y
* Type: Void
* Input: y(Y position)
* Output: none
*******************************************************************************/
void goto_y(int y) {
	if (get_y_pos() < y)
		move_y_inside();
	else if (get_y_pos() > y)
		move_y_outside();
	//   while position not reached
	while (get_y_pos() != y && warehouse.bool_restart)
		Sleep(1);
	stop_y();
}

/******************************************************************************
* Function Name: goto_z
* Description: Goes to the position given in the coordinate Z
* Type: Bool
* Input: z(Z position)
* Output: none
*******************************************************************************/
void goto_z(int z) {
	if (get_z_pos() < z)
		move_z_up();
	else if (get_z_pos() > z)
		move_z_down();
	//   while position not reached
	while (get_z_pos() != z && warehouse.bool_restart)
		Sleep(1);
	stop_z();
}

void goto_z_task(void *param)
{
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			int z;
			xQueueReceive(mbx_z, &z, portMAX_DELAY);
			goto_z(z);
			xSemaphoreGive(sem_z_done);
		}
	}
}

/******************************************************************************
* Function Name: goto_xz
* Description: Goes to the position given in the coordinate (X,Z)
* Type: Void
* Input: x(X position) / z(Z position), _wait_till_done(bool if is to wait for another task)
* Output: none
*******************************************************************************/
void goto_xz(int x, int z, bool _wait_till_done) {
	//goto_x(x);
	//goto_z(z);
	TPosition pos;
	pos.x = x;
	pos.z = z;
	xQueueSend(mbx_xz, &pos, portMAX_DELAY);
	if (_wait_till_done) {
		while (get_x_pos() != x && warehouse.bool_restart)  vTaskDelay(1);
		while (get_z_pos() != z && warehouse.bool_restart)  vTaskDelay(1);
	}
}

void goto_xz_task(void *param)
{
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			TPosition position;
			//wait until a goto request is received
			xQueueReceive(mbx_xz, &position, portMAX_DELAY);
			//send request for each goto_x_task and goto_z_task 
			xQueueSend(mbx_x, &position.x, portMAX_DELAY);
			xQueueSend(mbx_z, &position.z, portMAX_DELAY);
			//wait for goto_x completion
			xSemaphoreTake(sem_x_done, portMAX_DELAY);
			//wait for goto_z completion           
			xSemaphoreTake(sem_z_done, portMAX_DELAY);	
		}
	}
}

/******************************************************************************
* Function Name: is_z_top
* Description: Sees if the cursor is in the top part in the coordinate Z
* Type: Bool
* Input: none
* Output: bool(if cage is at top or not)
*******************************************************************************/
bool is_z_top() {
	int p0 = ReadDigitalU8(0);
	int p1 = ReadDigitalU8(1);
	if (!getBitValue(p0, 6) || !getBitValue(p1, 0) || !getBitValue(p1, 2)) return true;
	return false;
}

/******************************************************************************
* Function Name: is_z_low
* Description: Sees if the cursor is in the lower part in the coordinate Z
* Type: Bool
* Input: none
* Output: bool(if cage is at low or not)
*******************************************************************************/
bool is_z_low() {
	int p0 = ReadDigitalU8(0);
	int p1 = ReadDigitalU8(1);
	if (!getBitValue(p0, 7) || !getBitValue(p1, 1) || !getBitValue(p1, 3)) return true;
	return false;
}

/******************************************************************************
* Function Name: goto_up_level
* Description: The cursor goes one level up
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void goto_up_level() {
	if (!is_z_top())
		move_z_up();
	while (!is_z_top() && warehouse.bool_restart)
		taskYIELD();
	stop_z();
}

/******************************************************************************
* Function Name: goto_down_level
* Description: The cursor goes one level down
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void goto_down_level() {
	if (!is_z_low())
		move_z_down();
	while (!is_z_low() && warehouse.bool_restart)
		taskYIELD();
	stop_z();
}

/******************************************************************************
* Function Name: put_piece
* Description: Puts the piece into the specified cell in the warehouse
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void put_piece() {
	int led_state = 1;
	xQueueSend(mbx_flash1, &led_state, portMAX_DELAY); //Turn ON flashing LED

	// 1) Go one level up 
	goto_up_level();
	// 2) Put the platform inside
	if (!warehouse.bool_restart) return;
	goto_y(3);
	// 3) Go one level down
	if (!warehouse.bool_restart) return;
	goto_down_level();
	// 4) Put the platform outside
	if (!warehouse.bool_restart) return;
	goto_y(2);

	led_state = 0;
	xQueueSend(mbx_flash1, &led_state, portMAX_DELAY); //Turn OFF flashing LED
}

/******************************************************************************
* Function Name: get_piece
* Description: Gets the piece from the specified cell in the warehouse
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void get_piece() {
	int led_state = 1;
	xQueueSend(mbx_flash2, &led_state, portMAX_DELAY); //Turn ON flashing LED

	// 1) Put the platform inside
	goto_y(3);
	// 2) Go one level up
	if (!warehouse.bool_restart) return;
	goto_up_level();
	// 3) Put the platform outside
	if (!warehouse.bool_restart) return;
	goto_y(2);
	// 4) Go one level down
	if (!warehouse.bool_restart) return;
	goto_down_level();

	led_state = 0;
	xQueueSend(mbx_flash2, &led_state, portMAX_DELAY); //Turn OFF flashing LED
}

/******************************************************************************
* Function Name: calibration
* Description: Sets the cage to a calibrated position (semi-automatic calibration)
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void calibration() {
	
	if (get_z_pos() != 1 && get_z_pos() == -1) {
		move_z_down();
		Sleep(100);
		while (get_z_pos() == -1) Sleep(1);
		stop_z();
	}
	if (get_x_pos() != 3 && get_x_pos() == -1) {
		move_x_right();
		Sleep(100);
		while (get_x_pos() == -1) Sleep(1);
		stop_x();
	}
}

/******************************************************************************
* Function Name: option_put_piece
* Description: Puts the piece in the warehouse if there is conditions and updates the structure
* Type: Void
* Input: int plate_switch(plate number from the switch1 pulses, inicialized with (-1) for normal put_piece)
* Output: none
*******************************************************************************/
void option_put_piece(int plate_switch) {
	char buff[100];
	if (!valid_position()) {
		printf("\nPosition of cage not valid, please calibrate");
	}
	else if (is_car_inside(get_x_pos(), get_z_pos())) {
		printf("\nA car is already inside");
	}
	else if (!car_in_cage()) {
		printf("\nThere is no car in the cage");
	}
	else {
		int plate_number = 0;

		if (plate_switch == (-1)) { //cmd activated
			do {
				printf("\nInsert valid plate number: ");
				xQueueReceive(mbx_input, &plate_number, portMAX_DELAY);
				plate_number = plate_number - '0';  //plate_number must be unique and valid
			} while (is_plate_number_inside(plate_number) || (plate_number<0 || plate_number>9));
		}
		else {
			plate_number = plate_switch; //switch activated
			if (is_plate_number_inside(plate_number)) {
				printf("\nPlate number is not valid");
				return;
			}
		}

		printf("\nPutting piece...\n");
		put_piece();
		if (!warehouse.bool_restart) return;

		time_t now = time(0); //Gets the current time
		strftime(buff, 100, "%d-%m-%Y %H:%M:%S", localtime(&now));
		add_car(get_x_pos(), get_z_pos(), time(&now), plate_number);
		printf("\nInserted car in cell(%d,%d) with plate number %d parked at %s", get_x_pos(), get_z_pos(), plate_number, buff);
	}
}

/******************************************************************************
* Function Name: option_get_piece
* Description: Gets the piece from the warehouse if there is conditions and updates the structure
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void option_get_piece() {
	std::string parking_services;
	char buff[100];
	if (!valid_position()) {
		printf("\nPosition of cage not valid, please calibrate");
	}
	else if (!is_car_inside(get_x_pos(), get_z_pos())) {
		printf("\nThere is not car inside");
	}
	else if (car_in_cage()) {
		printf("\nThere is a car in the cage");
	}
	else if (get_y_pos() != 2) {
		printf("\nPlate must be on the cage");
	}
	else {
		printf("\nGetting piece...\n");
		get_piece();
		if (!warehouse.bool_restart) return;

		double time_parked = difftime(time(0), 0) - difftime(car[get_x_pos() - 1][get_z_pos() - 1].time_parked, 0); //time parked in the warehouse
		printf("\nTaken car in cell(%d,%d) with plate number %d parked for %.0f sec, costing %.0f euros", get_x_pos(), get_z_pos(), car[get_x_pos() - 1][get_z_pos() - 1].plate_number, time_parked, (time_parked*COST));

		strftime(buff, 100, "%d-%m-%Y %H:%M:%S", localtime(&car[get_x_pos() - 1][get_z_pos() - 1].time_parked));
		parking_services = "Car with plate number " + std::to_string(car[get_x_pos() - 1][get_z_pos() - 1].plate_number) + ", parked at " + buff + " for " + std::to_string((int)(time_parked)) + "sec, costing " + std::to_string((int)(time_parked * COST)) + " euros";
		warehouse.list_car_finished.push_back(parking_services); //adds to the list of finished parking services
		warehouse.money_earned += (time_parked*COST); //adds to the total money earned
		erase_car(get_x_pos(), get_z_pos());
	}
}

/******************************************************************************
* Function Name: flashLED
* Description: Flashes the parking LEDs
* Type: Void
* Input: int port(port of the LED, it can be 2(down LED) or 1(up LED))
* Output: none
*******************************************************************************/
void flashLED(int port) {
	if (port == 2 || port == 1) {
		uInt8 p = ReadDigitalU8(2);
		setBitValue(&p, port - 1, 1);
		WriteDigitalU8(2, p);
		Sleep(100);
		p = ReadDigitalU8(2);
		setBitValue(&p, port - 1, 0);
		WriteDigitalU8(2, p);
		Sleep(100);
	}
}

/******************************************************************************
* Function Name: flash_led1_task
* Description: Flashes the parking LEDs when receives a msg and stop when receives to turn off
* Type: Void
* Input: void *param
* Output: none
*******************************************************************************/
void flash_led1_task(void *param) {
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			int msg;
			int delay_time = 50;
			//waits for a msg to flash the led
			xQueueReceive(mbx_flash1, &msg, portMAX_DELAY);

			if (msg == 2) { // It is an emergency stop
				delay_time = 10;
			}
			while (msg != 0) { //until receives a msg saying to turn off the led
				flashLED(1);
				Sleep(delay_time);
				xQueueReceive(mbx_flash1, &msg, 0);
			}
		}
	}
}

void flash_led2_task(void *param) {
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			int msg;
			int delay_time = 50;
			//waits for a msg to flash the led
			xQueueReceive(mbx_flash2, &msg, portMAX_DELAY);

			if (msg == 2) {  // It is an emergency stop
				delay_time = 10;
			}
			while (msg != 0) { //until receives a msg saying to turn off the led
				flashLED(2);
				Sleep(delay_time);
				xQueueReceive(mbx_flash2, &msg, 0);
			}
		}
	}
}

/******************************************************************************
* Function Name: readSwitch
* Description: Reads the value of the parking Switchs
* Type: Int
* Input: int port(port of the LED, it can be 0(right LED) or 1(left LED))
* Output: 1(is activated) or 0(is disabled)
*******************************************************************************/
int readSwitch(int port) {
	uInt8 p = ReadDigitalU8(1);
	return getBitValue(p, (port + 4));
}

/******************************************************************************
* Function Name: switchES_task
* Description: Updates the semaphore if both buttons are pressed, enables emergency_stop_task
* Type: Void
* Input: void *param
* Output: none
*******************************************************************************/
void switchES_task(void *param) {
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			if (readSwitch(2) && readSwitch(1)) {  //Emergency Stop			
				xSemaphoreGive(sem_sw_es_ON, portMAX_DELAY);  //adds counter to notifie that the two switches were pressed
				xSemaphoreTake(sem_sw_es_OFF, portMAX_DELAY); //waits until the emergency stop is finished 
			}
		}
	}
}

void emergency_stop_task(void *param) {
	while (true) {
		taskYIELD();
		xSemaphoreTake(sem_sw_es_ON, portMAX_DELAY); //waits until the two switches are pressed
		xSemaphoreTake(sem_me_sw, portMAX_DELAY); //start of mutual exclusion with the switch_task
		warehouse.stop_state = ReadDigitalU8(2);
		stop_x();
		stop_z();
		stop_y();

		int led_state = 2;
		xQueueSend(mbx_flash1, &led_state, portMAX_DELAY); //sends msg to flash the two leds
		xQueueSend(mbx_flash2, &led_state, portMAX_DELAY);

		while (readSwitch(1) || readSwitch(2)) Sleep(1); 
		while (true) {
			if (readSwitch(1) && !readSwitch(2)) { //Resume Operation
				WriteDigitalU8(2, warehouse.stop_state);
				vTaskDelay(100);
				break;
			}
			if (readSwitch(2) && !readSwitch(1)) { //Reset System Operation
				warehouse.bool_restart = false;
				vTaskDelay(100);
				warehouse.bool_restart = true;
				taskYIELD();
				if (get_y_pos() == -1) {
					move_y_outside();
					Sleep(100);
					while (get_y_pos() == -1) Sleep(1);
					stop_y();
				}
				calibration();
				goto_y(2);
				if (car_in_cage()) goto_y(1);
				break;
			}
		}
		while (readSwitch(1) || readSwitch(2)) Sleep(1);
		xSemaphoreGive(sem_me_sw, portMAX_DELAY); //end of mutual exclusion with the switch_task

		led_state = 0;
		xQueueSend(mbx_flash1, &led_state, portMAX_DELAY); //sends msg to turn off the two leds
		xQueueSend(mbx_flash2, &led_state, portMAX_DELAY);
		xSemaphoreGive(sem_sw_es_OFF, portMAX_DELAY); //emergency stop is finished
	}
}

/******************************************************************************
* Function Name: switchES_task
* Description: Updates the semaphore if one of the buttons are pressed, enables switch_get_put_task
* Type: Void
* Input: void *param
* Output: none
*******************************************************************************/
void switch_task(void *param) {
	int sw_button;
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			xSemaphoreTake(sem_me_sw, portMAX_DELAY); //start of mutual exclusion with the switchES_task

			if (readSwitch(1) && !readSwitch(2)) {  //Put Piece
				sw_button = 1;
				xQueueSend(mbx_put_get, &sw_button, portMAX_DELAY); //sends the button pressed
				xSemaphoreTake(sem_sw_12, portMAX_DELAY); //waits until put piece is finished
			}
			if (readSwitch(2) && !readSwitch(1)) { //Get Piece
				sw_button = 2;
				xQueueSend(mbx_put_get, &sw_button, portMAX_DELAY); //sends the button pressed
				xSemaphoreTake(sem_sw_12, portMAX_DELAY); //waits until put piece is finished
			}

			xSemaphoreGive(sem_me_sw, portMAX_DELAY); //end of mutual exclusion with the switchES_task
		}
	}
}

void switch_get_put_task(void *param) {
	int sw_button;
	int i, j;
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			xQueueReceive(mbx_put_get, &sw_button, portMAX_DELAY); //receives the button pressed
			int pulse_counter = 0; //number of times the switch button was pressed
			bool bool_get_car = false;

			if (sw_button == 1) printf("\nClick the switch1 button to give the plate number of the car to be stored");
			if (sw_button == 2) printf("\nClick the switch2 button to give the plate number of the car to be retrieved");

			//Reads Pulses from the Switch
			time_t curr = time(0); //last time the switch was pressed
			while ((difftime(time(0), 0) - difftime(curr, 0)) < 10) { //while the difference of time between two switches pressed is 10
				vTaskDelay(5);
				int antes = readSwitch(sw_button);
				int depois = readSwitch(sw_button);
				while ((difftime(time(0), 0) - difftime(curr, 0)) < 10) {
					if (antes == 0 && depois != 0) {
						pulse_counter++;
						curr = time(0);
					}
					if (pulse_counter == 9) break; //max plate number
					antes = depois;
					depois = readSwitch(sw_button);
				}
			}
			printf("\nClick switch%d timeout, plate number %d", sw_button, pulse_counter);

			if (sw_button == 1) { //PUT PIECE
				option_put_piece(pulse_counter);
			}

			if (sw_button == 2) { //GET PIECE
				for (int x = 1; x <= 3; x++) {
					for (int z = 1; z <= 3; z++) {
						if (car[x - 1][z - 1].plate_number == pulse_counter) {
							bool_get_car = true;
							goto_xz(x, z, true);
							option_get_piece();
						}
					}
				}
				if (!bool_get_car) printf("\nThere was no car inside with the given plate number\n");
			}
			xSemaphoreGive(sem_sw_12, portMAX_DELAY); // put or get piece finished
		}
	}
}

/******************************************************************************
* Function Name: show_menu
* Description: Shows the menu
* Type: Void
* Input: none
* Output: none
*******************************************************************************/
void show_menu()
{
	printf("\nKeyboard: | (Q) | (W) | (E) | (R) | (T) | (Y) |  U  |  I  |  O  |  P  |");
	printf("\n          | (A) | (S) | (D) | (F) | (G) | (H) |  J  |  K  |  L  |  Ç  |");
	printf("\n          | (Z) | (X) | (C) | (V) | (B) |  N  |  M  |");
	printf("\nsw1 -> Put a car with a specific plate number in the current position");
	printf("\nsw2 -> Retrieve a car with a specific plate number");
	printf("\nsw1 AND sw2 -> EMERGENCY STOP");
	printf("\nsw1 in EMERGENCY STOP -> RESUME");
	printf("\nsw2 in EMERGENCY STOP -> RESET SYSTEM");
	printf("\nA:Go Left / D:Go Right / W:Go Up / S:Go Down / Q:Go Inside / E:Go Outside");
	printf("\nR:Resume / F:Stop / T:Terminate / G:Goto (X,Z) / Y:Get Piece / H:Put Piece");
	printf("\nZ:List of parked cars / X:Car plate search / C:Cell position search");
	printf("\nV:List of finished cars / B:Total earned");
}

/******************************************************************************
* Function Name: task_storage_services
* Description: Contains all the options that can be done in the simulator
* Type: Void
* Input: void *param
* Output: none
*******************************************************************************/
void task_storage_services(void *param)
{
	int cmd = -1;
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			bool bool_found = false;
			double duration;
			char buff[100];
			time_t now;
			int led_state;
			// get selected option from keyboard
			show_menu();
			printf("\n\nEnter option=");
			xQueueReceive(mbx_input, &cmd, portMAX_DELAY);
			//
			// Go Left
			//
			if (tolower(cmd) == 'a') {
				if (get_y_pos() != 2) {
					printf("\nCage must be available to move");
				}
				else if (get_x_pos() != 1) {
					printf("\nMoving left...");
					move_x_left();
					Sleep(100);
					while (get_x_pos() == -1 && warehouse.bool_restart) Sleep(1);
					stop_x();
				}
			}
			//
			// Go Right
			//
			if (tolower(cmd) == 'd') {
				if (get_y_pos() != 2) {
					printf("\nCage must be available to move");
				}
				else if (get_x_pos() != 3) {
					printf("\nMoving right...");
					move_x_right();
					Sleep(100);
					while (get_x_pos() == -1 && warehouse.bool_restart) Sleep(1);
					stop_x();
				}
			}
			//
			// Go Up
			//
			if (tolower(cmd) == 'w') {
				if (get_y_pos() != 2) {
					printf("\nCage must be available to move");
				}
				else if (get_z_pos() != 3) {
					printf("\nMoving up...");
					move_z_up();
					Sleep(100);
					while (get_z_pos() == -1 && warehouse.bool_restart) Sleep(1);
					stop_z();
				}
			}
			//
			// Go Down
			//
			if (tolower(cmd) == 's') {
				if (get_y_pos() != 2) {
					printf("\nCage must be available to move");
				}
				else if (get_z_pos() != 1) {
					printf("\nMoving down...");
					move_z_down();
					Sleep(100);
					while (get_z_pos() == -1 && warehouse.bool_restart) Sleep(1);
					stop_z();
				}
			}
			//
			// Go Inside 
			//
			if (tolower(cmd) == 'q') {
				if (get_y_pos() != 3) {
					printf("\n Moving inside...");
					move_y_inside();
					Sleep(100);
					while (get_y_pos() == -1 && warehouse.bool_restart) Sleep(1);
					stop_y();
				}
			}
			//
			// Go Outside
			//
			if (tolower(cmd) == 'e') {
				if (get_y_pos() != 1) {
					printf("\n Moving outside...");
					move_y_outside();
					Sleep(100);
					while (get_y_pos() == -1 && warehouse.bool_restart) Sleep(1);
					stop_y();
				}
			}
			//
			// Stop
			//
			if (tolower(cmd) == 'f') {
				printf("\nStoped");
				warehouse.stop_state = ReadDigitalU8(2);
				stop_x();
				stop_z();
				stop_y();
			}
			//
			// Resume
			//
			if (tolower(cmd) == 'r') {
				printf("\nResume");
				WriteDigitalU8(2, warehouse.stop_state);
			}
			//
			// Put Piece
			//
			if (tolower(cmd) == 'h') {
				option_put_piece(-1);
			}
			//
			// Get Piece
			//
			if (tolower(cmd) == 'y') {
				option_get_piece();
			}
			//
			// Goto (X,Z)
			//
			if (tolower(cmd) == 'g') {
				if (get_y_pos() != 2) {
					printf("\nCage must be available to move");
				}
				else {
					int x, z; // you can use scanf or one else you like
					printf("\nX=");
					xQueueReceive(mbx_input, &x, portMAX_DELAY);
					x = x - '0'; //convert from ascii code to number

					printf("\nZ=");
					xQueueReceive(mbx_input, &z, portMAX_DELAY);
					z = z - '0'; //convert from ascii code to number

					if (x >= 1 && x <= 3 && z >= 1 && z <= 3)
						goto_xz(x, z, true);
					else printf("\nWrong (x,z) coordinates, are you sleeping?...");
				}
			}
			//
			// Terminate
			//
			if (tolower(cmd) == 't')
			{
				printf("\nTerminated");
				WriteDigitalU8(2, 0); //stop all motores;
				vTaskEndScheduler(); // terminates application
			}
			//
			// List of parked cars
			//
			if (tolower(cmd) == 'z') {
				bool_found = false;
				printf("\nList of parked cars");
				for (int i = 0; i <= 2; i++) {
					for (int j = 0; j <= 2; j++) {
						if (car[i][j].inside) {
							bool_found = true;
							duration = difftime(time(0), 0) - difftime(car[i][j].time_parked, 0);
							printf("\nCar in cell(%d,%d) with plate number %d parked for %.0f sec, costing now %.0f euros", (i + 1), (j + 1), car[i][j].plate_number, duration, (duration * COST));
						}
					}
				}
				if (!bool_found) printf("\nThere isn't a single car parked");
			}
			//
			// Car plate search
			//
			if (tolower(cmd) == 'x') {
				bool_found = false;
				int plate;
				printf("\nInsert plate number: ");
				xQueueReceive(mbx_input, &plate, portMAX_DELAY);
				plate = plate - '0';

				printf("\nSearching for the car %d ...", plate);
				for (int i = 0; i <= 2; i++) {
					for (int j = 0; j <= 2; j++) {

						if (car[i][j].inside == true && car[i][j].plate_number == plate) {
							bool_found = true;
							strftime(buff, 100, "%d-%m-%Y %H:%M:%S", localtime(&car[i][j].time_parked));
							printf("\nThe car is in cell(%d,%d) since %s", (i + 1), (j + 1), buff);
						}
					}
				}
				if (!bool_found) printf("\nThat car isn't parked");
			}
			//
			// Cell position search
			//
			if (tolower(cmd) == 'c') {
				int x, z;
				printf("\nX=");
				xQueueReceive(mbx_input, &x, portMAX_DELAY);
				x = x - '0';

				printf("\nZ=");
				xQueueReceive(mbx_input, &z, portMAX_DELAY);
				z = z - '0';

				printf("\nSearching for the car in the cell(%d,%d) ...", x, z);
				if (is_car_inside(x, z))
				{
					duration = difftime(time(0), 0) - difftime(car[x - 1][z - 1].time_parked, 0);
					strftime(buff, 100, "%d-%m-%Y %H:%M:%S", localtime(&car[x - 1][z - 1].time_parked));
					printf("\nThe car has the plate number %d, parked since %s and for %.0f sec", car[x - 1][z - 1].plate_number, buff, duration);
				}
				else printf("\nThat car isn't parked");
			}
			//
			// List of finished cars
			//
			if (tolower(cmd) == 'v') {
				for (std::list<std::string>::const_iterator i = warehouse.list_car_finished.begin(); i != warehouse.list_car_finished.end(); ++i)
				{
					printf("\n%s", i->c_str());
				}
			}
			//
			// Total earned
			//
			if (tolower(cmd) == 'b') {
				printf("\nIt was acumulated %d euros", warehouse.money_earned);
			}
		}   // end while
	}
}

void receive_instructions_task(void *ignore) {
	int c = 0;
	while (true) {
		taskYIELD();
		while (warehouse.bool_restart) {
			c = _getwch();
			putchar(c);
			xQueueSend(mbx_input, &c, portMAX_DELAY);
		}
	}
}



/*********************************************************************MAIN*******************************************************/
void main(int argc, char **argv) {

	//                                      SENSOR VECTORS
	//           7           6            5         4        3        2        1         0
	// PO: [   3L(0)  |    3H(0)   |    Y1(0)   |  Y2(0) | Y3(0) |   A(0)  |   B(0)  |   C(0)  ]
	// P1: [ -------- | Switch2(1) | Switch1(1) | Car(1) | 1L(0) |  1H(0)  |  2L(0)  |  2H(0)  ]
	// P1: [ Right(1) |   Left(1 ) |    In(1)   | Out(1) | Up(0) | Down(0) | LedR(0) | LedL(0) ]

	//Automated Warehouse site: http://localhost:8081/parking.html 

	create_DI_channel(0);
	create_DI_channel(1);
	create_DO_channel(2);

	printf("\nwaiting for hardware simulator...");
	WriteDigitalU8(2, 0);
	printf("\ngot access to simulator...");

	sem_x_done = xSemaphoreCreateCounting(1000, 0);
	sem_z_done = xSemaphoreCreateCounting(1000, 0);
	sem_sw_12 = xSemaphoreCreateCounting(1000, 0);
	sem_sw_es_ON = xSemaphoreCreateCounting(1000, 0);
	sem_sw_es_OFF = xSemaphoreCreateCounting(1000, 0);
	sem_me_sw = xSemaphoreCreateCounting(1000, 1);

	mbx_x = xQueueCreate(10, sizeof(int));
	mbx_z = xQueueCreate(10, sizeof(int));
	mbx_xz = xQueueCreate(10, sizeof(TPosition));
	mbx_input = xQueueCreate(10, sizeof(int));
	mbx_flash1 = xQueueCreate(10, sizeof(int));
	mbx_flash2 = xQueueCreate(10, sizeof(int));
	mbx_put_get = xQueueCreate(10, sizeof(int));

	inic_Warehouse();
	calibration();

	xTaskCreate(goto_x_task, "goto_x_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_z_task, "goto_z_task", 100, NULL, 0, NULL);
	xTaskCreate(goto_xz_task, "v_gotoxz_task", 100, NULL, 0, NULL);
	xTaskCreate(switch_task, "switch_task", 100, NULL, 0, NULL);
	xTaskCreate(switchES_task, "switchES_task", 100, NULL, 0, NULL);
	xTaskCreate(switch_get_put_task, "switch_get_put_task", 100, NULL, 0, NULL);
	xTaskCreate(emergency_stop_task, "emergency_stop_task", 100, NULL, 0, NULL);
	xTaskCreate(flash_led1_task, "flash_led1_task", 100, NULL, 0, NULL);
	xTaskCreate(flash_led2_task, "flash_led2_task", 100, NULL, 0, NULL);
	xTaskCreate(task_storage_services, "task_storage_services", 100, NULL, 0, NULL);
	xTaskCreate(receive_instructions_task, "receive_instructions_task", 100, NULL, 0, NULL);
	vTaskStartScheduler();
}