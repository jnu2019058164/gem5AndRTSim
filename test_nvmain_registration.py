import m5
from m5.objects import *

# Test if NVMainMemInterface is available
print("Testing NVMainMemInterface registration...")

try:
    # Try to import NVMainMemInterface
    from m5.objects import NVMainMemInterface
    print("✓ NVMainMemInterface is available")
    
    # Try to create an instance
    mem_intf = NVMainMemInterface(
        config="RTSim/Config/RM.config",
        atomic_latency='30ns',
        atomic_variance='30ns',
        atomic_mode=False,
        NVMainWarmUp=False
    )
    print("✓ NVMainMemInterface instance created successfully")
    
    # Print some information
    print(f"  Memory type: {type(mem_intf).__name__}")
    print(f"  Config file: {mem_intf.config}")
    
    # Check if it's in the mem_list
    from configs.common.ObjectList import mem_list
    mem_names = mem_list.get_names()
    print(f"\nAvailable memory types:")
    for name in mem_names[:20]:  # Show first 20 for brevity
        print(f"  - {name}")
    
    if 'NVMainMemInterface' in mem_names:
        print("✓ NVMainMemInterface is in mem_list")
    else:
        print("✗ NVMainMemInterface is NOT in mem_list")
        print("  This is why --mem-type=NVMainMemInterface fails")
        
    print("\nTest completed successfully!")
    
except ImportError as e:
    print(f"✗ ImportError: {e}")
except Exception as e:
    print(f"✗ Error: {e}")

# Exit the simulator
m5.exit()
