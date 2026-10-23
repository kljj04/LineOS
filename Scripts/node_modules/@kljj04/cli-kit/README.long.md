# @kljj04/cli-kit — Full Documentation

All-in-one terminal styling library for Node.js.  
RGB truecolor, boxes, tables, spinners, and progress bars — all in one package, zero dependencies.

---

## Install

```bash
npm install @kljj04/cli-kit
```

---

## Core Concept

All styling follows a consistent 3-step chain:

```js
kit.color(R, G, B).style(...args).context(value)
```

| Step | Method | Description |
|---|---|---|
| 1 | `color(r, g, b)` | Set RGB truecolor (0–255 each) |
| 2 | `style(...args)` | Set text styles + output type |
| 3 | `context(value)` | Render the output |

You can also write it across multiple lines:

```js
kit
  .color(255, 100, 50)
  .style('bold', 'box')
  .context('Hello!')
```

---

## color(r, g, b)

Sets the foreground color using RGB truecolor.  
Requires a terminal with truecolor support (Windows Terminal, iTerm2, most modern terminals).

```js
kit.color(255, 0, 0)    // red
kit.color(0, 255, 0)    // green
kit.color(0, 0, 255)    // blue
kit.color(255, 255, 255) // white
```

---

## style(...args)

Accepts any combination of text styles and one output type.

### Text Styles

| Value | Description |
|---|---|
| `bold` | Bold text |
| `dim` | Dimmed text |
| `italic` | Italic text |
| `underline` | Underlined text |
| `inverse` | Inverted fg/bg colors |
| `strikethrough` | Strikethrough text |

### Output Types

| Value | Description |
|---|---|
| `print` | Plain text (default) |
| `box` | Unicode box border |
| `spinner` | Animated spinner |
| `progress` | Progress bar |
| `table` | Formatted table |

Multiple text styles can be combined. Only one output type should be passed.

```js
kit.color(255, 100, 50).style('bold', 'italic', 'box').context('Hello!')
```

---

## Output Types

### print (default)

Prints styled text to stdout. Output type can be omitted.

```js
kit.color(255, 100, 50).style('bold').context('Hello!')
kit.color(255, 100, 50).style('bold', 'print').context('Hello!')
```

---

### box

Wraps text in a Unicode box border. Supports multiline strings (`\n`).

```js
kit.color(100, 200, 255).style('box').context('Hello!')
```

```
┌────────┐
│ Hello! │
└────────┘
```

Multiline:

```js
kit.color(150, 255, 100).style('box').context('Line one\nLine two\nLine three')
```

```
┌────────────┐
│ Line one   │
│ Line two   │
│ Line three │
└────────────┘
```

---

### spinner

Returns a spinner object with a `.stop(text)` method.

```js
const s = kit.color(0, 255, 200).style('spinner').context('Loading...')

setTimeout(() => {
    s.stop('Done!')  // clears spinner, prints final message
}, 2000)
```

| Method | Description |
|---|---|
| `s.stop(text?)` | Stop spinner. Optionally print a final message. |

---

### progress

Pass a number from 0 to 100.

```js
kit.color(255, 200, 0).style('progress').context(72)
// [██████████████████████░░░░░░░░] 72%
```

**Live mode** — pass `{ value, override: true }` to overwrite the same line.  
Useful inside loops or intervals. -Obsolete

```js
kit.color(0, 255, 150).style('progress', 'override').context(i) //Now available
```

Example with interval:

```js
let i = 0
const interval = setInterval(() => {
    i++
    kit.color(0, 255, 150).style('progress', 'override').context(i)
    if (i >= 100) clearInterval(interval)
}, 30)
```

---

### table

Pass an array of objects. Keys are used as column headers automatically.

```js
kit.color(200, 150, 255).style('table').context([
    { name: 'Alice', age: 20, role: 'dev' },
    { name: 'Bob',   age: 25, role: 'designer' },
    { name: 'Carol', age: 30, role: 'PM' },
])
```

```
┌───────┬─────┬──────────┐
│ name  │ age │ role     │
┼───────┼─────┼──────────┼
│ Alice │ 20  │ dev      │
│ Bob   │ 25  │ designer │
│ Carol │ 30  │ PM       │
└───────┴─────┴──────────┘
```

---

## Full Example

```js
const kit = require('@kljj04/cli-kit')

// box
kit.color(0, 255, 100).style('bold', 'box').context('Welcome!')

// spinner
const s = kit.color(100, 200, 255).style('spinner').context('Loading...')
setTimeout(() => s.stop('Done!'), 1500)

// table
kit.color(200, 150, 255).style('table').context([
    { task: 'Build',  status: 'OK' },
    { task: 'Deploy', status: 'OK' },
])

// progress (live)
let i = 0
const iv = setInterval(() => {
    i++
    kit.color(255, 200, 0).style('progress', 'override').context(i)
    if (i >= 100) {
        clearInterval(iv)
        kit.color(0, 255, 100).style('bold').context('Complete!')
    }
}, 20)
```

---

## Planned Features

- `gradient([r,g,b], [r,g,b])` — per-character color interpolation for progress bars
- `style('round', 'box')` — rounded box corners

---

## License

MIT
