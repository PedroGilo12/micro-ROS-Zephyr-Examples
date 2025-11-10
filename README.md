# micro-ROS Zephyr Examples

A collection of examples, tutorials, and common fixes for working with micro-ROS on Zephyr RTOS.

<p align="center">
  <img src="https://upload.wikimedia.org/wikipedia/commons/thumb/6/64/Zephyr_RTOS_logo_2015.svg/1200px-Zephyr_RTOS_logo_2015.svg.png" alt="Zephyr RTOS Logo" width="200" style="margin-right: 40px;"/>
  <img src="https://micro.ros.org/img/micro-ROS_big_logo.png" alt="micro-ROS Logo" width="300"/>
</p>


## About This Repository

This repository aims to serve as a comprehensive starting point for those interested in working with micro-ROS and Zephyr. As this is a highly specialized domain with limited resources available online, my goal is to provide clear and practical guidance for newcomers.

This project was inspired by my experience during a Robot Navigation course at university, where I decided to combine my growing knowledge of ROS with my background in embedded systems to explore this niche field.

Here, you will find examples, tutorials, and solutions to common issues that I have developed throughout my learning journey.

## Requirements
- ROS2 Jazzy (for running micro-ROS Agent)
- Python 3.10+  
- CMake 3.20.5+

## Zephyr Setup

### Install dependencies

#### 1. Use apt to install the required dependencies:
```bash
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

#### 2. Verify the versions of the main dependencies installed on your system by entering:

```bash
cmake --version
python3 --version
dtc --version
```

---

### Get Zephyr and install Python dependencies

#### 3. Create a new virtual environment
```bash
python3 -m venv ~/zephyrproject/.venv
```

#### 4. Activate the virtual environment
```bash
source ~/zephyrproject/.venv/bin/activate
```

Once activated, your shell prompt will be prefixed with `(.venv)`.  
You can deactivate the environment at any time by running:
```bash
deactivate
```

> **Note:**  
> Remember to activate the virtual environment every time you start working.

---

#### 5. Install West
```bash
pip install west
```

---

#### 4. Get the Zephyr source code
```bash
west init -l samples/
west update
```
---

#### 5. Export Zephyr CMake package
This allows CMake to automatically load the boilerplate code required for building Zephyr applications.
```bash
west zephyr-export
```

---

### 6. Install Python dependencies
Use West to install the required Python packages:
```bash
west packages pip --install
```
