# Locale and timezone

[Documentation index](README.md) · [한국어](../ko_KR/Locale.md) · [English](../en_US/Locale.md)

Musl implements standard setlocale, strftime, strerror and localeconv. `liblocale.so` adds LOS defaults and built-in LOSGettext messages without replacing libc symbols. `/etc/locale` stores one locale name; `/etc/timezone` stores one project timezone name. Image updates preserve existing settings.

## Commands

```text
locale
locale -a
sudo locale set ko_KR.UTF-8
sudo locale set en_US.UTF-8
```

LOS settings accept C, C.UTF-8, en_US.UTF-8 and ko_KR.UTF-8; POSIX normalizes to C. Locale set checks root authority and replaces the setting using a temporary file and rename. It persists a normalized single name, not musl's potentially semicolon-separated LC_ALL result.

## Applying defaults

Declare Locale under Program.cfg Libraries, then initialize with:

```c
#include <LOSLocale.h>
#include <locale.h>

if (LOSLocaleRefresh() == NULL)
{
    setlocale(LC_ALL, "C");
}
```

LOSLocaleRefresh reads the file and selects the first nonempty value for each category:

1. LC_ALL
2. That category's LC_* variable
3. LANG
4. A valid /etc/locale value
5. C

Missing/invalid files fall back to C. Unsupported explicit environment names fail without changing the category selection. Empty environment variables fall through. File defaults are not written into LANG, so a previous file value cannot mask later updates. Explicit user environment values still take precedence.

The Locale constructor applies defaults at startup; bundled programs also use this API. Lsh reapplies them at every prompt, so `sudo locale set ...` affects built-in messages at the next prompt. Other running programs must refresh or restart. Standard `setlocale(LC_ALL, "")` reads musl's environment defaults, not LOS files.

LOSLocaleNormalize returns a canonical supported name or NULL. Do not use musl's acceptance of arbitrary locale names as validation of LOS settings.

## Translation scope

When LC_MESSAGES is ko_KR.UTF-8, LOSGettext binary-searches the English-keyed table in Libraries/Locale/Source/Gettext.c. Missing translations return the original string. Format arguments must match the source string. Lsh's directly selected messages also follow LC_MESSAGES; category overrides and sudo's forwarded locale environment affect this choice.

Built-in messages are not a complete musl Korean locale dataset. Date/weekday/month names, strerror/perror, numeric/currency formatting and collation need not become Korean. External gettext domains and plural APIs are not supplied. Date's standard strftime output can therefore remain English even when LOS messages are Korean.

## Timezones

If TZ is unset, the constructor converts /etc/timezone through the fixed timezone table into a POSIX TZ string and calls tzset. An explicitly set TZ is preserved. The table supplies fixed UTC offsets and abbreviations, not a regional DST database. Locale changes do not alter kernel UTC time or EXT2 timestamps; see [Clock](Clock.md).

## Validation

`python3 Scripts/Validation/Locale.py` runs project musl and production source with configuration I/O redirected to temporary files. It checks refresh within one process, environment/empty-value precedence, translations and normalized persistence. This is a host execution test, not a VM screen observation; see [Validation](Validation.md).
