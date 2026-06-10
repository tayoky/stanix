#!/usr/bin/env python3

# written by Samuel Duart(sasdallas)

import argparse
import bisect
import os
import os.path
import subprocess
import sys
import tempfile


class Linux:
    bpftraceBase = r"""
#define VCPU_REGS_RBP 5
#define VCPU_REGS_RIP 16
#define BASE_ADDR {base_memory_addr}
#define FRAME_COUNT {frame_count}

BEGIN
{{
    printf("starting\n");
}}
"""

    bpftraceVcpu = r"""
profile:hz:{profile_interval}
/pid == {pid}/
{{
    $vcpu_id = {vcpu};

    $file = (struct file *)curtask->files->fdt->fd[{vcpu}];
    if ($file == 0) {{
        return;
    }}

    $vcpu = (struct kvm_vcpu *)$file->private_data;

    if ((uint64)$vcpu->arch.regs[VCPU_REGS_RIP] < 0xFFFF000000000000) {{
	return;
    }}

    printf("thread_id: %d\t0x%lx\n", tid,
        (uint64)$vcpu->arch.regs[VCPU_REGS_RIP]);

$rbp = (uint64)$vcpu->arch.regs[VCPU_REGS_RBP];
    //$cr3 = (uint64)$vcpu->arch.mmu->root.hpa; // Fallback to $vcpu->arch.cr3 if needed
    $cr3 = (uint64)$vcpu->arch.cr3;
    $frame_id = 0;
    printf("Resolving CR3: %lx\n", $cr3);

    while ($frame_id < FRAME_COUNT && $rbp >= 0xffff800000000000)
    {{
    
        // Extract indices from the virtual RBP pointer
        $pml4_idx = ($rbp >> 39) & 0x1FF;
        $pdpt_idx = ($rbp >> 30) & 0x1FF;
        $pd_idx   = ($rbp >> 21) & 0x1FF;
        $pt_idx   = ($rbp >> 12) & 0x1FF;
        $page_offset = $rbp & 0xFFF;

	//printf("indexes: pml4=%d pdpt=%d pd=%d pt=%d pg_off=%d\n", $pml4_idx, $pdpt_idx, $pd_idx, $pt_idx, $page_offset);

        $phys_addr = (uint64)0;

        // 1. PML4 Entry
        $pml4_val = *(uint64 *)uptr(BASE_ADDR + ($cr3 & ~0xFFF) + ($pml4_idx * 8));
        if (!($pml4_val & 1)) {{ break; }} 

        // 2. PDPT Entry
        $pdpt_base = $pml4_val & 0x000FFFFFFFFFF000;
        $pdpt_val = *(uint64 *)uptr(BASE_ADDR + $pdpt_base + ($pdpt_idx * 8));
        if (!($pdpt_val & 1)) {{ break; }}

        // 3. PD Entry
        $pd_base = $pdpt_val & 0x000FFFFFFFFFF000;
        $pd_val = *(uint64 *)uptr(BASE_ADDR + $pd_base + ($pd_idx * 8));
        if (!($pd_val & 1)) {{ break; }}
        
        // Check for 2MB Huge Page (Bit 7 is Page Size - PS)
        if ($pd_val & 0x80) {{ 
            $phys_addr = ($pd_val & 0x000FFFFFC0000000) + ($rbp & 0x1FFFFF);
        }} 
        else {{
            // 4. PT Entry (Standard 4KB Page)
            $pt_base = $pd_val & 0x000FFFFFFFFFF000;
            $pt_val = *(uint64 *)uptr(BASE_ADDR + $pt_base + ($pt_idx * 8));
            if (!($pt_val & 1)) {{ break; }}

            $phys_addr = ($pt_val & 0x000FFFFFFFFFF000) + $page_offset;
        }}

        // Verify we resolved a valid address
        if ($phys_addr == 0) {{ break; }}

        // Access the host-mapped physical pointer
        $host_rbp = (uint64 *)uptr(BASE_ADDR + $phys_addr);

        $next_rbp = *$host_rbp;
        $frame = *( $host_rbp + 1 ); 

        if ($frame == 0) {{ break; }}
	if ($frame == 0xCDCDCDCDCDCDCDCD) {{ break; }} // stack terminator
	if ($frame <= 0xffff800000000000) {{ break; }} // non-kernel frame

        printf("thread_id: %d\t0x%lx\n", tid, $frame);

        // Enforce stack progress to strictly avoid infinite loops
        if ($next_rbp <= $rbp) {{ break; }}

        $rbp = $next_rbp;
        $frame_id++;
    }}

    printf("thread_id: %d\t1\n\n", tid);
}}
"""

    bpftraceEnd = r"""
interval:s:{duration}
{{
    exit();
}}
"""

    def __init__(self, pid):
        self.pid = str(pid)

    def bpftrace_base(self):
        return Linux.bpftraceBase

    def get_vcpus(self):
        vcpu_fds = {}

        proc_root = os.path.join("/proc", self.pid)

        for root, dirs, files in os.walk(proc_root):
            fd_dir = os.path.join(root, "fd")

            if not os.path.isdir(fd_dir):
                continue

            for fd in os.listdir(fd_dir):
                path = os.path.join(fd_dir, fd)

                try:
                    target = os.readlink(path)
                except OSError:
                    continue

                if "kvm-vcpu" in target:
                    vcpu_fds[int(fd)] = True

        return sorted(vcpu_fds.keys())

    def find_base_address(self):
        mapsizes = {}

        mapfile = os.path.join("/proc", self.pid, "maps")

        with open(mapfile, "r") as maps:
            for line in maps:
                line = line.strip()

                if "rw-p 00000000 00:00 0" not in line:
                    continue

                parts = line.split()
                addrs = parts[0].split("-")

                start = int(addrs[0], 16)
                end = int(addrs[1], 16)

                size = end - start
                mapsizes[size] = start

        sizes = sorted(mapsizes.keys())

        if not sizes:
            raise RuntimeError(
                "Could not locate guest memory mapping")

        return hex(mapsizes[sizes[-1]])


class Symbols:
    def __init__(self, symbol_file=None):
        self.symbols = {}
        self.symbol_keys = []

        if symbol_file is None:
            return

        with open(symbol_file, "r") as symfile:
            for line in symfile:
                parts = line.strip().split()

                if len(parts) < 3:
                    continue

                self.symbols[int(parts[0], 16)] = parts[2]

        self.symbol_keys = sorted(self.symbols.keys())

    def map_address(self, addr):
        if not self.symbol_keys:
            return "{:x}".format(addr)

        loc = bisect.bisect_right(
            self.symbol_keys,
            addr
        )

        if not loc:
            return "{:x}".format(addr)

        symkey = self.symbol_keys[loc - 1]
        symname = self.symbols[symkey]

        return "{}+{:x}".format(
            symname,
            addr - symkey
        )


def process_line(stacks, symbols, line):
    line = line.strip()

    if not line:
        return

    if not line.startswith("thread_id:"):
        return

    parts = line.split()

    thread_id = parts[1]
    value = parts[2]

    if thread_id not in stacks:
        stacks[thread_id] = []

    stack = stacks[thread_id]

    if not value.startswith("0x"):
        print("\n".join(stack))
        print("1")
        print("")
        stacks[thread_id] = []
        return

    addr = int(value, 16)
    stack.append(symbols.map_address(addr))


def generate_bpftrace_script(args):
    linux = Linux(args.pid)

    if args.base_address:
        base_addr = args.base_address
    else:
        base_addr = linux.find_base_address()

    params = vars(args).copy()
    params["base_memory_addr"] = base_addr

    script = linux.bpftrace_base().format(**params)

    for vcpu in linux.get_vcpus():
        local = params.copy()
        local["vcpu"] = vcpu

        script += Linux.bpftraceVcpu.format(**local)

    script += Linux.bpftraceEnd.format(**params)

    return script


def main():
    parser = argparse.ArgumentParser(
        description="Profile VM Guests via bpftrace"
    )

    parser.add_argument(
        "--pid",
        required=True,
        help="PID of QEMU process"
    )

    parser.add_argument(
        "--symbol_file",
        help="Optional symbol file"
    )

    parser.add_argument(
        "--frame_count",
        type=int,
        default=10,
        help="Frames to walk"
    )

    parser.add_argument(
        "--duration",
        default=10,
        help="Profile duration"
    )

    parser.add_argument(
        "--profile_interval",
        default=997,
        help="Sampling frequency"
    )

    parser.add_argument(
        "--base_address",
        default=None,
        help="Override guest base address"
    )

    parser.add_argument(
        "--script_only",
        action="store_true",
        default=False
    )

    args = parser.parse_args()

    script = generate_bpftrace_script(args)

    if args.script_only:
        print(script)
        return

    symbols = Symbols(args.symbol_file)

    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".bt",
        delete=False
    ) as scriptfile:

        scriptfile.write(script)
        scriptfile.flush()

        script_name = scriptfile.name

    cmd = [
        "bpftrace",
        script_name
    ]

    child = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    stacks = {}

    while True:
        line = child.stdout.readline()

        if not line:
            break

        process_line(
            stacks,
            symbols,
            line.decode(errors="ignore")
        )

    for line in child.stdout.readlines():
        process_line(
            stacks,
            symbols,
            line.decode(errors="ignore")
        )

    for tid, stack in list(stacks.items()):
        if stack:
            print("\n".join(stack))
            print("1")
            print("")

    child.wait()


if __name__ == "__main__":
    main()
