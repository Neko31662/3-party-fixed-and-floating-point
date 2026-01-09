# Three-Party Fixed and Floating Point MPC Protocol

This repository implements secure multi-party computation (MPC) protocols for three-party fixed-point and floating-point arithmetic operations.

## Compilation

### Environment Setup
First, run the environment setup script to install the required EMP-toolkit dependencies:

```bash
./setup_env_and_build.sh
```

### Build with CMake
Build the project using CMake:

```bash
mkdir -p build && cd build
cmake ..
make
```

## Testing

### Multi-Party Test Runner: `run_multiparty_test.sh`

Runs a three-party MPC test for any specified test program.

**Usage:**
```bash
./run_multiparty_test.sh <test_program_name>
```

**Features:**
- Starts 3 parties automatically
- Manages logs and process tracking
- Returns success/failure status

### Network Simulation: `network_simulation.sh`

Simulates different network conditions while running multi-party tests.

**Usage:**
```bash
./network_simulation.sh <network_type> <test_program_name>
```

**Parameters:**
- `<network_type>`: `LAN` (256 Mbps, 2ms RTT) or `WAN` (100 Mbps, 20ms RTT)
- `<test_program_name>`: Name of the test executable

**Requirements:** Requires `sudo` privileges for network traffic control.

## Usage Examples

### Basic Tests
```bash
# Build and run basic functionality test
cd build && make floating_point_test
./run_multiparty_test.sh floating_point_test
```

### Performance Tests
```bash
# Build performance tests
cd build && make speed_floating_add_test speed_fixed_mult_SOTA_test

# Run under different network conditions
./network_simulation.sh LAN speed_floating_add_test
./network_simulation.sh WAN speed_fixed_mult_SOTA_test
```
