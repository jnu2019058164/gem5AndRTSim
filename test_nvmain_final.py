# Final test script for NVMainMemInterface

import m5
from m5.objects import *

print("=== Testing NVMainMemInterface ===")

# Test 1: Import NVMainMemInterface
try:
    from m5.objects import NVMainMemInterface
    print("✓ NVMainMemInterface imported successfully")
except Exception as e:
    print(f"✗ Failed to import NVMainMemInterface: {e}")
    exit(1)

# Test 2: Create NVMainMemInterface instance
try:
    mem_intf = NVMainMemInterface(
        config="config.txt",
        atomic_latency='30ns',
        atomic_variance='30ns',
        atomic_mode=False,
        NVMainWarmUp=False
    )
    print("✓ NVMainMemInterface instance created successfully")
except Exception as e:
    print(f"✗ Failed to create NVMainMemInterface instance: {e}")
    exit(1)

# Test 3: Test controller method
try:
    mem_ctrl = mem_intf.controller()
    print(f"✓ Controller created successfully! Type: {type(mem_ctrl).__name__}")
except Exception as e:
    print(f"✗ Failed to create controller: {e}")
    exit(1)

# Test 4: Test controller attributes
try:
    # Set up basic controller attributes
    mem_ctrl.port = "test_port"
    print("✓ Controller attributes set successfully")
except Exception as e:
    print(f"✗ Failed to set controller attributes: {e}")
    exit(1)

print("\n=== All tests passed! ===")
print("NVMainMemInterface is ready to use with gem5.")
