# Practice Bomb — Step-by-Step Walkthrough

## Setup

```bash
# Compile with debug symbols, no optimization
gcc -g -O0 -o practice_bomb practice_bomb.c

# On macOS use lldb; on the class server use gdb
# This walkthrough shows BOTH side by side
```

---

## Before You Start: The Golden Rule

**Always set a breakpoint on `explode_bomb` first.**

This prevents detonation if you guess wrong. The debugger will pause
instead of letting the bomb print BOOM and exit.

```
# lldb (macOS)                    # gdb (Linux / class server)
lldb ./practice_bomb               gdb ./practice_bomb
(lldb) b explode_bomb              (gdb) b explode_bomb
```

---

## Phase 1: String Comparison

### What this pattern looks like

Phase 1 typically loads a hardcoded string address into a register,
then calls `strcmp(your_input, hidden_string)`. If the return is
nonzero → boom.

### Step-by-step (lldb)

```
(lldb) b phase_1                   # breakpoint at phase_1 entry
(lldb) r                           # run the program
  Enter input: anything            # type any garbage
  # → hits breakpoint at phase_1

(lldb) disas -n phase_1            # disassemble the function
```

You'll see something like:

```asm
practice_bomb`phase_1:
  ...
  ; rdi = your input (first argument)
  ; loads an address into rsi (second argument to strcmp)
  lea    rsi, [rip + 0x...]        ; ← this is the hidden string address
  call   strcmp
  test   eax, eax
  jne    <explode_bomb>            ; if strcmp != 0, explode
  ...
```

**Key insight:** Before the `call strcmp`, the second argument (rsi on
x86-64, x1 on ARM64) holds the address of the expected string.

```
(lldb) register read rsi           # x86-64
(lldb) register read x1            # ARM64 (M1)
  x1 = 0x0000000100003f50

(lldb) memory read --size 1 --format c --count 64 0x0000000100003f50
  # or shorthand:
(lldb) x/s 0x0000000100003f50
  0x100003f50: "science is organized knowledge"
```

**Answer:** `science is organized knowledge`

### Alternative shortcut: `strings`

```bash
strings practice_bomb | grep -i "science"
# → science is organized knowledge
```

For Phase 1, `strings` often reveals the answer directly. On the real
bomb, this still works — check `strings bomb` first.

---

## Phase 2: Numeric Sequence with a Loop

### What this pattern looks like

Phase 2 calls `sscanf` with a format string like `"%d %d %d %d %d"`,
then runs a loop that verifies a mathematical relationship between
the numbers (e.g., each is double the previous).

### Step-by-step

```
(lldb) b phase_2
(lldb) c                           # continue (Phase 1 already solved)
  Enter input: 1 2 3 4 5           # guess — will be wrong, but
                                    # explode_bomb breakpoint saves us
  # → hits breakpoint at phase_2
```

**Step 1: Find the sscanf format string**

```
(lldb) disas -n phase_2
```

Look for the `call sscanf` instruction. Before it, a register (rsi on
x86-64, x1 on ARM64) is loaded with the format string address:

```
(lldb) # step until just before the sscanf call, then:
(lldb) x/s $rsi                    # x86-64
(lldb) x/s $x1                     # ARM64
  → "%d %d %d %d %d"
```

Now you know: **5 integers** are expected.

**Step 2: Find the first number check**

After sscanf, the assembly will compare `nums[0]` against a constant:

```asm
  cmp    dword [rbp - 0x24], 0x3   ; nums[0] must be 3
  jne    <explode_bomb>
```

So `nums[0] = 3`.

**Step 3: Understand the loop**

The loop body does something like:

```asm
loop:
  mov    eax, [rbp + rcx*4 - 0x28] ; load nums[i-1]
  shl    eax, 1                     ; multiply by 2 (shift left 1)
  cmp    [rbp + rcx*4 - 0x24], eax  ; compare nums[i] with nums[i-1]*2
  jne    <explode_bomb>
```

`shl eax, 1` = `eax * 2`. So each number must be double the previous.

Starting from 3: `3, 6, 12, 24, 48`

**Step 4: Verify by stepping through**

```
(lldb) b explode_bomb              # already set
(lldb) r                           # restart
  Phase 1: science is organized knowledge
  Phase 2: 3 6 12 24 48
  # → ✅ Phase 2 defused!
```

**Answer:** `3 6 12 24 48`

---

## Phase 3: Switch Statement

### What this pattern looks like

Phase 3 reads two integers. The first integer selects a `switch` case,
and each case sets an expected value for the second integer.

### Step-by-step

```
(lldb) b phase_3
(lldb) c
  Enter input: 0 0                 # guess — first value picks the case
  # → hits breakpoint at phase_3
```

**Step 1: Find sscanf format**

Same technique as Phase 2:

```
(lldb) # examine the format string before sscanf call
(lldb) x/s $rsi
  → "%d %d"                        # two integers
```

**Step 2: Identify the switch structure**

In the disassembly, you'll see one of two patterns:

**(a) Jump table** (common in real Bomb Lab):
```asm
  cmp    eax, 0x3                  ; if a > 3, default → explode
  ja     <explode_bomb>
  jmp    [rip + rax*8 + offset]    ; jump table indexed by first input
```

**(b) Cascading comparisons** (simpler, used in practice bomb at -O0):
```asm
  cmp    eax, 0x0
  je     case_0
  cmp    eax, 0x1
  je     case_1
  ...
```

**Step 3: Read the expected value from each case**

For case 0:
```asm
case_0:
  mov    dword [rbp - 0x8], 0x213  ; expected = 0x213 = 531
  jmp    end_switch
```

So if your first input is `0`, the second must be `531`.

You can pick ANY valid case. Multiple answers work:
- `0 531`
- `1 196`
- `2 818`
- `3 107`

**Step 4: Use GDB/LLDB to verify without computing hex manually**

```
(lldb) p/d 0x213
  (int) 531
```

**Answer (one of):** `0 531`

---

## Phase 4: Recursive Function

### What this pattern looks like

Phase 4 reads an integer, calls a recursive helper function (like
Fibonacci or factorial), and checks that the return value equals
a specific target.

### Step-by-step

```
(lldb) b phase_4
(lldb) b func4                     # also break on the helper
(lldb) c
  Enter input: 5                   # guess
```

**Step 1: Understand what func4 does**

```
(lldb) disas -n func4
```

You'll see two recursive calls:
```asm
func4:
  cmp    edi, 0x0                  ; if n <= 0, return 1
  jle    base_case
  cmp    edi, 0x1                  ; if n == 1, return 1
  je     base_case

  ; recursive: func4(n-1) + func4(n-2)
  lea    edi, [rdi - 1]
  call   func4
  mov    ebx, eax                  ; save func4(n-1)
  lea    edi, [r14 - 2]            ; not exact, but the pattern
  call   func4
  add    eax, ebx                  ; return func4(n-1) + func4(n-2)
  ...

base_case:
  mov    eax, 0x1                  ; return 1
```

Two base cases returning 1, two recursive calls added together →
**this is Fibonacci**: f(0)=1, f(1)=1, f(n)=f(n-1)+f(n-2).

**Step 2: Find the target value**

Back in phase_4's disassembly, after the call to func4:
```asm
  cmp    eax, 0x22                 ; 0x22 = 34
  jne    <explode_bomb>
```

So `func4(n)` must equal 34.

**Step 3: Compute Fibonacci values**

| n | func4(n) |
|---|----------|
| 0 | 1        |
| 1 | 1        |
| 2 | 2        |
| 3 | 3        |
| 4 | 5        |
| 5 | 8        |
| 6 | 13       |
| 7 | 21       |
| 8 | **34** ← |

**Answer:** `8`

**Step 4: Also check the bounds**

Phase 4 also checks `n >= 0 && n <= 15` before calling func4.
8 is in range, so we're good.

---

## Summary of Techniques

| Technique                  | When to use                                    |
|----------------------------|------------------------------------------------|
| `strings bomb`             | First thing — may reveal Phase 1 answer        |
| `objdump -d bomb > bomb.s` | Offline reading of full disassembly             |
| `x/s <addr>`              | Examine a memory address as a string            |
| `p/d <hex>`               | Convert hex constants to decimal                |
| Breakpoint on `explode_bomb` | ALWAYS — prevents point deduction            |
| `info registers`           | Check all register values at a breakpoint       |
| `x/Nwd <addr>`            | Examine N words of memory (stack, arrays)       |
| Step through loops          | Understand per-iteration logic (Phase 2, 6)    |
| Identify `sscanf` format   | Tells you how many values and what types        |
| Recognize recursion pattern | Two `call`s to same function = likely Fibonacci |

---

## Applying This to Your Real Bomb

Your real bomb phases follow the same patterns but with different
constants, strings, and sometimes more complex logic. The approach
is identical:

1. **Disassemble** the phase function
2. **Identify** key library calls (strcmp, sscanf, etc.)
3. **Read** string/format arguments from registers before calls
4. **Trace** the control flow (loops, switches, recursion)
5. **Extract** the expected values from comparison instructions
6. **Verify** with breakpoints before committing your answer

Phase 5 typically involves character-level encoding or array indexing,
and Phase 6 involves linked list manipulation. Same principles apply —
just more instructions to trace.
