# 🐚 Shell Lab Guide — Writing Your Own Unix Shell (tsh)

> **TL;DR:** You're given a skeleton C file (`tsh.c`) with 7 empty functions. Your job is to fill them in so that your tiny shell (`tsh`) behaves identically to the reference shell (`tshref`).

---

## 🎯 What Is This Lab About?

You're building a **mini command-line shell** — like `bash` or `zsh`, but much simpler. Your shell will:

1. **Print a prompt** (`tsh> `) and wait for user input
2. **Run programs** by forking child processes
3. **Manage jobs** — track background/foreground/stopped jobs
4. **Handle signals** — ctrl-C (kill), ctrl-Z (stop), and child process notifications

---

## 📋 What You Actually Need to Do

You need to implement **7 functions** in `tsh.c`. Everything else (parsing, job list management, etc.) is already written for you.

### The 7 Functions (in suggested implementation order)

| # | Function | What It Does | ~Lines |
|---|----------|-------------|--------|
| 1 | `eval` | The heart of the shell — parse the command, decide what to do | ~70 |
| 2 | `builtin_cmd` | Check if the command is `quit`, `jobs`, `bg`, or `fg` | ~25 |
| 3 | `do_bgfg` | Handle the `bg` and `fg` commands | ~50 |
| 4 | `waitfg` | Wait (busy-loop) until a foreground job finishes | ~20 |
| 5 | `sigchld_handler` | Handle SIGCHLD — reap dead/stopped children | ~80 |
| 6 | `sigint_handler` | Handle ctrl-C — forward SIGINT to foreground job | ~15 |
| 7 | `sigtstp_handler` | Handle ctrl-Z — forward SIGTSTP to foreground job | ~15 |

---

## 🧠 Key Concepts Explained Simply

### Foreground vs Background Jobs
- **Foreground** (no `&` at end): Shell **waits** until the program finishes before accepting new input
- **Background** (ends with `&`): Shell **doesn't wait** — you can type more commands right away

```
tsh> /bin/ls -l          ← foreground: shell waits for ls to finish
tsh> /bin/ls -l &        ← background: shell immediately shows prompt again
```

### Job IDs (JID) vs Process IDs (PID)
- **PID**: The OS-assigned process ID (a number like 9723)
- **JID**: Your shell's own ID for the job (starts at 1, increments)
- On the command line: `%5` means JID 5, plain `5` means PID 5

### Signals You Need to Handle
| Signal | Trigger | Default Action | What Your Shell Should Do |
|--------|---------|----------------|--------------------------|
| `SIGINT` | ctrl-C | Terminate process | Forward to the foreground job (kill it) |
| `SIGTSTP` | ctrl-Z | Stop (suspend) process | Forward to the foreground job (pause it) |
| `SIGCHLD` | Child terminates or stops | — | Reap the child, update job list, print message |

### Job State Transitions
```
FG (running) ──ctrl-Z──→ ST (stopped)
ST (stopped) ──fg cmd──→ FG (running)
ST (stopped) ──bg cmd──→ BG (running)
BG (running) ──fg cmd──→ FG (running)
```

---

## 🔧 How to Implement Each Function

### 1. `eval(char *cmdline)` — The Main Loop Body

This is the most important function. Here's the logic:

```
1. Parse the command line using parseline() → get argv[] and bg flag
2. If empty line, return immediately
3. Check if it's a built-in command (call builtin_cmd)
   - If yes, builtin_cmd handles it → return
4. If NOT a built-in command, we need to run an external program:
   a. Block SIGCHLD signals (sigprocmask)  ← CRITICAL: prevents race condition
   b. Fork a child process
   c. In the CHILD:
      - Set own process group: setpgid(0, 0)  ← CRITICAL: for signal isolation
      - Unblock SIGCHLD (inherited from parent's block)
      - Exec the program (execve)
      - If execve fails → print error and exit(1)
   d. In the PARENT:
      - Add the child to job list (addjob)
      - Unblock SIGCHLD (sigprocmask)
      - If foreground job (bg == 0): waitfg(pid)
      - If background job (bg == 1): print job info "[jid] (pid) cmdline"
```

**⚠️ Why block SIGCHLD before forking?**
If you don't, the child might finish SO fast that `sigchld_handler` runs and calls `deletejob` BEFORE the parent even calls `addjob`. The job would never appear in the job list. Blocking prevents this race condition.

**⚠️ Why `setpgid(0, 0)` in the child?**
Without it, the child is in the same process group as your shell. So ctrl-C would kill BOTH your shell AND the child. `setpgid(0,0)` puts the child in its own group, so your shell can selectively forward signals.

### 2. `builtin_cmd(char **argv)` — Built-in Command Checker

Check `argv[0]` and act immediately:
- `"quit"` → exit the shell (`exit(0)`)
- `"jobs"` → call `listjobs(jobs)` and return 1
- `"bg"` or `"fg"` → call `do_bgfg(argv)` and return 1
- Anything else → return 0 (not a built-in)

Return **1** if it was a built-in (already handled), **0** if not.

### 3. `do_bgfg(char **argv)` — The `bg` and `fg` Commands

```
1. Check if argv[1] is provided. If not → print "bg/fg command requires PID or %%jobid argument" → return
2. Determine which job the user means:
   - If argv[1] starts with '%': it's a JID → use getjobjid()
   - Otherwise: it's a PID → use getjobpid()
   - If the job doesn't exist → print error → return
3. Send SIGCONT to the job (kill(-job->pid, SIGCONT))
   Note: use negative PID to send to the whole process group
4. If command was "bg":
   - Set job state to BG
   - Print: [jid] (pid) cmdline
5. If command was "fg":
   - Set job state to FG
   - Call waitfg(job->pid) to wait for it to finish
```

### 4. `waitfg(pid_t pid)` — Wait for Foreground Job

Simple busy loop:
```c
while (fgpid(jobs) == pid) {
    sleep(1);
}
```
That's it. Just keep checking if the foreground job is still running. The actual cleanup happens in `sigchld_handler`.

### 5. `sigchld_handler(int sig)` — Reap Children

This is the trickiest one. Called when a child terminates OR stops.

```
Use waitpid(-1, &status, WUNTRACED | WNOHANG) in a loop:
  - WNOHANG: don't block if no child is ready
  - WUNTRACED: also report stopped children

For each child reaped:
  - If WIFEXITED(status) or WIFSIGNALED(status):
    → The job terminated
    → If WIFSIGNALED: print "Job (pid) terminated by signal <sig>"
    → Delete the job from the job list (deletejob)
  - If WIFSTOPPED(status):
    → The job was stopped (e.g., by ctrl-Z)
    → Change job state to ST
    → Print "Job (pid) stopped by signal <sig>"
```

**Key point:** Do ALL reaping here. Don't call `waitpid` in `waitfg` — just use the busy loop there.

### 6. `sigint_handler(int sig)` — Handle ctrl-C

```
1. Get the foreground job's PID (fgpid(jobs))
2. If there IS a foreground job (pid > 0):
   - Send SIGINT to the whole process group: kill(-pid, SIGINT)
3. If no foreground job → do nothing (ignore the signal)
```

### 7. `sigtstp_handler(int sig)` — Handle ctrl-Z

Same structure as sigint_handler, but send SIGTSTP:
```
1. Get the foreground job's PID (fgpid(jobs))
2. If there IS a foreground job (pid > 0):
   - Send SIGTSTP to the whole process group: kill(-pid, SIGTSTP)
3. If no foreground job → do nothing
```

---

## 🧪 How to Test

### Quick test (one trace at a time)
```bash
make                    # compile your shell
make test01            # run trace 01 on YOUR shell
make rtest01           # run trace 01 on REFERENCE shell
```

### Compare outputs
Your output should match `tshref` **exactly** (except PIDs will differ).

### Run all traces
```bash
make test01   through   make test16
```

### Interactive testing
```bash
./tsh
tsh> /bin/ls -l
tsh> /bin/ls -l &
tsh> jobs
tsh> fg %1
tsh> quit
```

### Use the reference shell to see expected behavior
```bash
./tshref
```

---

## ⚠️ Common Pitfalls

| Pitfall | Fix |
|---------|-----|
| Race condition: child reaped before `addjob` | Block SIGCHLD before fork, unblock after `addjob` |
| ctrl-C kills your shell too | Call `setpgid(0,0)` in child after fork |
| Sending signal to wrong process | Use `kill(-pid, sig)` (negative PID = whole group) |
| Zombie processes piling up | Reap in `sigchld_handler` with `waitpid(-1, ..., WNOHANG\|WUNTRACED)` |
| Calling `waitpid` in both `waitfg` and handler | Only call `waitpid` in the handler; `waitfg` just busy-loops |
| Forgetting to unblock SIGCHLD in child | After `setpgid`, unblock SIGCHLD before `execve` |
| Not checking return values of system calls | Check EVERY system call return value (style points!) |

---

## 📦 Helper Functions Already Provided (Don't Modify These)

| Function | Purpose |
|----------|---------|
| `parseline()` | Splits command line into `argv[]`, returns 1 if background (`&`) |
| `addjob()` | Add a job to the job list |
| `deletejob()` | Remove a job from the job list |
| `listjobs()` | Print all jobs |
| `fgpid()` | Get PID of current foreground job (0 if none) |
| `getjobpid()` | Find job by PID |
| `getjobjid()` | Find job by JID |
| `pid2jid()` | Convert PID to JID |
| `initjobs()` | Initialize the job list |
| `Signal()` | Wrapper for `sigaction` (reliable signal installation) |

---

## 📊 Grading

| Category | Points |
|----------|--------|
| Correctness (16 traces × 5 pts) | 80 |
| Style: Good comments | 5 |
| Style: Check return value of EVERY system call | 5 |
| **Total** | **90** |

---

## 📤 Submission

Submit only your `tsh.c` file to LMS.

---

## 🚀 Recommended Implementation Order

1. **`builtin_cmd`** — easiest, just string comparisons
2. **`eval`** — the core, but start simple (just fork+exec, no signals yet)
3. **`sigchld_handler`** — so terminated jobs get reaped
4. **`waitfg`** — simple busy loop
5. **`sigint_handler`** and **`sigtstp_handler`** — forward signals
6. **`do_bgfg`** — bg/fg commands
7. Add SIGCHLD blocking/unblocking to `eval` — fix the race condition
8. Test with trace01 → trace16, fixing issues as you go

Start with `make test01` and `make rtest01`, compare outputs, then move to trace02, and so on. The traces build up in complexity!