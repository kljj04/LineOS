# Accounts and sudo

[Documentation index](README.md) · [한국어](../ko_KR/Users.md) · [English](../en_US/Users.md)

UserDB is a LOS shared library built against musl. Its authentication model is independent of whether standard libc account lookup APIs are available.

## Account storage

| Component | Role |
| --- | --- |
| `/etc/userdb` | Authoritative LOS accounts, UID/GID, salts and Argon2id hashes |
| `/etc/passwd` | Generated public account-name records with `x` password fields |
| `/etc/sudoers` | Allowed-user list for bundled sudo, not upstream sudoers syntax |
| Login | UserDB lookup/password verification and user-session startup |
| sudo | Calling-user lookup, authentication and root command execution |
| adduser / deluser | UserDB and home-directory management |

Root-authorized updates use temporary files, exclusive locking and rename. Passwords use the custom Argon2id encoding with m=65536,t=3,p=1. Public name information is separated from the secret hash source. Existing records are not automatically converted to shadow files or another hash format.

## Using sudo

Bundled sudo is a LOS implementation. Basic command execution works, including:

```text
sudo cat /etc/sudoers
sudo locale set ko_KR.UTF-8
sudo -k
sudo cd /etc
```

The executable installs root-owned mode 4755 at `/bin/lsh/sudo.lix`. It identifies the caller through the real UID, checks UserDB and the allowed-user file, authenticates if needed and runs commands with root UID/GID and a restricted environment. The authentication cache uses LOS kernel APIs and login-session identity; `-k` clears it.

Commands are resolved under `/bin/lsh` or by an explicitly supplied path, with the project `.lix` suffix handling. The LOS-specific sudo cd command changes the parent shell directory through LOSParentChdir.

## Unsupported upstream features

The full upstream sudo option set, sudoers policy language, plugins, PAM, alternate target users, policy auditing and I/O logging are not implemented. Porting upstream sudo requires reviewing authentication, terminal handling, signals, process groups, job control and the kernel APIs used by the selected build. Musl account lookup alone does not provide PAM authentication.

## Secure dynamic execution

The kernel supports root-owned setuid execution with real/effective UID/GID and AT_SECURE. Secure executables and path components must be root-owned and not group/other-writable. Symlinks and dot/dot-dot traversal are rejected. The secure interpreter is restricted to trusted `/lib/libc.so`; secure file mmap also checks trusted paths. Musl uses AT_SECURE to choose secure loader-environment handling. Setgid execution is unsupported.

These checks are part of the execution model. Working basic sudo commands do not imply the full upstream sudo/security model or a complete Linux security boundary. Capabilities, supplementary groups and general signal delivery are not provided.

## Compatibility boundary

Standard shadow/group integration, supplementary groups, PAM, NSS plugins and standard password-hash compatibility are incomplete. Login and sudo authenticate through UserDB directly rather than getpwnam/PAM. Future account changes must preserve original records and consistency with the generated public name database.

See [Validation](Validation.md) for disposable secure-exec/sudo fixtures.
