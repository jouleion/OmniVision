# FreeRTOS in the /cpp Folder

This folder contains the implementation of tasks using FreeRTOS for an Arduino-based project. FreeRTOS is a real-time operating system that allows multitasking by creating and managing multiple tasks.

## Key Concepts

- **Tasks**: Independent threads of execution. Each task runs in its own context and can be assigned a priority.
- **pvParameters**: A pointer passed to a task during its creation. It can be used to pass data to the task. If there are no parameters, use NULL.
- **vTaskDelay**: A FreeRTOS function to block the current task for a specified time, allowing other tasks to execute.
- **portTICK_PERIOD_MS**: A constant that defines the duration of one tick in milliseconds. It is used to convert time delays into ticks, which are the basic unit of time in FreeRTOS.
- **Stack Depth**: The amount of stack memory allocated to a task, specified in words (not bytes). Each task requires its own stack to store local variables, function call information, and context during task switching. The value should be chosen based on the task's memory requirements.

## Variables in main.cpp

- **tofSensors**: A vector that holds multiple Time-of-Flight (TOF) sensor objects. Each sensor is initialized with a unique ID and pin number.
- **numSensors**: The number of TOF sensors to initialize. This value determines the size of the `tofSensors` vector.
- **pvParameters**: A parameter passed to tasks during their creation. It is currently unused (set to NULL) in this project.
- **portTICK_PERIOD_MS**: Used in `vTaskDelay` to specify the delay time in milliseconds. For example, `vTaskDelay(100 / portTICK_PERIOD_MS)` delays the task for 100 milliseconds.

## Tasks in this Project

1. **TaskReadTOF**: Reads data from the Time-of-Flight (TOF) sensors.
2. **TaskSendData**: Simulates sending data.
3. **TaskProcessData**: Simulates processing data.
4. **TaskActuation**: Simulates actuation based on processed data.

## How FreeRTOS Works

FreeRTOS uses a scheduler to manage task execution. Tasks with higher priority are executed first. If tasks have the same priority, they share CPU time in a round-robin fashion.

## Example

```cpp
xTaskCreate(TaskReadTOF, "ReadTOF", 100, NULL, 1, NULL);
```
- `TaskReadTOF`: The function to execute as a task.
- `"ReadTOF"`: A human-readable name for the task.
- `100`: Stack size allocated for the task.
- `NULL`: Pointer to parameters passed to the task (pvParameters).
- `1`: Priority of the task.
- `NULL`: Handle to the task (not used here).