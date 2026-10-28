// Scripts/Frontend/UI.js
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

const FS = require("node:fs");

class Renderer
{
    constructor(Output = process.stdout, ErrorOutput = process.stderr, Environment = process.env)
    {
        this.Output = Output;
        this.ErrorOutput = ErrorOutput;
        this.Environment = Environment;
        this.ColorEnabled = Boolean(Output.isTTY && Environment.NO_COLOR === undefined && (Environment.TERM || "dumb") !== "dumb");
        this.Animated = Boolean(Output.isTTY && (Environment.TERM || "dumb") !== "dumb");
        this.Frames = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"];
        this.Timer = null;

        this.Active = null;
    }

    Width()
    {
        let Cols = /^[1-9][0-9]*$/.test(this.Environment.COLUMNS || "") ? Number(this.Environment.COLUMNS) : (this.Output.columns || 80);
        return Cols > 4 ? Cols - 4 : Cols;
    }

    Color(R, G, B)
    {
        return this.ColorEnabled ? `\x1b[38;2;${R};${G};${B}m` : "";
    }

    Reset()
    {
        return this.ColorEnabled ? "\x1b[0m" : "";
    }

    Bold()
    {
        return this.ColorEnabled ? "\x1b[1m" : "";
    }

    Gradient(Text, R1, G1, B1, R2, G2, B2)
    {
        const Characters = Array.from(Text);
        const Colored = Characters.map((Character, Index) =>
        {
            const Denominator = Math.max(1, Characters.length - 1);
            const R = R1 + Math.trunc((R2 - R1) * Index / Denominator);
            const G = G1 + Math.trunc((G2 - G1) * Index / Denominator);
            const B = B1 + Math.trunc((B2 - B1) * Index / Denominator);

            return this.Color(R, G, B) + Character;
        });

        return Colored.join("") + this.Reset();
    }

    Line()
    {
        const Width = this.Width();
        let Text = "  ";

        for (let Index = 0; Index < Width; Index++)
        {
            const R = 48 + Math.trunc(35 * Index / Width);
            const G = 65 + Math.trunc(35 * Index / Width);
            const B = 88 + Math.trunc(55 * Index / Width);

            Text += this.Color(R, G, B) + "─";
        }

        return Text + this.Reset() + "\n";
    }

    Header(Title = "LineOS Builder")
    {
        let Text = "\n" + this.Line();

        Text += "  " + this.Bold();
        Text += this.Gradient(Title, 85, 205, 245, 175, 145, 245);
        Text += this.Reset() + "\n";
        Text += this.Line() + "\n";

        this.Output.write(Text);
    }

    Progress(Current, Total, Width)
    {
        Current = Math.min(Current, Total);

        const Filled = Math.trunc(Current * Width / Total);
        let Text = this.Color(65, 80, 100) + "[";

        for (let Index = 0; Index < Width; Index++)
        {
            if (Index < Filled)
            {
                const R = 75 + Math.trunc(65 * Index / Width);
                const G = 195 + Math.trunc(15 * Index / Width);
                const B = 245 - Math.trunc(10 * Index / Width);

                Text += this.Color(R, G, B) + "━";
            }
            else
            {
                Text += this.Color(55, 68, 85) + "─";
            }
        }

        Text += this.Color(65, 80, 100) + "] ";
        Text += this.Color(140, 155, 175);
        Text += `${Math.trunc(Current * 100 / Total)}%`.padStart(4);
        Text += this.Reset();

        return Text;
    }

    Message(Kind, Text)
    {
        let Color = [110, 180, 255];
        let Symbol = "→";

        if (Kind === "success")
        {
            Color = [80, 220, 140];
            Symbol = "✓";
        }
        else if (Kind === "error")
        {
            Color = [255, 90, 110];
            Symbol = "✗";
        }

        const Output = Kind === "error" ? this.ErrorOutput : this.Output;
        const Prefix = this.Color(...Color) + `  ${Symbol} ` + this.Reset();

        Output.write(Prefix + Text + "\n");
    }

    StageLine(Symbol, Duration, State, Data)
    {
        const [Total, Current, Completed, File] = Data;
        const Count = `[${Total}/${Current}]`;
        let BarWidth = Math.min(40, Math.trunc(this.Width() / 3));
        let NameWidth = this.Width() - Count.length - BarWidth - 22;

        if (NameWidth < 8)
        {
            BarWidth = 6;
            NameWidth = this.Width() - Count.length - BarWidth - 22;
        }

        NameWidth = Math.max(1, NameWidth);

        let Name = File;

        if (Array.from(Name).length > NameWidth)
        {
            Name = NameWidth === 1 ? "…" : "…" + Array.from(Name).slice(-(NameWidth - 1)).join("");
        }

        let Color = [100, 195, 240];

        if (State === "success")
        {
            Color = [100, 215, 165];
        }
        else if (State === "error")
        {
            Color = [245, 115, 125];
        }

        const ProgressTotal = Math.max(1, Number(Total));
        const ProgressCurrent = State === "success" ? ProgressTotal : Number(Completed);
        let Text = this.Color(...Color) + `  ${Symbol} `;

        Text += this.Color(125, 140, 160) + Count + " ";
        Text += this.Progress(ProgressCurrent, ProgressTotal, BarWidth);
        Text += this.Reset() + " " + Name;
        Text += " ".repeat(Math.max(0, NameWidth - Array.from(Name).length)) + " ";
        Text += this.Color(...(State === "running" ? [140, 155, 175] : Color));
        Text += Duration.padStart(7) + "s" + this.Reset();
        Text += State === "running" ? "" : "\n";

        const Output = State === "error" ? this.ErrorOutput : this.Output;

        Output.write(Text);
    }

    CellWidth(Text)
    {
        return Array.from(Text).reduce((Width, Character) =>
        {
            const CharacterWidth = /[가-힣ㄱ-ㅎㅏ-ㅣ]/.test(Character) ? 2 : 1;

            return Width + CharacterWidth;
        }, 0);
    }

    Table(Rows)
    {
        const Cells = Rows.map(Row => Row.split("|"));
        const Widths = [0, 1].map(Index =>
        {
            const ColumnWidths = Cells.map(Row => this.CellWidth(Row[Index] || ""));

            return Math.max(...ColumnWidths);
        });
        const Border = (Left, Middle, Right) =>
        {
            let Text = this.Reset() + this.Color(65, 80, 100) + "  " + Left;

            Text += "━".repeat(Widths[0] + 2) + Middle;
            Text += "━".repeat(Widths[1] + 2) + Right;
            Text += "\n" + this.Reset();

            return Text;
        };

        let Text = Border("┏", "┳", "┓");

        Cells.forEach((Row, Index) =>
        {
            Text += this.Reset() + this.Color(65, 80, 100) + "  ┃";

            Row.forEach((Cell, Column) =>
            {
                if (Rows[Index].includes("Total"))
                {
                    Text += this.Bold() + this.Color(100, 215, 165);
                }
                else
                {
                    Text += this.Color(175, 190, 210);
                }

                Text += " " + Cell;
                Text += " ".repeat(Widths[Column] - this.CellWidth(Cell)) + " ";
                Text += this.Reset() + this.Color(65, 80, 100) + "┃";
            });

            Text += "\n" + this.Reset();

            if (Index === 0 && Cells.length > 1)
            {
                Text += Border("┣", "╋", "┫");
            }
        });

        this.Output.write(Text + Border("┗", "┻", "┛"));
    }

    Summary(Build, Image, Mount, Total)
    {
        const Header = "Stage".padEnd(Math.max(5, this.Width() - Total.length - 9));
        this.Output.write("\n" + this.Color(125, 140, 160) + "  BUILD SUMMARY\n" + this.Reset());
        const Rows = [
            `${Header}|Time`,
            `Build|${Build}s`,
            `Image|${Image}s`,
            `Boot files|${Mount}s`,
            `Total|${Total}s`
        ];

        this.Table(Rows);
        this.Output.write("\n");
    }

    QEMUStatus(Symbol, Message, Duration)
    {
        let Color = [100, 195, 240];

        if (Symbol === "✓")
        {
            Color = [100, 215, 165];
        }
        else if (Symbol === "✗")
        {
            Color = [245, 115, 125];
        }

        let Text = this.Color(...Color) + `  ${Symbol} `;

        Text += this.Reset() + Message;
        Text += " ".repeat(Math.max(1, this.Width() - Message.length - Duration.length - 5));
        Text += this.Color(140, 155, 175) + Duration + "s" + this.Reset();

        const Output = Symbol === "✗" ? this.ErrorOutput : this.Output;

        Output.write(Text);
    }

    Analysis(Status, Duration, Rows)
    {
        const ValueWidth = Math.max(8, this.Width() - 21);
        const Values = Rows.map(Row =>
        {
            const [Name, Value] = Row.split("|");

            return `${Name}|${Value.length > ValueWidth ? Value.slice(0, ValueWidth - 1) + "…" : Value}`;
        });
        this.Output.write("\n" + this.Color(125, 140, 160) + "  QEMU ANALYSIS\n" + this.Reset());
        const RowsToDisplay = [
            `Metric|${"Result".padEnd(ValueWidth)}`,
            `Process|${Number(Status) === 0 ? "Closed" : "Failed"}`,
            `Duration|${Duration}s`,
            `Exit code|${Status}`,
            ...Values
        ];

        this.Table(RowsToDisplay);
        this.Output.write("\n");
    }

    Duration(Start)
    {
        return (Math.floor(Number(process.hrtime.bigint() - Start) / 10000000) / 100).toFixed(2);
    }

    Stop()
    {
        if (this.Timer)
        {
            clearInterval(this.Timer);
            this.Timer = null;
            this.Output.write("\r\x1b[2K");
        }

        this.Active = null;
    }

    Start(Kind, File, Message)
    {
        this.Stop();
        this.Active = {
            Kind,
            File,
            Start: process.hrtime.bigint(),
            Index: 0,
            Data: [1, 0, 0, Message]
        };

        if (Kind === "stage")
        {
            this.Message("info", Message);
        }
        else if (!this.Animated)
        {
            this.Message("info", "QEMU running");
        }

        if (this.Animated)
        {
            this.Timer = setInterval(() =>
            {
                const Active = this.Active;

                if (Kind === "stage")
                {
                    try
                    {
                        const Data = FS.readFileSync(File, "utf8").trimEnd().split("\t");

                        if (Data.length === 4)
                        {
                            Active.Data = Data;
                        }
                    }
                    catch (Error)
                    {
                        if (Error.code !== "ENOENT")
                        {
                            this.Stop();
                            this.Message("error", Error.message);
                            return;
                        }
                    }
                }

                this.Output.write("\r\x1b[2K");
                const Symbol = this.Frames[Active.Index++ % this.Frames.length];

                if (Kind === "stage")
                {
                    this.StageLine(Symbol, this.Duration(Active.Start), "running", Active.Data);
                }
                else
                {
                    this.QEMUStatus(Symbol, "QEMU running", this.Duration(Active.Start));
                }
            }, 80);
        }
    }

    Help()
    {
        let Text = "  Usage: ./Scripts/Run.sh [option]\n\n";

        Text += "  Options:\n";
        Text += "    (none), -all    Unmount -> build/update -> QEMU -> mount\n";
        Text += "    -build [path]   Build all or one program (incremental)\n";
        Text += "    -clean          Make Clean\n";
        Text += "    -format         Make Format\n";
        Text += "    -image          Prepare disk image and boot files\n";
        Text += "    -m, -mount      Mount EFI and root partitions\n";
        Text += "    -u, -umount     Unmount image and detach loop devices\n";
        Text += "    -qemu           Run QEMU, then restore image mounts\n";
        Text += "    -h, --help      Show this help message\n\n";

        this.Header();
        this.Output.write(Text);
    }

    Handle(Command, ...Arguments)
    {
        switch (Command)
        {
        case "sync":
            FS.writeFileSync(Arguments[0], "ready");
            break;

        case "header":
            this.Header(Arguments[0]);
            break;

        case "message":
            this.Message(...Arguments);
            break;

        case "summary":
            this.Summary(...Arguments);
            break;

        case "analysis":
            this.Analysis(Arguments[0], Arguments[1], Arguments.slice(2));
            break;

        case "stage-start":
            this.Start("stage", Arguments[0], Arguments[1]);
            break;

        case "stage-end":
        {
            this.Stop();

            const Success = Number(Arguments[0]) === 0;
            const Symbol = Success ? "✓" : "✗";
            const State = Success ? "success" : "error";

            this.StageLine(Symbol, Arguments[1], State, Arguments.slice(2));
            break;
        }

        case "qemu-start":
            this.Start("qemu", "", "");
            break;

        case "qemu-end":
        {
            this.Stop();

            const Success = Number(Arguments[0]) === 0;
            const Symbol = Success ? "✓" : "✗";
            const Message = Success ? "QEMU closed" : `QEMU failed (exit ${Arguments[0]})`;
            const Output = Success ? this.Output : this.ErrorOutput;

            this.QEMUStatus(Symbol, Message, Arguments[1]);
            Output.write("\n");
            break;
        }

        case "checks":
            if (this.Animated || Arguments[0] === Arguments[1])
            {
                const Current = Number(Arguments[0]);
                const Total = Number(Arguments[1]);
                const Width = Math.max(4, this.Width() - 19);
                let Text = this.Animated ? "\r\x1b[2K" : "";

                Text += "  " + this.Progress(Current, Total, Width);
                Text += `  Checks ${Arguments[0]}/${Arguments[1]}`;

                this.Output.write(Text);
            }

            if (Arguments[0] === Arguments[1])
            {
                this.Output.write("\n\n");
            }

            break;

        case "error-text":
            this.ErrorOutput.write(Arguments[0]);
            break;

        case "help":
            this.Help();
            break;

        default:
            throw new Error(`Unknown UI command: ${Command}`);
        }
    }
}

function Main()
{
    const UI = new Renderer();
    const Decoder = new (require("node:string_decoder").StringDecoder)("utf8");
    let Pending = "";
    let Count = null;
    let Fields = [];

    process.stdin.on("data", Chunk =>
    {
        Pending += Decoder.write(Chunk);

        let Index;

        while ((Index = Pending.indexOf("\0")) !== -1)
        {
            const Field = Pending.slice(0, Index);
            Pending = Pending.slice(Index + 1);

            if (Count === null)
            {
                Count = Number(Field);

                if (!Number.isInteger(Count) || Count < 1 || Count > 1000)
                {
                    throw new Error("Invalid UI frame");
                }
            }
            else
            {
                Fields.push(Field);

                if (Fields.length === Count)
                {
                    UI.Handle(...Fields);
                    Count = null;
                    Fields = [];
                }
            }
        }
    });

    process.stdin.on("end", () =>
    {
        UI.Stop();

        if (Count !== null || Pending.length)
        {
            process.exitCode = 1;
        }
    });

    process.on("SIGINT", () =>
    {
        UI.Stop();
        process.exit(130);
    });

    process.on("SIGTERM", () =>
    {
        UI.Stop();
        process.exit(143);
    });
}

if (require.main === module)
{
    Main();
}

module.exports = {Renderer};
