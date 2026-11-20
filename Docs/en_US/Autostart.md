# User autostart

[Documentation index](README.md) · [한국어](../ko_KR/Autostart.md) · [English](../en_US/Autostart.md)

After authentication, the session process applies the user's UID/GID, changes to the home directory and reads `~/.config/autostart`. It starts each entry using fork/execve without waiting for completion, then starts the login shell as another child. A failed startup application does not prevent the shell from starting.

## Configuration

If missing, Login creates `.config` and this default file with user authority. Existing configuration is preserved.

```text
/sys/lwm/lwm.lix
```

Each line contains one absolute executable path. Blank lines and lines beginning with # are skipped. Surrounding whitespace and CRLF are accepted. Arguments, shell syntax, variables and PATH lookup are unsupported. A line can contain at most 4095 bytes, with at most 32 startup applications.

Disable startup entries by emptying the file:

```sh
echo -n "" > "$HOME/.config/autostart"
```

Restore lwm:

```sh
echo /sys/lwm/lwm.lix > "$HOME/.config/autostart"
```

## Descriptors and graphics

Ordinary startup applications receive `/dev/null` as stdin; stdout/stderr remain connected to the terminal. GUI programs request TTY/display ownership through the existing interfaces.

When lwm starts, Terminal connects as an IPC background client. Lwm reads hardware input and sends keys to the focused client; Terminal forwards its received keys to the shell through a separate input queue. The CLI remains usable while lwm is running. See [lwm](Lwm.md) for controls and resizing limits.

## Session lifetime

The session returns to Login after collecting the shell and all directly started applications. If the shell exits while an application remains alive, the session waits for that application. Forced termination on logout needs signal delivery that is not implemented. Forked descendants are not tracked as a complete session process tree.

See [Validation](Validation.md) for the distinction between implementation, build tests and observed UI behavior.
