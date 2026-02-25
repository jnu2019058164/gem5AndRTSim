# Simple test script for NVMainMemInterface

import m5
from m5.objects import *

# Test if NVMainMemInterface is available
print("Testing NVMainMemInterface...")
try:
    from m5.objects import NVMainMemInterface
    print("NVMainMemInterface imported successfully!")
    
    # Test creating an instance
    mem_intf = NVMainMemInterface(
        config="config.txt",
        atomic_latency='30ns',
        atomic_variance='30ns',
        atomic_mode=False,
        NVMainWarmUp=False
    )
    print("NVMainMemInterface instance created successfully!")
    
    # Test controller method
    print("Testing controller method...")
    mem_ctrl = mem_intf.controller()
    print(f"Controller created successfully! Type: {type(mem_ctrl).__name__}")
    
    print("All tests passed!")
except Exception as e:
    print(f"Error: {e}")
    import traceback
    traceback.print_exc()
