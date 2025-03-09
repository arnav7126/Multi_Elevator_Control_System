# Multi-Elevator Control System Simulation

## Project Overview

The project involves writing a POSIX-compliant C program to control a set of elevators efficiently using shared memory and message queues. The goal is to minimize the number of turns and elevator movements required to serve passenger requests.

## Features

- **Multi-Process Communication**: Uses message queues to obtain authorization strings and communicate with helper processes.
- **Shared Memory Management**: Stores elevator states, passenger requests, and movement commands in shared memory.
- **Efficient Scheduling Algorithm**: Minimizes turns and elevator movements while handling all passenger requests.
- **POSIX Compliance**: Implements inter-process communication (IPC) for synchronization.
- **Solver Processes**: Uses multiple solver processes to determine correct authorization strings for moving elevators.
- **Threading for Performance**: Utilizes threads to handle multiple elevators concurrently, improving efficiency.

## How It Works

1. **Initialization**:

   - The program reads from `input.txt` to get the number of elevators, floors, and solver processes.
   - Connects to **shared memory** and **message queues** for inter-process communication.

2. **Processing Turns**:
   - The **main helper process** provides the current state of elevators and new passenger requests.
   - The program decides which elevators should move and whether passengers should be picked up or dropped off.
   - If an elevator has passengers, the program requests an **authorization string** from the **solver process**.
3. **Sending Commands**:
   - The program updates shared memory with elevator movement commands (`'u'`, `'d'`, `'s'`).
   - Picks up and drops off passengers as required.
   - Sends a message to the **main helper process** to verify authorization and process movements.
4. **Receiving Updates**:

   - The helper responds with the updated state of the elevators.
   - The process repeats until all passenger requests are fulfilled.

5. **Termination**:
   - Once all requests are completed, the helper sends a **termination signal**, and the program exits cleanly.

## Installation & Execution

### Prerequisites

- Ubuntu 22.04 or later
- GCC compiler with POSIX thread support

### Compilation

```sh
gcc solution.c -lpthread -o solution

gcc helper-program.c -lpthread -o helper
```

`testcase[t].txt` & `testcaseDetails.txt` should be in the same folder as your `solution.c` & `helper-program.c`

### Running the Program

```sh
./helper <test_case_number>
```

Example:

```sh
./helper 3
```

This will process `testcase3.txt`.

## Possible Errors

Sometimes you might get an error message saying **No space left on device**. This might be due to your system's message queue limit and/or shared memory limit being reached

### Clear Message Queues

Run the following command to list active message queues:

```sh
ipcs -q
```

Run the following command to clear all queues owned by your user:

```sh
for id in $(ipcs -q | awk '{print $2}'); do ipcrm -q $id; done
```

### Clear Shared Memory

Run the following command to list active shared memory:

```sh
ipcs -m
```

Run the following command to clear all shared memory owned by your user:

```sh
for id in $(ipcs -m | awk 'NR>3 {print $2}'); do ipcrm -m $id; done
```

Now it should work.
