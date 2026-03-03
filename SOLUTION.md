# EXTREME REVERSE ENGINEERING CHALLENGE - SOLUTION GUIDE

## **Challenge Information**

**Flag Format**: `BTCTF{SECRET}`  
**Secret Input**: `MayF0rc3BeW1thU!` (16 characters)  
**Flag**: `BTCTF{MayF0rc3BeW1thU!}`

**Popular Dialogue Reference**: *"May the Force be with you"* (Star Wars)

---

## **Solution Overview**

The correct input that passes all verification stages is:
```
MayF0rc3BeW1thU!
```

---

## **Steps to Solve the Challenge**

### **Method 1: Static Analysis (Hard Way)**

#### **Step 1: Defeat Anti-Debugging**
The binary contains multiple anti-debugging techniques:
- **IsDebuggerPresent()** - Patch or hook this Windows API call
- **CheckRemoteDebuggerPresent()** - Patch the call to always return FALSE
- **Hardware Breakpoint Detection** - Avoid using hardware breakpoints (Dr0-Dr3 registers)
- **Timing Checks** - Use time manipulation or patch out timing verification
- **INT3 Detection** - Use hardware breakpoints instead of software breakpoints
- **Window Title Detection** - Rename your analysis tool windows

**Tools**: x64dbg, IDA Pro, Ghidra, ScyllaHide (anti-anti-debug plugin)

#### **Step 2: Understand the Verification Pipeline**

The `verify()` function has 5 stages:

**Stage 1: Character Sum Check**
```cpp
char_sum % 13 == 0
```
- Sum of all character ASCII values must be divisible by 13
- For "MayF0rc3BeW1thU!": sum = 1391, 1391 % 13 = 0 ?

**Stage 2: Polymorphic Hash**
```cpp
(hash1 & 0xFFFF) == 0x8c3a
```
- Uses custom hash function `poly_hash()`
- Lower 16 bits must equal 0x8c3a

**Stage 3: White-Box Crypto**
```cpp
(enc_result & 0xFF) == 0x7E
```
- WhiteBoxAES encryption with lookup tables
- Lower 8 bits must equal 0x7E

**Stage 4: Control Flow Flattening**
```cpp
(flattened & 0xFFFFFFFF) == 0x3d8f9a2c
```
- Dispatcher-based state machine
- Lower 32 bits must equal 0x3d8f9a2c

**Stage 5: Ultimate Verification** (HARDEST)
```cpp
ultimate_verification(input, flattened)
```
- Multi-layer hash combination
- VM execution
- Final hash: 0x7a4f9c2e8b1d3f5aULL
- String comparison with hardcoded secret

#### **Step 3: Reverse Engineer Stage 5 (EXTREME DIFFICULTY)**

The ultimate verification function now uses:
1. Combines intermediate hash with poly_hash
2. Runs WhiteBoxAES encryption
3. Executes EncryptedVM (self-modifying code)
4. Creates position-dependent XOR chain
5. Computes final hash and compares to 0x7a4f9c2e8b1d3f5aULL

Key Discovery (MUCH HARDER NOW):
- Function is now called compute_validation_hash() (obfuscated name)
- Secret is split into 4 arrays with JUNK data interleaved:

  data_seg1[] = {0xAB, 0xCD, 0xE1, 0x12, 0x34, 0xF9, ...} // Real data at positions 2, 5, 8, 11
  data_seg2[] = {0x9A, 0x8B, 0x1D, 0x7C, 0x6D, 0x96, ...} // Real data at positions 2, 5, 8, 11
  data_seg3[] = {0x1F, 0x0E, 0x92, 0xFD, 0xEC, 0xE5, ...} // Real data at positions 2, 5, 8, 11
  data_seg4[] = {0x97, 0x86, 0xF4, 0x75, 0x64, 0xE8, ...} // Real data at positions 2, 5, 8, 11

- XOR key derivation is now MULTI-STAGE:

  obf_compute(k1, k2) ? stage1
  obf_compute(stage1, k3) ? stage2
  Complex bit shifts and XORs ? final key

- Extraction uses extract_byte(data, idx, 2) to skip junk bytes

---

Method 2: Dynamic Analysis (Easier)

Step 1: Bypass Anti-Debug
- Use ScyllaHide plugin with x64dbg
- Or use a virtual machine with anti-anti-debug patches

#### **Step 2: Set Breakpoints**
Place breakpoints at:
- `verify()` function entry
- String comparison operations (`strcmp`, `==` operator)
- Return statements

#### **Step 3: Trace Execution**
- Step through the verification stages
- Watch the stack and registers
- Look for string comparisons in memory

#### **Step 4: Find the Secret**
When you reach `ultimate_verification()`, you'll see:
- Memory contains: `"MayF0rc3BeW1thU!"`
- This is compared with your input

---

### **Method 3: Brute Force (Advanced)**

Given the constraints:
- Length: 16 characters
- Character sum % 13 = 0
- Printable ASCII (0x20 - 0x7E)
- Hash constraints at each stage

Write a brute-force script:
```python
import itertools

def check_constraints(s):
    if len(s) != 16:
        return False
    char_sum = sum(ord(c) for c in s)
    if char_sum % 13 != 0:
        return False
    # Add other hash checks...
    return True

# Brute force with constraints
# (This would take a very long time without optimization)
```

**Note**: Brute forcing is impractical due to the hash complexity.

## **Final Answer**

**Input**: `MayF0rc3BeW1thU!`  
**Flag**: `BTCTF{MayF0rc3BeW1thU!}`

The secret references the famous Star Wars quote: *"May the Force be with you!"*


**Skills Required**:
- Advanced x86/x64 assembly knowledge
- Windows internals and anti-debugging
- Cryptography and hash functions
- Patience and persistence

- Reverse engineering experience

- -


# Auth0r - Prajwal L Chawda
