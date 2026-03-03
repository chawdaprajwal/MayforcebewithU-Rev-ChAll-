# EXTREME REVERSE ENGINEERING CTF - Solution Guide

## 🎯 Challenge Overview

**Challenge Name:** Extreme Reverse Engineering CTF  
**Difficulty:** 8.5/10 (Expert Level)  
**Category:** Reverse Engineering, Anti-Debug, Obfuscation  
**Platform:** Windows (x86/x64)  
**Binary:** `challenge.exe`

---

## 🚩 **THE SOLUTION**

### **FLAG: `BTCTF{MayF0rc3BeW1thU!}`**

**Secret Key:** `MayF0rc3BeW1thU!`

This is a leet-speak version of the famous Star Wars quote: **"May the Force be with you!"**
- M**ay** **F**0**rc**3 **Be** **W**1**th** **U**!
- Substitutions: o→0, e→3, i→1

---

## 📋 Quick Start

### **Test the Solution:**
```bash
# Run the challenge
.\challenge.exe

# Enter the secret key when prompted
MayF0rc3BeW1thU!
```

### **Expected Output:**
```
========================================
    EXTREME REVERSE ENGINEERING CTF
========================================

  [!] Anti-Debug: ENABLED
  [!] Self-Modifying Code: ACTIVE
  [!] White-Box Crypto: ARMED
  [!] Difficulty: INSANE

Enter the secret key: MayF0rc3BeW1thU!

  ███████ ██    ██  ██████  ██████ ███████ ███████ ███████ 
  ██      ██    ██ ██      ██      ██      ██      ██      
  ███████ ██    ██ ██      ██      █████   ███████ ███████ 
       ██ ██    ██ ██      ██      ██           ██      ██ 
  ███████  ██████   ██████  ██████ ███████ ███████ ███████ 

  [✓] CONGRATULATIONS! You've cracked it!

  FLAG: BTCTF{MayF0rc3BeW1thU!}

  You are a true reverse engineering master!
  The Force is strong with you.
```

---

## 🔍 Challenge Analysis

### **Protection Layers:**

#### **1. Anti-Debugging (9/10 Difficulty)**
- ✅ `IsDebuggerPresent()` - Basic debugger detection
- ✅ `CheckRemoteDebuggerPresent()` - Remote debugger check
- ✅ PEB `BeingDebugged` flag check
- ✅ PEB `NtGlobalFlag` check (0x70)
- ✅ Hardware breakpoint detection (Dr0-Dr3)
- ✅ Timing checks (5 locations, 50-500ms thresholds)
- ✅ Parent process detection (x64dbg, OllyDbg, WinDbg)
- ✅ Binary integrity check (code checksum)
- ✅ Memory dump detection (guard pages)
- ✅ Tool window detection (IDA, Ghidra, x64dbg)

#### **2. Obfuscation (9/10 Difficulty)**
- ✅ **Encrypted VM** with self-modifying bytecode
- ✅ **Control flow flattening** (state machine dispatcher)
- ✅ **Polymorphic seed** for runtime variance
- ✅ **String encryption** (all sensitive strings)
- ✅ **White-box cryptography** simulation
- ✅ Multi-layer hash validation

#### **3. Validation Constraints**
```cpp
// Constraint 1: Length
input.length() == 16

// Constraint 2: Printable ASCII
0x20 <= char <= 0x7E

// Constraint 3: Sum modulo
sum(ASCII_values) % 13 == 5  // Sum = 1318 for solution

// Constraint 4: Polymorphic hash
poly_hash(input) & 0xFFFF == 0x8c3a

// Constraint 5: WhiteBox AES
WhiteBoxAES(input) & 0xFF == 0x7E

// Constraint 6: Flattened result
flatten_extreme(input) & 0xFFFFFFFF == 0x3d8f9a2c

// Constraint 7: String match
input == "MayF0rc3BeW1thU!"

// Constraint 8: Hash match
final_hash == 0x7a4f9c2e8b1d3f5aULL
```

---

## 🛠️ Solution Methodology

### **Approach 1: With Hint (Easy Path)**

If you have the hint "MayF0rc3BeW1thU!", you can:

1. **Bypass Anti-Debug:**
   ```bash
   # Use ScyllaHide plugin in x64dbg
   # OR patch the binary to NOP anti-debug checks
   ```

2. **Verify the hint:**
   ```python
   hint = "MayF0rc3BeW1thU!"
   print(f"Length: {len(hint)}")           # 16 ✓
   print(f"Sum: {sum(ord(c) for c in hint)}")  # 1318
   print(f"Sum % 13: {sum(ord(c) for c in hint) % 13}")  # 5 ✓
   ```

3. **Test the solution:**
   ```bash
   echo MayF0rc3BeW1thU! | .\challenge.exe
   ```

---

### **Approach 2: Without Hint (Hard Path)**

#### **Step 1: Static Analysis**

1. **Disassemble the binary:**
   ```bash
   # Use IDA Pro, Ghidra, or Binary Ninja
   ida64.exe challenge.exe
   ```

2. **Identify key functions:**
   - `main()` - Entry point
   - `verify()` - Main validation
   - `poly_hash()` - Polymorphic hash
   - `flatten_extreme()` - Control flow flattening
   - `ultimate_verification()` - Final check

3. **Extract constraints:**
   - Look for comparison operations
   - Note magic constants (0x8c3a, 0x7E, 0x3d8f9a2c, etc.)

#### **Step 2: Dynamic Analysis**

1. **Patch anti-debug:**
   ```
   Method 1: NOP out IsDebuggerPresent calls
   Method 2: Hook API functions
   Method 3: Use ScyllaHide plugin
   ```

2. **Attach debugger:**
   ```bash
   x64dbg challenge.exe
   ```

3. **Set breakpoints:**
   - `verify()` entry
   - Before each constraint check
   - At string comparison

4. **Monitor execution:**
   - Watch input string processing
   - Track hash calculations
   - Observe constraint validation

#### **Step 3: Symbolic Execution (Advanced)**

Use **angr** for automated analysis:

```python
import angr
import claripy

# Load binary
project = angr.Project('./challenge.exe', auto_load_libs=False)

# Create symbolic input
flag = claripy.BVS('flag', 16 * 8)

# Create initial state
state = project.factory.entry_state(args=['./challenge.exe'])
state.posix.files[0].content = flag

# Find success path
simgr = project.factory.simulation_manager(state)
simgr.explore(find=lambda s: b"CONGRATULATIONS" in s.posix.dumps(1))

if simgr.found:
    solution = simgr.found[0].solver.eval(flag, cast_to=bytes)
    print(f"Found: {solution.decode()}")
```

#### **Step 4: Constraint Solving**

Use **Z3 SMT solver**:

```python
from z3 import *

# Create 16 character variables
chars = [BitVec(f'c{i}', 8) for i in range(16)]
solver = Solver()

# Constraint 1: Printable ASCII
for c in chars:
    solver.add(And(c >= 0x20, c <= 0x7E))

# Constraint 2: Sum mod 13 = 5
solver.add((Sum(chars) % 13) == 5)

# Constraint 3: Specific values (if known from analysis)
# solver.add(chars[0] == ord('M'))
# ... add more constraints from analysis

if solver.check() == sat:
    model = solver.model()
    solution = ''.join(chr(model[c].as_long()) for c in chars)
    print(f"Solution: {solution}")
```

---

## 🔧 Tools Required

### **Essential Tools:**
1. **IDA Pro / Ghidra** - Static analysis
2. **x64dbg / WinDbg** - Dynamic debugging
3. **HxD / 010 Editor** - Hex editing
4. **PE Bear / CFF Explorer** - PE analysis

### **Advanced Tools:**
5. **angr** - Symbolic execution
6. **Z3** - Constraint solving
7. **Frida** - Dynamic instrumentation
8. **ScyllaHide** - Anti-anti-debugging

### **Python Libraries:**
```bash
pip install angr z3-solver pefile capstone unicorn
```

---

## 🎓 Learning Points

### **Key Takeaways:**

1. **Anti-Debug Techniques:**
   - Multiple layers are more effective than single checks
   - Timing-based detection is hard to bypass
   - Hardware breakpoint detection is very effective

2. **Obfuscation Methods:**
   - VM-based obfuscation significantly increases difficulty
   - Self-modifying code prevents static analysis
   - Control flow flattening hides logic

3. **Reverse Engineering Skills:**
   - Understanding x86/x64 assembly is crucial
   - Dynamic analysis complements static analysis
   - Automated tools (angr, Z3) can solve complex challenges

4. **CTF Strategy:**
   - Look for hints in error messages
   - Check for hardcoded strings or patterns
   - Test simple inputs first before complex analysis

---

## 📊 Difficulty Breakdown

```
Overall Difficulty: 8.5/10 (Expert Level)

├─ Anti-Debug:        9/10  ████████░░
├─ Obfuscation:       9/10  █████████░
├─ Cryptography:      7/10  ███████░░░
├─ Static Analysis:   8/10  ████████░░
└─ Dynamic Analysis:  9/10  █████████░
```

### **Solve Time Estimates:**

| Skill Level | With Hint | Without Hint |
|-------------|-----------|--------------|
| Beginner    | 8-12h     | Impossible   |
| Intermediate| 4-6h      | Weeks        |
| Advanced    | 1-2h      | Days         |
| Expert      | 30min-1h  | 1-2 days     |

---

## 🐛 Known Issues

### **Issue 1: Sum Modulo Constraint**
The original challenge had `sum % 13 == 0`, but the solution has `sum % 13 == 5`.
This was intentionally fixed to match the intended solution.

### **Issue 2: Polymorphic Seed**
The seed is now **deterministic** for reproducible results. Original version used random timestamp-based seed.

---

## 🏆 Challenge Statistics

```
Lines of Code:        ~700
Functions:            25+
Classes:              3
Anti-Debug Checks:    10+
Validation Layers:    8
Encrypted Strings:    8
Decoy Arrays:         4
Magic Constants:      15+
Validation Complexity: O(n³)
```

---

## 🎯 Alternative Solutions

### **Method 1: Binary Patching**
```
1. Open challenge.exe in x64dbg
2. Find the string comparison at ultimate_verification()
3. Patch JNE to JMP (always succeed)
4. Input any 16-character string
```

### **Method 2: Memory Dump**
```
1. Attach debugger
2. Set breakpoint at final string comparison
3. Read expected string from memory
4. Use that as input
```

### **Method 3: API Hooking**
```python
# Hook with Frida
import frida

script = """
Interceptor.attach(ptr("0x..."), {  // Address of verify()
    onEnter: function(args) {
        console.log("Input: " + Memory.readUtf8String(args[0]));
    }
});
"""
```

---

## 📚 Additional Resources

### **Learning Materials:**
- [Practical Reverse Engineering](https://www.amazon.com/dp/1118787315) - Book
- [Reversing.kr](http://reversing.kr) - Practice challenges
- [Crackmes.one](https://crackmes.one) - More challenges
- [OpenSecurityTraining](https://opensecuritytraining.info) - Free courses

### **Tools Documentation:**
- [IDA Pro](https://hex-rays.com/ida-pro/)
- [Ghidra](https://ghidra-sre.org/)
- [angr](https://angr.io/)
- [Z3 Theorem Prover](https://github.com/Z3Prover/z3)

---

## 🤝 Credits

**Challenge Author:** [Your Name]  
**Difficulty Rating:** Expert (8.5/10)  
**Category:** Reverse Engineering, Anti-Debug, Obfuscation  
**Release Date:** 2024  

---

## 📝 Notes

- This challenge is designed for **educational purposes** only
- All anti-debug techniques are for **learning** reverse engineering
- The solution is intentionally themed around **Star Wars** for fun
- Consider this challenge suitable for **advanced CTF competitions**

---

## 🎉 Congratulations!

If you solved this challenge, you've demonstrated:
- ✅ Advanced reverse engineering skills
- ✅ Anti-debugging bypass techniques
- ✅ Static and dynamic analysis proficiency
- ✅ Persistence and problem-solving abilities

**You are truly a reverse engineering master!**

May the Force be with you! 🌟

---

## 📞 Support

If you have questions or found bugs:
- Open an issue on GitHub
- Contact: [your-email@example.com]
- Discord: [Your Server]

---

**End of Solution Guide**
