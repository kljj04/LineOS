# DeviceFS and process ABI

## Errors

`Common/Syscall/Errno.h` defines Linux x86-64 error numbers. Kernel calls return negative errors and unknown syscall numbers return -ENOSYS. Libc translates -4095..-1 to errno and -1 (MAP_FAILED for mmap), preserving errno on success. The raw LineOSSyscall entry functions preserve raw results.

`__errno_location()` accesses an internal LIBC_THREAD_STATE. Storage is currently private to each single-threaded process, copied by fork and reset by a new image. Thread support must replace the accessor with TLS selection before shared-address-space threads are enabled; this is not a pthread-safe TLS implementation.

FD errors distinguish EBADF, EFAULT, EACCES, ENOENT, EISDIR, ENOTDIR, EMFILE and backend errors. Partial transfers return the transferred count. Existing sticky regular-file write errors remain; close consumes the FD before reporting an error. VFS lock contention is serialized while preserving interrupt state and servicing TLB shootdowns.

## Device tree and descriptors

Boot registers a fixed DeviceFS backend at /dev, without changing the root design or adding a general mount API. VFS resolves absolute/relative paths, symlink targets and /dev/.., enumerates the subtree and includes dev once in the root directory. Nodes are a read-only registry. These special nodes exist in the running kernel VFS; mounting the EXT2 image on the host does not expose /dev. No placeholder or ordinary device files are created on EXT2. Keyboard input continues through lsh1 and its master queue. The kernel mouse task consumes pointer events and updates the hardware cursor; raw keyboard/mouse device-file protocols are not registered.

| Node | Behavior |
| --- | --- |
| /dev/null | EOF on read; discard writes. |
| /dev/zero | Zero-filled reads; discard writes. |
| /dev/full | Zero-filled reads; nonempty writes fail with ENOSPC. |
| /dev/lsh1 | Existing program terminal queues. |
| /dev/lsh1-master | Existing master queues; boot Init only; one open object. |
| /dev/fb0 | Pixel read/write, screen-info and presentation ioctl, shared mmap. |

Init now uses /dev/lsh1-master instead of the non-UNIX character-device child path /dev/lsh1/master. The old path is not retained as an alias. Device operations provide open/read/write/close/ioctl/map. Inherited descriptors share open-object references and offsets; final close releases the device. Read-only directory FDs support getdents64 (217), using Linux x86-64 dirent64 records. stat/lstat/fstat (4/6/5) report node type, permissions, identity, size and timestamps using a 144-byte x86-64 layout. Device st_rdev identifies the fixed registry node; device timestamps and size are zero. Regular-file fstat retains the existing path and namespace-generation checks. opendir/readdir, mknod and dynamic device registration remain unsupported.

## ioctl and display

ioctl (16) returns EBADF for invalid descriptors and ENOTTY for unsupported files/devices/commands. User pointers use checked VMM copy operations.

| Command | ABI | Behavior |
| --- | --- | --- |
| FBIOGET_VSCREENINFO, 0x4600 | Linux, 160-byte layout | Read the fixed 32-bit BGRA mode. |
| FBIOGET_FSCREENINFO, 0x4602 | Linux x86-64, 80-byte layout | Read stride, byte size and truecolor metadata. Physical/MMIO addresses are zero. |
| LOS_FBIO_PRESENT | LineOS, _IOW('L', 1, ...) | Reuse full-screen presentation. |
| LOS_FBIO_PRESENT_RECT | LineOS, _IOW('L', 2, ...) | Reuse rectangle presentation. |
| LOS_FBIO_FLUSH | LineOS, _IO('L', 3) | Mark the full screen dirty and issue VirtIO transfer/flush. |

Mode changes, panning, colormaps, DRM/KMS and vsync are unsupported. Private presentation commands require write access. fb0 writes transfer/flush accepted pixels; mmap writes require an explicit LOS_FBIO_FLUSH. Clients must coordinate concurrent shared framebuffer writes. Terminal opens fb0 with O_RDWR, queries both screen-info ioctls, validates the fixed 32-bit layout, and maps the pixel span with MAP_SHARED. It keeps its private rendering buffer so cursor restoration does not alter the displayed pixels, copies each completed frame into the mapping, and calls LOS_FBIO_FLUSH. Existing read(3), TTF fonts, UTF-8 decoding and scrolling remain. Existing Display library wrappers and display/asset syscalls remain available for other clients.

## mmap and munmap

mmap (9) uses the six-argument Linux x86-64 register ABI; munmap is 11. Anonymous MAP_PRIVATE pages are zeroed with requested R/W/X permissions. fb0 MAP_SHARED maps framebuffer RAM, requiring read access and write access for writable mappings. Mappings survive FD close. The mapping validates page alignment and the rounded page span against the framebuffer allocation, whose padding is zeroed by the page allocator. Executable and private framebuffer mappings are rejected. Framebuffer recreation is rejected once allocated, preserving mapped page lifetime; dynamic display resizing is unsupported. Partial munmap and already-unmapped holes within the mmap arena are supported. Process image cleanup releases owned pages.

The arena is 0x600000000000..0x600100000000, with 64 MiB per call and 128 MiB of reserved region spans per process. Addresses are hints; callers must use the returned address. PROT_NONE, fixed mappings, anonymous MAP_SHARED and ordinary file mappings are unsupported. Unsupported mapping combinations return EOPNOTSUPP; ordinary files return ENODEV. munmap does not remove ELF or asset pages outside this arena. mprotect, msync and file writeback remain unimplemented. VMM serializes page-table changes and flushes the local TLB before freeing pages; an address space running on another CPU cannot be modified. Shared-address-space threads remain unsupported.

## fork and execve

fork (57) returns the child PID in the parent and zero in the child. It eagerly copies private user pages, registers and stack; assets/framebuffer shared mappings and open-file offsets remain shared. Cwd and credentials are copied. Boot Init asset-publisher authority is not granted to fork children. COW is not implemented.

execve (59) prepares an image before committing, preserves PID/FDs/cwd/credentials, replaces the address space on success and preserves the old image on failure. Limits are 64 argv entries and 64 envp entries, 4096 bytes per string and 16384 combined string bytes. The existing LineOS entry convention passes RDI=argc, RSI=argv and RDX=envp; Linux initial-stack/auxv compatibility is not implemented. Only static LineOS ELF images are supported, without a dynamic linker, scripts or ELF TLS.

Creation/replacement uses a wait queue to serialize fork and execve. Slot exhaustion returns EAGAIN. Fork retains at most 16 execution records and reaps terminated records during the next creation call. All bundled program launches use fork followed by execve in the child, with argv[0]. Login supplies USER and PATH=/bin/lsh to the shell, which inherits its environment when executing children; other launch sites supply an empty environment. The spawn/spawnv extension and syscall numbers 1030/1031 are removed. Exec failure is reported by the child, which exits with 127; the parent receives only the fork result. The shell waits for each external child with waitpid before printing the next prompt and supports relative executable paths and cd [PATH] through chdir (80); cd without a path selects /. It has no built-in ls. The two-line prompt displays the current path from getcwd (79) and the USER environment value: `┏━[PATH]━(USER)` followed by `┗━>`. waitpid/wait use wait4 (61) with a positive child PID or -1, blocking mode or WNOHANG, and optional exit status. Unsupported process-group selection and rusage return EOPNOTSUPP; unsupported flags return EINVAL. Uncollected child status is retained while the parent lives; wait consumes it once, with ECHILD thereafter. Dead-parent records are reclaimed on later creation calls without reparenting. Signal delivery and threads remain unsupported. Existing regular-file path and namespace-generation checks remain, so open-inode lifetime across rename/unlink is not Linux-compatible.

## References and follow-up

See the Linux [syscall table](https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl), [errno UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/errno.h) and [framebuffer UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/linux/fb.h). Follow-up work includes TLS errno, wait/reaping, inode lifetime, file mmap/mprotect/msync, display ownership and replacing the retained display/asset extensions after the device path is validated.

Commands without / are searched in colon-separated PATH entries, trying the exact filename and then a .lix suffix unless already present. Empty PATH entries select the current directory. An unset PATH defaults to /bin/lsh. Paths containing / execute exactly as entered. Shell lookup runs in the fork child and keeps inherited environment and waitpid ordering.
