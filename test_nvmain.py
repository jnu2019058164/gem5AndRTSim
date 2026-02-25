# Test script for NVMainMemInterface

import m5
from m5.objects import *

# Create a system
system = System()

# Set up the clock domain
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the memory
system.mem_ranges = [AddrRange('512MB')]

# Create the memory controller using NVMainMemInterface
from m5.objects import NVMainMemInterface

# Create memory interface
mem_intf = NVMainMemInterface(
    config="config.txt",
    atomic_latency='30ns',
    atomic_variance='30ns',
    atomic_mode=False,
    NVMainWarmUp=False
)

# Create memory controller
mem_ctrl = MemCtrl(dram=mem_intf)

# Create system bus
system.membus = SystemXBar()

# Connect memory controller to bus
mem_ctrl.port = system.membus.mem_side_ports

# Create a simple CPU
system.cpu = AtomicSimpleCPU()

# Connect CPU to bus
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

# Create root object and add system to it
root = Root(full_system=False)
root.system = system

# Set up the system
m5.instantiate()

print("System instantiated successfully!")
print(f"Memory interface type: {type(mem_intf).__name__}")
print(f"Memory controller type: {type(mem_ctrl).__name__}")

# Run for a few cycles
print("Running for 1000 cycles...")
m5.simulate(1000)

print("Simulation completed successfully!")
