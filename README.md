# Automated Warehouse Controller

**Course:** Real-Time Systems

**Specialty Area:** Robotics

**Academic Year:** 2018/19

**Semester:** 1st

**Grade:** 20 out of 20

**Technologies Used:** C++, freeRTOS

**Brief Description:** Development of a program to perform the management of an automated warehouse, using a real-time framework. The tools necessary to develop the work comprise a real-time kernel (freeRTOS), Visual Studio C/C++ compiler, and the hardware kit that represents the warehouse. Other tools available for developing this work are a warehouse simulator and a set of low-level functions for software/hardware interaction.

Makes use of real-time system’s concepts, namely:

- Task / Process: concurrency,
- State, event, actions (triggered by events)
- Synchronization and communication between tasks
- Accessing shared resources / exclusive access resources

**Low-Level Requirements:**

- Physical interaction
- Move storage in each axis x, y, and z
- Goto cage to cell (x,z)
- Put in cell (x,z)
- get from cell (x,z)
- Joystick with keyboard
- Read Parking left Buttons
- Activate Parking lights (at left)

**High-Level requirements (operation):**

- Semi-automatic calibration of the parking kit.
- Parking a car (given the plate number); shows cell position and hour for the parked car in screen.
- Car leaves Parking (given the plate number); shows plate number, time_duration and cost in the screen
- While being operated (moving), if both left buttons are pressed, then perform emergency STOP
- During an emergency STOP, if up button is pressed, then RESUME operation
- During an emergency STOP, if down button is pressed, then Reset the system (perform semi-automatic calibration and if it has a car in cage, take it out and then resume from the begging)
- Left up light flashes while parking
- Left down light flashes while car leaves parking
- Both lights flash faster during an emergency STOP (and until resume)

**High-Level requirements (information):**

- Show list of parked cars (plate number, cell, time, cost)
- Given a plate number with the keyboard, shows the (x,z) coordinates and hour parked of the corresponding car in the screen
- Given a (x,z) coordinate with the keyboard, shows the corresponding car plate, hour, duration in the screen
- Shows the list of all (finished) parking services (plate number, date, duration, cost)
- Show the total earned in Euros from all (finished) parking services

**Non-Functional requirements:**

- While operating, the system can accumulate parking/leave Requests
- While operating, the system can provide information (the req. from the previous section)
- The system works well (one car per cell, not taking cars from empty cells)
- The system always moves within limits x,y,z
- The system moves along (x,z) only when y is stopped at center position (y==2)

---

### Demo [[link](https://drive.google.com/file/d/1JaC9oqbPb7YchkhQyfM83ATzbumLLCM7/view?usp=sharing)]

