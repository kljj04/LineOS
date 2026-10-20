// Kernel/Source/Render/Web/Web.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/Web/Web.h>
#include <Render/TrueType/TrueTypeEngine.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Network/Network.h>
#include <Input/Input.h>
#include <Scheduler/Task.h>
#include <Memory/PMM.h>
#include <Debug/Debug.h>

#define WEB_HTML_LIMIT 65536
#define WEB_NODE_LIMIT 768
#define WEB_RULE_LIMIT 128
#define WEB_RUN_LIMIT  4096
#define WEB_URL_LIMIT  512
#define WEB_COLOR      1
#define WEB_BACKGROUND 2
#define WEB_FONT_SIZE  4
#define WEB_MARGIN     8
#define WEB_PADDING    16
#define WEB_MAX_WIDTH  32
#define WEB_CENTER     64
#define WEB_HIDDEN     128

#define WEB_HOME "http://info.cern.ch/hypertext/WWW/TheProject.html"

typedef struct
{
    UINT32  Mask;
    UINT32  Color;
    UINT32  Background;
    UINT32  FontSize;
    UINT32  Margin;
    UINT32  Padding;
    UINT32  MaxWidth;
    BOOLEAN Center;
    BOOLEAN Hidden;
} WEB_STYLE;

typedef struct
{
    CHAR8     Tag[16];
    CHAR8     ID[48];
    CHAR8     Class[96];
    CHAR8     Href[WEB_URL_LIMIT];
    CHAR8     Inline[512];
    UINT16    Parent;
    UINT16    FirstChild;
    UINT16    LastChild;
    UINT16    Next;
    UINT32    Text;
    UINT32    Length;
    WEB_STYLE Style;
    BOOLEAN   Stylesheet;
} WEB_NODE;

typedef struct
{
    CHAR8     Selector[96];
    WEB_STYLE Style;
    UINT32    Specificity;
} WEB_RULE;

typedef struct
{
    CHAR16  Text[96];
    UINT32  X;
    UINT32  Y;
    UINT32  Width;
    UINT32  Height;
    UINT32  Size;
    UINT32  Color;
    UINT16  Link;
    BOOLEAN Underline;
} WEB_RUN;

typedef struct
{
    UINT32 X;
    UINT32 Y;
    UINT32 Width;
    UINT32 Height;
    UINT32 Color;
} WEB_BOX;

typedef struct
{
    UINT32  Left;
    UINT32  Right;
    UINT32  X;
    UINT32  Y;
    UINT32  LineHeight;
    BOOLEAN Space;
} WEB_FLOW;

STATIC WEB_NODE Nodes[WEB_NODE_LIMIT];
STATIC WEB_RULE Rules[WEB_RULE_LIMIT];
STATIC WEB_RUN  Runs[WEB_RUN_LIMIT];
STATIC WEB_BOX  Boxes[WEB_NODE_LIMIT];
STATIC UINT8    HTML[WEB_HTML_LIMIT];
STATIC CHAR8    Text[WEB_HTML_LIMIT];
STATIC UINT32   TextUsed;
STATIC UINT32   NodeCount;
STATIC UINT32   RuleCount;
STATIC UINT32   RunCount;
STATIC UINT32   BoxCount;
STATIC UINT32   DocumentHeight;
STATIC UINT32   Scale;
STATIC UINT32   ViewX;
STATIC UINT32   ViewY;
STATIC UINT32   ViewWidth;
STATIC UINT32   ViewHeight;
STATIC UINT32   Scroll;
STATIC UINT32   PageBackground;
STATIC UINT16   BodyNode;
STATIC CHAR8    CurrentURL[WEB_URL_LIMIT];
STATIC CHAR8    Address[WEB_URL_LIMIT];
STATIC CHAR8    History[16][WEB_URL_LIMIT];
STATIC UINT32   HistoryCount;
STATIC CHAR8    PageTitle[160];
STATIC CHAR16   Message[160];
STATIC UINT32   Bytes;
STATIC UINT16   Status;
STATIC BOOLEAN  Editing;
STATIC BOOLEAN  Truncated;

STATIC CHAR8 Lower(CHAR8 Value)
{
    return Value >= 'A' && Value <= 'Z' ? Value + 'a' - 'A' : Value;
}

STATIC BOOLEAN Space(CHAR8 Value)
{
    return Value == ' ' || Value == '\t' || Value == '\r' || Value == '\n' || Value == '\f';
}

STATIC UINT32 Length(CONST CHAR8 *String)
{
    UINT32 Count = 0;

    while (String[Count] != 0)
    {
        Count++;
    }

    return Count;
}

STATIC VOID Copy(CHAR8 *Destination, CONST CHAR8 *Source, UINT32 Capacity)
{
    UINT32 Index = 0;

    while (Index + 1 < Capacity && Source[Index] != 0)
    {
        Destination[Index] = Source[Index];
        Index++;
    }

    Destination[Index] = 0;
}

STATIC BOOLEAN Equal(CONST CHAR8 *A, CONST char *B)
{
    while (*A != 0 && *B != 0)
    {
        if (Lower(*A++) != Lower((CHAR8) *B++))
        {
            return FALSE;
        }
    }

    return *A == 0 && *B == 0;
}

STATIC BOOLEAN Prefix(CONST CHAR8 *A, CONST char *B)
{
    while (*B != 0)
    {
        if (Lower(*A++) != Lower((CHAR8) *B++))
        {
            return FALSE;
        }
    }

    return TRUE;
}

STATIC UINT32 Number(CONST CHAR8 *String)
{
    UINT32 Value = 0;

    while (*String >= '0' && *String <= '9')
    {
        if (Value > 10000)
        {
            return 10000;
        }

        Value = Value * 10 + *String++ - '0';
    }

    return Value;
}

STATIC BOOLEAN PixelValue(CONST CHAR8 *Value)
{
    UINT32 Offset = 0;

    while (Value[Offset] >= '0' && Value[Offset] <= '9')
    {
        Offset++;
    }

    return Offset != 0 && (Value[Offset] == 0 || (Value[Offset] == 'p' && Value[Offset + 1] == 'x' && Value[Offset + 2] == 0));
}

STATIC UINT32 Hex(CHAR8 Ch)
{
    if (Ch >= '0' && Ch <= '9')
    {
        return Ch - '0';
    }

    Ch = Lower(Ch);
    return Ch >= 'a' && Ch <= 'f' ? Ch - 'a' + 10 : 16;
}

STATIC BOOLEAN Color(CONST CHAR8 *Value, UINT32 *Result)
{
    if (*Value == '#')
    {
        UINT32 Count = 0;
        UINT32 RGB = 0;
        Value++;

        while (Hex(Value[Count]) < 16 && Count < 6)
        {
            RGB = RGB * 16 + Hex(Value[Count++]);
        }

        if (Count == 3)
        {
            RGB = ((RGB & 0xF00) << 8) | ((RGB & 0xF00) << 4) | ((RGB & 0xF0) << 4) | (RGB & 0xF0) | ((RGB & 15) << 4) | (RGB & 15);
        }
        else if (Count != 6)
        {
            return FALSE;
        }

        *Result = RGB;
        return TRUE;
    }

    if (Equal(Value, "white"))
    {
        *Result = 0xFFFFFF;
    }
    else if (Equal(Value, "black"))
    {
        *Result = 0;
    }
    else if (Equal(Value, "navy"))
    {
        *Result = 0x132C59;
    }
    else if (Equal(Value, "blue"))
    {
        *Result = 0x2054CC;
    }
    else if (Equal(Value, "red"))
    {
        *Result = 0xCC3344;
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

STATIC VOID ParseStyle(CONST CHAR8 *CSS, WEB_STYLE *Style)
{
    UINT32 Offset = 0;

    while (CSS[Offset] != 0)
    {
        CHAR8  Property[48];
        CHAR8  Value[160];
        UINT32 Count = 0;

        while (Space(CSS[Offset]) || CSS[Offset] == ';')
        {
            Offset++;
        }

        while (CSS[Offset] != 0 && CSS[Offset] != ':' && CSS[Offset] != ';')
        {
            if (Count + 1 < sizeof(Property) && !Space(CSS[Offset]))
            {
                Property[Count++] = Lower(CSS[Offset]);
            }

            Offset++;
        }

        Property[Count] = 0;

        if (CSS[Offset] != ':')
        {
            if (CSS[Offset] != 0)
            {
                Offset++;
                continue;
            }

            break;
        }

        Offset++;

        while (Space(CSS[Offset]))
        {
            Offset++;
        }

        Count = 0;

        while (CSS[Offset] != 0 && CSS[Offset] != ';')
        {
            if (Count + 1 < sizeof(Value))
            {
                Value[Count++] = CSS[Offset];
            }

            Offset++;
        }

        while (Count != 0 && Space(Value[Count - 1]))
        {
            Count--;
        }

        Value[Count] = 0;

        if (Equal(Property, "color") && Color(Value, &Style->Color))
        {
            Style->Mask |= WEB_COLOR;
        }
        else if ((Equal(Property, "background") || Equal(Property, "background-color")) && Color(Value, &Style->Background))
        {
            Style->Mask |= WEB_BACKGROUND;
        }
        else if (Equal(Property, "font-size") && PixelValue(Value) && Number(Value) >= 8 && Number(Value) <= 72)
        {
            Style->FontSize = Number(Value);
            Style->Mask |= WEB_FONT_SIZE;
        }
        else if (Equal(Property, "margin") && PixelValue(Value) && Value[0] >= '0' && Value[0] <= '9')
        {
            Style->Margin = Number(Value) > 64 ? 64 : Number(Value);
            Style->Mask |= WEB_MARGIN;
        }
        else if (Equal(Property, "padding") && PixelValue(Value) && Value[0] >= '0' && Value[0] <= '9')
        {
            Style->Padding = Number(Value) > 64 ? 64 : Number(Value);
            Style->Mask |= WEB_PADDING;
        }
        else if (Equal(Property, "max-width") && PixelValue(Value) && Number(Value) >= 100)
        {
            Style->MaxWidth = Number(Value);
            Style->Mask |= WEB_MAX_WIDTH;
        }
        else if (Equal(Property, "text-align"))
        {
            Style->Center = Equal(Value, "center");
            Style->Mask |= WEB_CENTER;
        }
        else if (Equal(Property, "display"))
        {
            Style->Hidden = Equal(Value, "none");
            Style->Mask |= WEB_HIDDEN;
        }
    }
}

STATIC VOID ParseCSS(CONST CHAR8 *CSS, UINT32 Size)
{
    UINT32 Offset = 0;

    while (Offset < Size && RuleCount < WEB_RULE_LIMIT)
    {
        UINT32 Start;
        CHAR8  Declarations[1024];
        UINT32 Count;

        while (Offset < Size && Space(CSS[Offset]))
        {
            Offset++;
        }

        if (Offset + 1 < Size && CSS[Offset] == '/' && CSS[Offset + 1] == '*')
        {
            Offset += 2;

            while (Offset + 1 < Size && !(CSS[Offset] == '*' && CSS[Offset + 1] == '/'))
            {
                Offset++;
            }

            Offset += 2;
            continue;
        }

        Start = Offset;

        while (Offset < Size && CSS[Offset] != '{')
        {
            Offset++;
        }

        if (Offset == Size)
        {
            break;
        }

        UINT32 SelectorEnd = Offset++;
        Count = 0;
        UINT32 Depth = 1;

        while (Offset < Size && Depth != 0)
        {
            CHAR8 Ch = CSS[Offset++];

            if (Ch == '{')
            {
                Depth++;
            }
            else if (Ch == '}')
            {
                Depth--;
            }

            if (Depth != 0 && Count + 1 < sizeof(Declarations))
            {
                Declarations[Count++] = Ch;
            }
        }

        Declarations[Count] = 0;

        if (CSS[Start] == '@')
        {
            continue;
        }

        while (Start < SelectorEnd && RuleCount < WEB_RULE_LIMIT)
        {
            WEB_RULE *Rule = &Rules[RuleCount];
            Count = 0;

            while (Start < SelectorEnd && CSS[Start] != ',')
            {
                if (Count + 1 < sizeof(Rule->Selector))
                {
                    Rule->Selector[Count++] = CSS[Start];
                }

                Start++;
            }

            if (Start < SelectorEnd)
            {
                Start++;
            }

            while (Count != 0 && Space(Rule->Selector[Count - 1]))
            {
                Count--;
            }

            Rule->Selector[Count] = 0;
            ParseStyle(Declarations, &Rule->Style);
            RuleCount++;
        }
    }
}

STATIC VOID Attribute(CONST CHAR8 *Start, UINT32 Size, CONST char *Name, CHAR8 *Output, UINT32 Capacity)
{
    UINT32 Offset = 0;
    Output[0] = 0;

    while (Offset < Size)
    {
        CHAR8  Key[32];
        UINT32 Count = 0;
        UINT32 Begin;
        UINT32 End;
        CHAR8  Quote = 0;

        while (Offset < Size && (Space(Start[Offset]) || Start[Offset] == '/'))
        {
            Offset++;
        }

        while (Offset < Size && !Space(Start[Offset]) && Start[Offset] != '=')
        {
            if (Count + 1 < sizeof(Key))
            {
                Key[Count++] = Lower(Start[Offset]);
            }

            Offset++;
        }

        Key[Count] = 0;

        while (Offset < Size && Space(Start[Offset]))
        {
            Offset++;
        }

        if (Offset >= Size || Start[Offset] != '=')
        {
            continue;
        }

        Offset++;

        while (Offset < Size && Space(Start[Offset]))
        {
            Offset++;
        }

        if (Offset < Size && (Start[Offset] == '\'' || Start[Offset] == '"'))
        {
            Quote = Start[Offset++];
        }

        Begin = Offset;

        while (Offset < Size && (Quote != 0 ? Start[Offset] != Quote : !Space(Start[Offset])))
        {
            Offset++;
        }

        End = Offset;

        if (Quote != 0 && Offset < Size)
        {
            Offset++;
        }

        if (Equal(Key, Name))
        {
            Count = End - Begin;
            Count = Count < Capacity - 1 ? Count : Capacity - 1;
            KMemCpy(Output, Start + Begin, Count);
            Output[Count] = 0;
            return;
        }
    }
}

STATIC BOOLEAN VoidTag(CONST CHAR8 *Tag)
{
    return Equal(Tag, "br") || Equal(Tag, "hr") || Equal(Tag, "img") || Equal(Tag, "meta") || Equal(Tag, "link") || Equal(Tag, "input") || Equal(Tag, "nextid");
}

STATIC UINT16 AddNode(UINT16 Parent)
{
    UINT16 Index;

    if (NodeCount >= WEB_NODE_LIMIT)
    {
        Truncated = TRUE;
        return 0;
    }

    Index = (UINT16) NodeCount++;
    Nodes[Index].Parent = Parent;

    if (Nodes[Parent].LastChild != 0)
    {
        Nodes[Nodes[Parent].LastChild].Next = Index;
    }
    else
    {
        Nodes[Parent].FirstChild = Index;
    }

    Nodes[Parent].LastChild = Index;
    return Index;
}

STATIC VOID ParseHTML(UINT32 Size)
{
    UINT16 Parent = 0;
    UINT32 Offset = 0;
    UINT32 Depth = 0;

    KMemSet(Nodes, 0, sizeof(Nodes));
    KMemSet(Rules, 0, sizeof(Rules));
    NodeCount = 1;
    RuleCount = 0;
    TextUsed = 0;
    BodyNode = 0;
    Truncated = FALSE;
    PageTitle[0] = 0;

    while (Offset < Size && NodeCount < WEB_NODE_LIMIT)
    {
        if (HTML[Offset] == '<')
        {
            UINT32  Start = ++Offset;
            UINT32  NameEnd;
            UINT32  End;
            CHAR8   Tag[16];
            CHAR8   Quote = 0;
            BOOLEAN Closing = FALSE;

            if (Start + 2 < Size && HTML[Start] == '!' && HTML[Start + 1] == '-' && HTML[Start + 2] == '-')
            {
                Offset += 3;

                while (Offset + 2 < Size && !(HTML[Offset] == '-' && HTML[Offset + 1] == '-' && HTML[Offset + 2] == '>'))
                {
                    Offset++;
                }

                Offset = Offset + 2 < Size ? Offset + 3 : Size;
                continue;
            }

            if (Offset < Size && HTML[Offset] == '/')
            {
                Closing = TRUE;
                Offset++;
            }

            Start = Offset;

            while (Offset < Size && !Space(HTML[Offset]) && HTML[Offset] != '>' && HTML[Offset] != '/')
            {
                Offset++;
            }

            NameEnd = Offset;

            while (Offset < Size)
            {
                CHAR8 Ch = HTML[Offset++];

                if (Quote != 0)
                {
                    if (Ch == Quote)
                    {
                        Quote = 0;
                    }
                }
                else if (Ch == '\'' || Ch == '"')
                {
                    Quote = Ch;
                }
                else if (Ch == '>')
                {
                    break;
                }
            }

            End = Offset;
            UINT32 Count = NameEnd - Start;
            Count = Count < sizeof(Tag) - 1 ? Count : sizeof(Tag) - 1;

            for (UINT32 Index = 0; Index < Count; Index++)
            {
                Tag[Index] = Lower(HTML[Start + Index]);
            }

            Tag[Count] = 0;

            if (Tag[0] == '!' || Tag[0] == '?' || Tag[0] == 0)
            {
                continue;
            }

            if (Closing)
            {
                UINT16 Match = Parent;
                UINT32 Levels = Depth;

                while (Match != 0 && !Equal(Nodes[Match].Tag, (CONST char *) Tag))
                {
                    Match = Nodes[Match].Parent;

                    if (Levels != 0)
                    {
                        Levels--;
                    }
                }

                if (Match != 0)
                {
                    Parent = Nodes[Match].Parent;
                    Depth = Levels == 0 ? 0 : Levels - 1;
                }

                continue;
            }

            if (Equal(Tag, "style") || Equal(Tag, "script") || Equal(Tag, "title"))
            {
                UINT32 Content = Offset;

                while (Offset < Size && !(HTML[Offset] == '<' && Offset + 2 + Length(Tag) <= Size && HTML[Offset + 1] == '/' && Prefix(HTML + Offset + 2, (CONST char *) Tag)))
                {
                    Offset++;
                }

                if (Equal(Tag, "style"))
                {
                    UINT32 CSSLength = Offset - Content;
                    UINT16 Index = AddNode(Parent);

                    if (Index != 0 && TextUsed + CSSLength + 1 < sizeof(Text))
                    {
                        Copy(Nodes[Index].Tag, (CONST CHAR8 *) "style", sizeof(Nodes[Index].Tag));
                        Nodes[Index].Text = TextUsed;
                        Nodes[Index].Length = CSSLength;
                        KMemCpy(Text + TextUsed, HTML + Content, CSSLength);
                        TextUsed += CSSLength;
                        Text[TextUsed++] = 0;
                    }
                }
                else if (Equal(Tag, "title"))
                {
                    Count = Offset - Content;
                    Count = Count < sizeof(PageTitle) - 1 ? Count : sizeof(PageTitle) - 1;
                    KMemCpy(PageTitle, HTML + Content, Count);
                    PageTitle[Count] = 0;
                }

                continue;
            }

            if (Equal(Tag, "head") || Equal(Tag, "header") || Equal(Tag, "html"))
            {
                continue;
            }

            if ((Equal(Tag, "p") && Equal(Nodes[Parent].Tag, "p")) || ((Equal(Tag, "dt") || Equal(Tag, "dd")) && (Equal(Nodes[Parent].Tag, "dt") || Equal(Nodes[Parent].Tag, "dd"))) || (Equal(Tag, "li") && Equal(Nodes[Parent].Tag, "li")))
            {
                Parent = Nodes[Parent].Parent;
                Depth = Depth == 0 ? 0 : Depth - 1;
            }

            UINT16 Index = AddNode(Parent);

            if (Index == 0)
            {
                break;
            }

            Copy(Nodes[Index].Tag, Tag, sizeof(Nodes[Index].Tag));
            UINT32 AttributeSize = End > NameEnd ? End - NameEnd - 1 : 0;
            Attribute(HTML + NameEnd, AttributeSize, "id", Nodes[Index].ID, sizeof(Nodes[Index].ID));
            Attribute(HTML + NameEnd, AttributeSize, "class", Nodes[Index].Class, sizeof(Nodes[Index].Class));
            Attribute(HTML + NameEnd, AttributeSize, "href", Nodes[Index].Href, sizeof(Nodes[Index].Href));
            Attribute(HTML + NameEnd, AttributeSize, "style", Nodes[Index].Inline, sizeof(Nodes[Index].Inline));

            if (Equal(Tag, "link"))
            {
                CHAR8 Rel[64];
                Attribute(HTML + NameEnd, AttributeSize, "rel", Rel, sizeof(Rel));
                Nodes[Index].Stylesheet = Equal(Rel, "stylesheet");
            }

            if (Equal(Tag, "body"))
            {
                BodyNode = Index;
            }

            if (!VoidTag(Tag) && Depth < 32)
            {
                Parent = Index;
                Depth++;
            }
        }
        else
        {
            UINT32 Start = Offset;

            while (Offset < Size && HTML[Offset] != '<')
            {
                Offset++;
            }

            UINT32 Count = Offset - Start;

            if (TextUsed + Count + 1 >= sizeof(Text))
            {
                Truncated = TRUE;
                break;
            }

            UINT16 Index = AddNode(Parent);

            if (Index == 0)
            {
                break;
            }

            Nodes[Index].Text = TextUsed;
            Nodes[Index].Length = Count;
            KMemCpy(Text + TextUsed, HTML + Start, Count);
            TextUsed += Count;
            Text[TextUsed++] = 0;
        }
    }

    if (Offset < Size)
    {
        Truncated = TRUE;
    }
}

STATIC BOOLEAN HasClass(CONST CHAR8 *Classes, CONST CHAR8 *Name)
{
    UINT32 Offset = 0;
    UINT32 Size = Length(Name);

    while (Classes[Offset] != 0)
    {
        while (Space(Classes[Offset]))
        {
            Offset++;
        }

        UINT32 Start = Offset;

        while (Classes[Offset] != 0 && !Space(Classes[Offset]))
        {
            Offset++;
        }

        if (Offset - Start == Size)
        {
            BOOLEAN Match = TRUE;

            for (UINT32 Index = 0; Index < Size; Index++)
            {
                Match &= Classes[Start + Index] == Name[Index];
            }

            if (Match)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

STATIC BOOLEAN MatchCompound(UINT16 Node, CONST CHAR8 *Selector, UINT32 *Specificity)
{
    UINT32 Offset = 0;
    CHAR8  Part[96];

    if (Selector[Offset] == '*')
    {
        Offset++;
    }
    else if (Selector[Offset] != '.' && Selector[Offset] != '#')
    {
        UINT32 Count = 0;

        while (Selector[Offset] != 0 && Selector[Offset] != '.' && Selector[Offset] != '#')
        {
            Part[Count++] = Selector[Offset++];
        }

        Part[Count] = 0;

        if (!Equal(Nodes[Node].Tag, (CONST char *) Part))
        {
            return FALSE;
        }

        (*Specificity)++;
    }

    while (Selector[Offset] != 0)
    {
        CHAR8  Kind = Selector[Offset++];
        UINT32 Count = 0;

        if (Kind != '#' && Kind != '.')
        {
            return FALSE;
        }

        while (Selector[Offset] != 0 && Selector[Offset] != '#' && Selector[Offset] != '.')
        {
            Part[Count++] = Selector[Offset++];
        }

        Part[Count] = 0;

        if (Count == 0 || (Kind == '#' ? !Equal(Nodes[Node].ID, (CONST char *) Part) : !HasClass(Nodes[Node].Class, Part)))
        {
            return FALSE;
        }

        *Specificity += Kind == '#' ? 100 : 10;
    }

    return TRUE;
}

STATIC BOOLEAN MatchSelector(UINT16 Node, CONST CHAR8 *Selector, UINT32 *Specificity)
{
    UINT32  End = Length(Selector);
    BOOLEAN First = TRUE;
    *Specificity = 0;

    while (End != 0)
    {
        CHAR8  Part[96];
        UINT32 Start;

        while (End != 0 && Space(Selector[End - 1]))
        {
            End--;
        }

        Start = End;

        while (Start != 0 && !Space(Selector[Start - 1]))
        {
            Start--;
        }

        if (End == Start)
        {
            break;
        }

        KMemCpy(Part, Selector + Start, End - Start);
        Part[End - Start] = 0;
        UINT32 Score = 0;

        while (!MatchCompound(Node, Part, &Score))
        {
            if (First || Node == 0)
            {
                return FALSE;
            }

            Node = Nodes[Node].Parent;
            Score = 0;
        }

        *Specificity += Score;
        First = FALSE;
        End = Start;

        if (End != 0)
        {
            if (Node == 0)
            {
                return FALSE;
            }

            Node = Nodes[Node].Parent;
        }
    }

    return !First;
}

STATIC VOID ApplyStyle(WEB_STYLE *Destination, CONST WEB_STYLE *Source, UINT32 Mask)
{
    if (Mask & WEB_COLOR)
        Destination->Color = Source->Color;
    if (Mask & WEB_BACKGROUND)
        Destination->Background = Source->Background;
    if (Mask & WEB_FONT_SIZE)
        Destination->FontSize = Source->FontSize;
    if (Mask & WEB_MARGIN)
        Destination->Margin = Source->Margin;
    if (Mask & WEB_PADDING)
        Destination->Padding = Source->Padding;
    if (Mask & WEB_MAX_WIDTH)
        Destination->MaxWidth = Source->MaxWidth;
    if (Mask & WEB_CENTER)
        Destination->Center = Source->Center;
    if (Mask & WEB_HIDDEN)
        Destination->Hidden = Source->Hidden;
    Destination->Mask |= Mask;
}

STATIC VOID ComputeStyles(VOID)
{
    for (UINT32 Index = 0; Index < NodeCount; Index++)
    {
        WEB_NODE *Node = &Nodes[Index];
        WEB_STYLE Style = {0};
        UINT32    Scores[8] = {0};

        Style.Color = Index == 0 ? 0x263345 : Nodes[Node->Parent].Style.Color;
        Style.FontSize = Index == 0 ? 18 : Nodes[Node->Parent].Style.FontSize;
        Style.Center = Index == 0 ? FALSE : Nodes[Node->Parent].Style.Center;
        Style.Hidden = Index == 0 ? FALSE : Nodes[Node->Parent].Style.Hidden;

        if (Equal(Node->Tag, "h1"))
            Style.FontSize = 34;
        if (Equal(Node->Tag, "h2"))
            Style.FontSize = 28;
        if (Equal(Node->Tag, "h3"))
            Style.FontSize = 23;
        if (Equal(Node->Tag, "a"))
            Style.Color = 0x245BBC;
        if (Equal(Node->Tag, "p") || Equal(Node->Tag, "dl") || Equal(Node->Tag, "ul") || Equal(Node->Tag, "ol"))
            Style.Margin = 12;
        if (Equal(Node->Tag, "h1") || Equal(Node->Tag, "h2") || Equal(Node->Tag, "h3"))
            Style.Margin = 18;
        if (Equal(Node->Tag, "dd") || Equal(Node->Tag, "ul") || Equal(Node->Tag, "ol"))
            Style.Padding = 18;
        if (Equal(Node->Tag, "style") || Equal(Node->Tag, "meta") || Equal(Node->Tag, "link") || Equal(Node->Tag, "nextid") || Equal(Node->Tag, "input"))
            Style.Hidden = TRUE;

        for (UINT32 Rule = 0; Rule < RuleCount; Rule++)
        {
            UINT32 Score;

            if (!MatchSelector((UINT16) Index, Rules[Rule].Selector, &Score))
            {
                continue;
            }

            Score++;

            for (UINT32 Property = 0; Property < 8; Property++)
            {
                UINT32 Bit = 1U << Property;

                if ((Rules[Rule].Style.Mask & Bit) != 0 && Score >= Scores[Property])
                {
                    ApplyStyle(&Style, &Rules[Rule].Style, Bit);
                    Scores[Property] = Score;
                }
            }
        }

        WEB_STYLE Inline = {0};
        ParseStyle(Node->Inline, &Inline);
        ApplyStyle(&Style, &Inline, Inline.Mask);

        if (Index != 0 && Nodes[Node->Parent].Style.Hidden)
        {
            Style.Hidden = TRUE;
        }

        Node->Style = Style;
    }
}

STATIC UINT32 Decode(CONST CHAR8 *String, UINT32 Size, UINT32 *Offset)
{
    UINT32 Position = *Offset;
    UINT32 Value = (UINT8) String[Position++];

    if (Value == '&')
    {
        CHAR8  Entity[16];
        UINT32 Count = 0;

        while (Position < Size && Count + 1 < sizeof(Entity) && String[Position] != ';' && !Space(String[Position]))
        {
            Entity[Count++] = String[Position++];
        }

        Entity[Count] = 0;

        if (Position < Size && String[Position] == ';')
        {
            UINT32 Result = 0;

            if (Equal(Entity, "amp"))
                Result = '&';
            if (Equal(Entity, "lt"))
                Result = '<';
            if (Equal(Entity, "gt"))
                Result = '>';
            if (Equal(Entity, "quot"))
                Result = '"';
            if (Equal(Entity, "apos"))
                Result = '\'';
            if (Equal(Entity, "nbsp"))
                Result = ' ';

            if (Entity[0] == '#')
            {
                UINT32 Index = 1;
                UINT32 Base = 10;

                if (Lower(Entity[Index]) == 'x')
                {
                    Base = 16;
                    Index++;
                }

                for (; Entity[Index] != 0; Index++)
                {
                    UINT32 Digit = Hex(Entity[Index]);

                    if (Digit >= Base || Result > 0x10FFFF / Base)
                    {
                        Result = 0;
                        break;
                    }

                    Result = Result * Base + Digit;
                }
            }

            if (Result != 0 && Result <= 0xFFFF && (Result < 0xD800 || Result > 0xDFFF))
            {
                *Offset = Position + 1;
                return Result;
            }
        }
    }
    else if (Value >= 0xC2 && Value <= 0xEF)
    {
        UINT32 Count = Value < 0xE0 ? 1 : 2;
        UINT32 Result = Value & (Count == 1 ? 0x1F : 15);

        if (Count <= Size - Position)
        {
            for (UINT32 Index = 0; Index < Count; Index++)
            {
                UINT8 Ch = String[Position++];

                if ((Ch & 0xC0) != 0x80)
                {
                    *Offset = *Offset + 1;
                    return '?';
                }

                Result = (Result << 6) | (Ch & 0x3F);
            }

            if (Result >= (Count == 1 ? 0x80U : 0x800U) && (Result < 0xD800 || Result > 0xDFFF))
            {
                *Offset = Position;
                return Result;
            }
        }
    }

    *Offset = *Offset + 1;
    return Value < 128 ? Value : '?';
}

STATIC VOID BreakLine(WEB_FLOW *Flow)
{
    if (Flow->X != Flow->Left || Flow->LineHeight != 0)
    {
        Flow->Y += Flow->LineHeight != 0 ? Flow->LineHeight : 26 * Scale;
    }

    Flow->X = Flow->Left;
    Flow->LineHeight = 0;
    Flow->Space = FALSE;
}

STATIC VOID AddWord(WEB_FLOW *Flow, CONST CHAR16 *Word, WEB_STYLE *Style, UINT16 Link)
{
    INT32  Left = 0;
    INT32  Top = 0;
    INT32  Right = 0;
    INT32  Bottom = 0;
    UINT32 FontSize = Style->FontSize * Scale;
    UINT32 Gap = Flow->Space && Flow->X != Flow->Left ? FontSize / 3 : 0;

    MeasureTrueTypeText(PRETENDARD, Word, FontSize, &Left, &Top, &Right, &Bottom);
    UINT32 Width = Right > 0 ? (UINT32) Right + 2 : FontSize / 3;

    if (Width > Flow->Right - Flow->Left)
    {
        UINT32 Count = 0;

        while (Word[Count] != 0)
        {
            Count++;
        }

        if (Count > 1)
        {
            CHAR16 First[96];
            UINT32 Half = Count / 2;
            KMemCpy(First, Word, Half * sizeof(CHAR16));
            First[Half] = 0;
            AddWord(Flow, First, Style, Link);
            AddWord(Flow, Word + Half, Style, Link);
            return;
        }
    }

    if (Flow->X + Gap + Width > Flow->Right && Flow->X != Flow->Left)
    {
        BreakLine(Flow);
        Gap = 0;
    }

    if (RunCount >= WEB_RUN_LIMIT)
    {
        Truncated = TRUE;
        return;
    }

    WEB_RUN *Run = &Runs[RunCount++];
    UINT32   Index = 0;

    while (Word[Index] != 0 && Index + 1 < 96)
    {
        Run->Text[Index] = Word[Index];
        Index++;
    }

    Run->Text[Index] = 0;
    Run->X = Flow->X + Gap;
    Run->Y = Flow->Y;
    Run->Width = Width;
    Run->Height = FontSize * 3 / 2;
    Run->Size = FontSize;
    Run->Color = Style->Color;
    Run->Link = Link;
    Run->Underline = Link != 0;
    Flow->X += Gap + Width;

    if (Run->Height > Flow->LineHeight)
    {
        Flow->LineHeight = Run->Height;
    }

    Flow->Space = FALSE;
}

STATIC VOID LayoutText(WEB_NODE *Node, WEB_FLOW *Flow, UINT16 Link, BOOLEAN Pre)
{
    UINT32 Offset = 0;
    CHAR16 Word[96];
    UINT32 Count = 0;

    while (Offset < Node->Length)
    {
        UINT32 Codepoint = Decode(Text + Node->Text, Node->Length, &Offset);

        if (Codepoint <= 32 || Codepoint == 160)
        {
            if (Count != 0)
            {
                Word[Count] = 0;
                AddWord(Flow, Word, &Node->Style, Link);
                Count = 0;
            }

            if (Pre && Codepoint == '\n')
            {
                if (Flow->X == Flow->Left)
                {
                    Flow->Y += Node->Style.FontSize * Scale * 3 / 2;
                }
                else
                {
                    BreakLine(Flow);
                }
            }
            else
            {
                Flow->Space = TRUE;
            }
        }
        else
        {
            Word[Count++] = (CHAR16) Codepoint;

            if (Count == 95)
            {
                Word[Count] = 0;
                AddWord(Flow, Word, &Node->Style, Link);
                Count = 0;
            }
        }
    }

    if (Count != 0)
    {
        Word[Count] = 0;
        AddWord(Flow, Word, &Node->Style, Link);
    }
}

STATIC BOOLEAN Block(CONST CHAR8 *Tag)
{
    return Equal(Tag, "body") || Equal(Tag, "p") || Equal(Tag, "div") || Equal(Tag, "section") || Equal(Tag, "article") || Equal(Tag, "h1") || Equal(Tag, "h2") || Equal(Tag, "h3") || Equal(Tag, "h4") || Equal(Tag, "ul") || Equal(Tag, "ol") || Equal(Tag, "li") || Equal(Tag, "dl") || Equal(Tag, "dt") || Equal(Tag, "dd") || Equal(Tag, "pre") || Equal(Tag, "blockquote") || Equal(Tag, "hr");
}

STATIC VOID LayoutNode(UINT16 Index, WEB_FLOW *Flow, UINT16 Link, BOOLEAN Pre, UINT32 Depth)
{
    BOOLEAN   HasBlockChildren = FALSE;
    WEB_NODE *Node = &Nodes[Index];
    BOOLEAN   IsBlock = Block(Node->Tag);
    WEB_FLOW  Inner;
    WEB_FLOW *Target = Flow;
    UINT32    FirstRun = RunCount;
    UINT32    Box = UINT32_MAX;

    if (Depth > 40 || Node->Style.Hidden)
    {
        return;
    }

    if (Equal(Node->Tag, "a") && Node->Href[0] != 0)
    {
        Link = Index;
    }

    Pre |= Equal(Node->Tag, "pre");

    if (Node->Tag[0] == 0 && Index != 0)
    {
        LayoutText(Node, Flow, Link, Pre);
        return;
    }

    if (Equal(Node->Tag, "br"))
    {
        if (Flow->X == Flow->Left)
        {
            Flow->Y += Node->Style.FontSize * Scale * 3 / 2;
        }
        else
        {
            BreakLine(Flow);
        }

        return;
    }

    if (IsBlock)
    {
        BreakLine(Flow);
        Flow->Y += Node->Style.Margin * Scale;
        Inner = *Flow;
        UINT32 Padding = Node->Style.Padding * Scale;
        UINT32 Available = Inner.Right > Inner.Left ? Inner.Right - Inner.Left : 0;

        if (Node->Style.MaxWidth != 0 && Node->Style.MaxWidth * Scale < Available)
        {
            UINT32 Extra = Available - Node->Style.MaxWidth * Scale;
            Inner.Left += Extra / 2;
            Inner.Right -= Extra / 2;
        }

        if (Padding * 2 + 80 * Scale < Inner.Right - Inner.Left)
        {
            Inner.Left += Padding;
            Inner.Right -= Padding;
        }

        Inner.X = Inner.Left;
        Inner.Y += Padding;
        Target = &Inner;

        if ((Node->Style.Mask & WEB_BACKGROUND) != 0 && BoxCount < WEB_NODE_LIMIT)
        {
            Box = BoxCount++;
            Boxes[Box].X = Inner.Left - Padding;
            Boxes[Box].Y = Flow->Y;
            Boxes[Box].Width = Inner.Right - Inner.Left + Padding * 2;
            Boxes[Box].Color = Node->Style.Background;
        }
    }

    for (UINT16 Child = Node->FirstChild; Child != 0; Child = Nodes[Child].Next)
    {
        HasBlockChildren |= Block(Nodes[Child].Tag) && !Nodes[Child].Style.Hidden;
        LayoutNode(Child, Target, Link, Pre, Depth + 1);
    }

    if (IsBlock)
    {
        BreakLine(Target);

        if (Node->Style.Center && !HasBlockChildren)
        {
            for (UINT32 Run = FirstRun; Run < RunCount;)
            {
                UINT32 End = Run + 1;
                UINT32 Right = Runs[Run].X + Runs[Run].Width;

                while (End < RunCount && Runs[End].Y == Runs[Run].Y)
                {
                    if (Runs[End].X + Runs[End].Width > Right)
                    {
                        Right = Runs[End].X + Runs[End].Width;
                    }

                    End++;
                }

                UINT32 Shift = Right < Inner.Right ? (Inner.Right - Right) / 2 : 0;

                for (; Run < End; Run++)
                {
                    Runs[Run].X += Shift;
                }
            }
        }

        Flow->Y = Inner.Y + Node->Style.Padding * Scale + Node->Style.Margin * Scale;
        Flow->X = Flow->Left;
        Flow->LineHeight = 0;
        Flow->Space = TRUE;

        if (Box != UINT32_MAX)
        {
            Boxes[Box].Height = Flow->Y - Boxes[Box].Y - Node->Style.Margin * Scale;
        }
    }
}

STATIC VOID Layout(VOID)
{
    UINT32 Width = VirtIOGPUGetFrameBufferWidth();
    UINT32 Height = VirtIOGPUGetFrameBufferHeight();
    Scale = Width >= 1800 ? 2 : 1;
    ViewX = 204 * Scale;
    ViewY = 120 * Scale;
    ViewWidth = Width > ViewX + 24 * Scale ? Width - ViewX - 24 * Scale : 100;
    ViewHeight = Height > ViewY + 44 * Scale ? Height - ViewY - 44 * Scale : 100;
    RunCount = 0;
    BoxCount = 0;
    PageBackground = 0xFFFFFF;
    ComputeStyles();

    if ((Nodes[BodyNode].Style.Mask & WEB_BACKGROUND) != 0)
    {
        PageBackground = Nodes[BodyNode].Style.Background;
    }

    WEB_FLOW Flow = {32 * Scale, ViewWidth - 32 * Scale, 32 * Scale, 24 * Scale, 0, FALSE};
    LayoutNode(0, &Flow, 0, FALSE, 0);
    BreakLine(&Flow);
    DocumentHeight = Flow.Y + 32 * Scale;

    if (Scroll > DocumentHeight || DocumentHeight <= ViewHeight)
    {
        Scroll = 0;
    }
}

STATIC VOID DrawLabel(CONST CHAR8 *Label, UINT32 X, UINT32 Y, UINT32 Size, UINT32 RGB, UINT32 MaxWidth)
{
    CHAR16 Wide[192];
    UINT32 Offset = 0;
    UINT32 Count = 0;
    UINT32 StringLength = Length(Label);

    while (Offset < StringLength && Count + 1 < 192)
    {
        UINT32 Codepoint = Decode(Label, StringLength, &Offset);
        Wide[Count++] = (CHAR16) Codepoint;
    }

    Wide[Count] = 0;

    while (Count != 0)
    {
        INT32 Left = 0, Top = 0, Right = 0, Bottom = 0;
        MeasureTrueTypeText(PRETENDARD, Wide, Size, &Left, &Top, &Right, &Bottom);

        if (Right <= (INT32) MaxWidth)
        {
            break;
        }

        Wide[--Count] = 0;
    }

    DrawTrueTypeText(PRETENDARD, Wide, X, Y, RGB, Size);
}

VOID WebRender(VOID)
{
    UINT32 Width = VirtIOGPUGetFrameBufferWidth();
    UINT32 Height = VirtIOGPUGetFrameBufferHeight();

    if (Width < 640 || Height < 400)
    {
        return;
    }

    FillScreen(0xE9EDF2);
    FillRect(0, 0, Width, 64 * Scale, 0x15252D);
    DrawLabel((CONST CHAR8 *) "LineOS", 24 * Scale, 40 * Scale, 25 * Scale, 0xF5F7F6, 130 * Scale);
    DrawLabel((CONST CHAR8 *) "web", 126 * Scale, 40 * Scale, 18 * Scale, 0x94B7A9, 80 * Scale);
    DrawLabel((CONST CHAR8 *) "HTTP READER", Width - 158 * Scale, 39 * Scale, 13 * Scale, 0xB9C9C3, 150 * Scale);
    FillRect(0, 64 * Scale, Width, 44 * Scale, 0xFFFFFF);
    DrawLabel((CONST CHAR8 *) "< Back", 20 * Scale, 93 * Scale, 15 * Scale, 0x364E60, 75 * Scale);
    DrawLabel((CONST CHAR8 *) "Reload", 107 * Scale, 93 * Scale, 15 * Scale, 0x364E60, 70 * Scale);
    FillRect(204 * Scale, 71 * Scale, Width - 228 * Scale, 30 * Scale, Editing ? 0xE0F0E8 : 0xF0F3F6);
    DrawLabel(Editing ? Address : CurrentURL, 216 * Scale, 92 * Scale, 14 * Scale, 0x304C42, Width - 255 * Scale);
    DrawLabel((CONST CHAR8 *) "EXPLORE", 24 * Scale, 150 * Scale, 12 * Scale, 0x728292, 160 * Scale);
    FillRect(14 * Scale, (Prefix(CurrentURL, "http://example.com") ? 216 : 169) * Scale, 174 * Scale, 42 * Scale, 0xD8E5DE);
    DrawLabel((CONST CHAR8 *) "The Web project", 26 * Scale, 195 * Scale, 16 * Scale, 0x1F5041, 155 * Scale);
    DrawLabel((CONST CHAR8 *) "Example Domain", 26 * Scale, 241 * Scale, 16 * Scale, 0x42596D, 155 * Scale);
    DrawLabel((CONST CHAR8 *) "CONTROLS", 24 * Scale, 309 * Scale, 12 * Scale, 0x728292, 160 * Scale);
    DrawLabel((CONST CHAR8 *) "Click a link to open", 24 * Scale, 342 * Scale, 13 * Scale, 0x5A6C7B, 166 * Scale);
    DrawLabel((CONST CHAR8 *) "Ctrl+L   Address", 24 * Scale, 372 * Scale, 13 * Scale, 0x5A6C7B, 166 * Scale);
    DrawLabel((CONST CHAR8 *) "Up / Down   Scroll", 24 * Scale, 400 * Scale, 13 * Scale, 0x5A6C7B, 166 * Scale);
    DrawLabel((CONST CHAR8 *) "PgUp / PgDn", 24 * Scale, 428 * Scale, 13 * Scale, 0x5A6C7B, 166 * Scale);
    FillRect(ViewX, ViewY, ViewWidth, ViewHeight, PageBackground);

    for (UINT32 Index = 0; Index < BoxCount; Index++)
    {
        WEB_BOX *Box = &Boxes[Index];
        INT64    Top = (INT64) Box->Y - Scroll;
        INT64    Bottom = Top + Box->Height;

        if (Bottom > 0 && Top < ViewHeight)
        {
            Top = Top < 0 ? 0 : Top;
            Bottom = Bottom > ViewHeight ? ViewHeight : Bottom;
            FillRect(ViewX + Box->X, ViewY + (UINT32) Top, Box->Width, (UINT32) (Bottom - Top), Box->Color);
        }
    }

    for (UINT32 Index = 0; Index < RunCount; Index++)
    {
        WEB_RUN *Run = &Runs[Index];
        INT64    Y = (INT64) Run->Y - Scroll;

        if (Y < 0 || Y + Run->Height > ViewHeight || Run->X + Run->Width > ViewWidth)
        {
            continue;
        }

        DrawTrueTypeText(PRETENDARD, Run->Text, ViewX + Run->X, ViewY + (UINT32) Y + Run->Size, Run->Color, Run->Size);

        if (Run->Underline)
        {
            FillRect(ViewX + Run->X, ViewY + (UINT32) Y + Run->Size + 2, Run->Width, Scale, Run->Color);
        }
    }

    if (DocumentHeight > ViewHeight)
    {
        UINT32 Thumb = (UINT32) ((UINT64) ViewHeight * ViewHeight / DocumentHeight);
        UINT32 Position = (UINT32) ((UINT64) Scroll * (ViewHeight - Thumb) / (DocumentHeight - ViewHeight));
        FillRect(ViewX + ViewWidth - 5 * Scale, ViewY + Position, 3 * Scale, Thumb, 0x9DAFA5);
    }

    FillRect(0, Height - 30 * Scale, Width, 30 * Scale, 0xDDE5EA);
    DrawTrueTypeText(PRETENDARD, Message, 24 * Scale, Height - 10 * Scale, 0x506575, 13 * Scale);
    DrawLabel(PageTitle, ViewX, Height - 10 * Scale, 13 * Scale, 0x506575, Width - ViewX - 20 * Scale);
    VirtIOGPUFlush();
}

STATIC VOID SetMessage(CONST CHAR16 *Value)
{
    UINT32 Index = 0;

    while (Index + 1 < sizeof(Message) / sizeof(Message[0]) && Value[Index] != 0)
    {
        Message[Index] = Value[Index];
        Index++;
    }

    Message[Index] = 0;
}

STATIC BOOLEAN ParseURL(CONST CHAR8 *URL, CHAR8 *Host, UINT16 *Port, CHAR8 *Path)
{
    UINT32 Offset = 7;
    UINT32 Count = 0;

    if (!Prefix(URL, "http://"))
    {
        SetMessage(L"Only http:// pages are supported");
        return FALSE;
    }

    while (URL[Offset] != 0 && URL[Offset] != '/' && URL[Offset] != ':' && URL[Offset] != '?' && URL[Offset] != '#')
    {
        if (Count >= 253)
        {
            return FALSE;
        }

        Host[Count++] = URL[Offset++];
    }

    Host[Count] = 0;
    *Port = 80;

    if (Count == 0)
    {
        return FALSE;
    }

    if (URL[Offset] == ':')
    {
        UINT32 Value = 0;
        UINT32 Digits = 0;
        Offset++;

        while (URL[Offset] >= '0' && URL[Offset] <= '9')
        {
            Value = Value * 10 + URL[Offset++] - '0';
            Digits++;

            if (Value > 65535 || Digits > 5)
            {
                return FALSE;
            }
        }

        if (Value == 0 || Digits == 0)
        {
            return FALSE;
        }

        *Port = (UINT16) Value;
    }

    Count = 0;

    if (URL[Offset] != '/')
    {
        Path[Count++] = '/';
    }

    while (URL[Offset] != 0 && URL[Offset] != '#')
    {
        if (Count + 1 >= WEB_URL_LIMIT)
        {
            return FALSE;
        }

        Path[Count++] = URL[Offset++];
    }

    Path[Count] = 0;
    return TRUE;
}

STATIC BOOLEAN ResolveURL(CONST CHAR8 *Href, CHAR8 *Output)
{
    UINT32 Used = 0;
    UINT32 BaseEnd = 7;
    UINT32 Start = 0;

    if (Href[0] == 0 || Length(Href) >= WEB_URL_LIMIT)
    {
        return FALSE;
    }

    if (Prefix(Href, "http://") || Prefix(Href, "https://"))
    {
        Copy(Output, Href, WEB_URL_LIMIT);
        return TRUE;
    }

    if (Href[0] == '/' && Href[1] == '/')
    {
        Copy(Output, (CONST CHAR8 *) "http:", WEB_URL_LIMIT);
        Used = 5;
    }
    else
    {
        for (UINT32 Index = 0; Href[Index] != 0 && Href[Index] != '/' && Href[Index] != '#'; Index++)
        {
            if (Href[Index] == ':')
            {
                return FALSE;
            }
        }

        while (CurrentURL[BaseEnd] != 0 && CurrentURL[BaseEnd] != '/')
        {
            BaseEnd++;
        }

        if (Href[0] != '/')
        {
            UINT32 End = BaseEnd;

            while (CurrentURL[End] != 0 && CurrentURL[End] != '?' && CurrentURL[End] != '#')
            {
                End++;
            }

            if (Href[0] == '#')
            {
                BaseEnd = End;
            }
            else
            {
                while (End > BaseEnd && CurrentURL[End - 1] != '/')
                {
                    End--;
                }

                BaseEnd = End;
            }
        }

        if (BaseEnd + Length(Href) + 1 >= WEB_URL_LIMIT)
        {
            return FALSE;
        }

        KMemCpy(Output, CurrentURL, BaseEnd);
        Used = BaseEnd;

        if (Href[0] != '/' && Href[0] != '#' && (Used == 0 || Output[Used - 1] != '/'))
        {
            Output[Used++] = '/';
        }
    }

    while (Href[Start] != 0 && Used + 1 < WEB_URL_LIMIT)
    {
        Output[Used++] = Href[Start++];
    }

    Output[Used] = 0;
    return Href[Start] == 0;
}

STATIC VOID LoadStylesheets(VOID)
{
    STATIC UINT8 CSS[32768];
    UINT32       External = 0;

    for (UINT32 Index = 1; Index < NodeCount; Index++)
    {
        if (Equal(Nodes[Index].Tag, "style"))
        {
            ParseCSS(Text + Nodes[Index].Text, Nodes[Index].Length);
        }
        else if (Nodes[Index].Stylesheet && External < 4)
        {
            CHAR8  URL[WEB_URL_LIMIT];
            CHAR8  Host[254];
            CHAR8  Path[WEB_URL_LIMIT];
            UINT16 Port;
            UINT16 CSSStatus;
            UINT32 Size;
            External++;

            if (ResolveURL(Nodes[Index].Href, URL) && ParseURL(URL, Host, &Port, Path) && NetworkHTTPGet(Host, Port, Path, CSS, sizeof(CSS) - 1, &Size, &CSSStatus, 6000) && CSSStatus == 200)
            {
                CSS[Size] = 0;
                ParseCSS(CSS, Size);
            }
        }
    }
}

BOOLEAN WebLoad(CONST CHAR8 *URL)
{
    CHAR8  Host[254];
    CHAR8  Path[WEB_URL_LIMIT];
    CHAR8  Target[WEB_URL_LIMIT];
    UINT16 Port;

    if (URL == NULL || Length(URL) >= WEB_URL_LIMIT)
    {
        return FALSE;
    }

    Copy(Target, URL, sizeof(Target));

    if (!ParseURL(Target, Host, &Port, Path))
    {
        WebRender();
        return FALSE;
    }

    SetMessage(L"Loading page...");
    WebRender();

    if (!NetworkHTTPGet(Host, Port, Path, HTML, sizeof(HTML) - 1, &Bytes, &Status, 15000))
    {
        SetMessage(NetworkGetLastError());
        WebRender();
        return FALSE;
    }

    HTML[Bytes] = 0;
    Copy(CurrentURL, Target, sizeof(CurrentURL));
    Copy(Address, Target, sizeof(Address));
    ParseHTML(Bytes);
    LoadStylesheets();
    Scroll = 0;
    Layout();
    SetMessage(Status == 200 ? (Truncated ? L"Page limit reached" : L"Page loaded") : L"Server returned an HTTP error or redirect");
    DebugWrite("[WEB] loaded ");
    DebugWrite((CONST char *) CurrentURL);
    DebugWrite(" bytes=");
    DebugWriteDec(Bytes);
    DebugWrite(" nodes=");
    DebugWriteDec(NodeCount);
    DebugWrite(" css=");
    DebugWriteDec(RuleCount);
    DebugWrite(" runs=");
    DebugWriteDec(RunCount);
    DebugWriteLine("");
    WebRender();
    return TRUE;
}

STATIC VOID Navigate(CONST CHAR8 *URL)
{
    CHAR8 Previous[WEB_URL_LIMIT];
    Copy(Previous, CurrentURL, sizeof(Previous));
    Editing = FALSE;

    if (WebLoad(URL) && Previous[0] != 0 && !Equal(CurrentURL, (CONST char *) Previous))
    {
        if (HistoryCount == 16)
        {
            for (UINT32 Index = 1; Index < 16; Index++)
            {
                Copy(History[Index - 1], History[Index], sizeof(History[0]));
            }

            HistoryCount--;
        }

        Copy(History[HistoryCount++], Previous, sizeof(History[0]));
    }
}

STATIC VOID GoBack(VOID)
{
    if (HistoryCount != 0)
    {
        CHAR8 Target[WEB_URL_LIMIT];
        Copy(Target, History[HistoryCount - 1], sizeof(Target));

        if (WebLoad(Target))
        {
            HistoryCount--;
        }
    }
}

STATIC VOID ScrollBy(INT32 Delta)
{
    INT64  Next = (INT64) Scroll + Delta;
    UINT32 Max = DocumentHeight > ViewHeight ? DocumentHeight - ViewHeight : 0;
    Scroll = Next < 0 ? 0 : (Next > Max ? Max : (UINT32) Next);
    WebRender();
}

STATIC VOID Click(UINT32 X, UINT32 Y)
{
    if (Y >= 71 * Scale && Y < 102 * Scale)
    {
        if (X < 94 * Scale)
        {
            GoBack();
        }
        else if (X < 184 * Scale)
        {
            WebLoad(CurrentURL);
        }
        else if (X >= ViewX)
        {
            Editing = TRUE;
            Copy(Address, CurrentURL, sizeof(Address));
            WebRender();
        }

        return;
    }

    if (X < ViewX)
    {
        if (Y >= 169 * Scale && Y < 212 * Scale)
        {
            Navigate((CONST CHAR8 *) WEB_HOME);
        }
        else if (Y >= 216 * Scale && Y < 258 * Scale)
        {
            Navigate((CONST CHAR8 *) "http://example.com/");
        }

        return;
    }

    if (X < ViewX + ViewWidth && Y >= ViewY && Y < ViewY + ViewHeight)
    {
        UINT32 LocalY = Y - ViewY + Scroll;
        UINT32 LocalX = X - ViewX;

        for (UINT32 Index = 0; Index < RunCount; Index++)
        {
            WEB_RUN *Run = &Runs[Index];

            if (Run->Link != 0 && LocalX >= Run->X && LocalX < Run->X + Run->Width && LocalY >= Run->Y && LocalY < Run->Y + Run->Height)
            {
                CHAR8 Target[WEB_URL_LIMIT];

                if (Nodes[Run->Link].Href[0] == '#')
                {
                    Scroll = 0;
                    WebRender();
                }
                else if (ResolveURL(Nodes[Run->Link].Href, Target))
                {
                    Navigate(Target);
                }
                else
                {
                    SetMessage(L"This link type is not supported");
                    WebRender();
                }

                return;
            }
        }
    }
}

STATIC CHAR8 KeyCharacter(UINT16 Code, UINT8 Modifiers)
{
    STATIC CONST char Normal[] = "\0\0"
                                 "1234567890-=\0\0qwertyuiop[]\0\0asdfghjkl;'`\0\\zxcvbnm,./";
    STATIC CONST char Shifted[] = "\0\0"
                                  "!@#$%^&*()_+\0\0QWERTYUIOP{}\0\0ASDFGHJKL:\"~\0|ZXCVBNM<>?";

    if (Code == 57)
    {
        return ' ';
    }

    if (Code >= sizeof(Normal) - 1)
    {
        return 0;
    }

    CHAR8 Ch = (Modifiers & 1) != 0 ? Shifted[Code] : Normal[Code];

    if ((Modifiers & 2) != 0 && Ch >= 'a' && Ch <= 'z')
    {
        Ch -= 'a' - 'A';
    }
    else if ((Modifiers & 2) != 0 && Ch >= 'A' && Ch <= 'Z')
    {
        Ch += 'a' - 'A';
    }

    return Ch;
}

STATIC VOID Keyboard(KEYBOARD_STATUS *Event)
{
    if (!Event->Pressed)
    {
        return;
    }

    if ((Event->Modifiers & 4) != 0 && Event->KeyCode == 38)
    {
        Editing = TRUE;
        Address[0] = 0;
        WebRender();
        return;
    }

    if (Editing)
    {
        UINT32 Count = Length(Address);

        if (Event->KeyCode == 28)
        {
            if (!Prefix(Address, "http://") && !Prefix(Address, "https://"))
            {
                CHAR8 Target[WEB_URL_LIMIT];
                Copy(Target, (CONST CHAR8 *) "http://", sizeof(Target));
                Copy(Target + 7, Address, sizeof(Target) - 7);
                Navigate(Target);
            }
            else
            {
                Navigate(Address);
            }
        }
        else if (Event->KeyCode == 1)
        {
            Editing = FALSE;
            WebRender();
        }
        else if (Event->KeyCode == 14 && Count != 0)
        {
            Address[Count - 1] = 0;
            WebRender();
        }
        else
        {
            CHAR8 Ch = KeyCharacter(Event->KeyCode, Event->Modifiers);

            if (Ch != 0 && Count + 1 < sizeof(Address))
            {
                Address[Count] = Ch;
                Address[Count + 1] = 0;
                WebRender();
            }
        }

        return;
    }

    if (Event->KeyCode == 103)
        ScrollBy(-(INT32) (60 * Scale));
    else if (Event->KeyCode == 108)
        ScrollBy(60 * Scale);
    else if (Event->KeyCode == 104)
        ScrollBy(-(INT32) ViewHeight * 3 / 4);
    else if (Event->KeyCode == 109)
        ScrollBy(ViewHeight * 3 / 4);
    else if (Event->KeyCode == 102)
        ScrollBy(-(INT32) DocumentHeight);
    else if (Event->KeyCode == 107)
        ScrollBy(DocumentHeight);
    else if (Event->KeyCode == 63)
        WebLoad(CurrentURL);
    else if (Event->KeyCode == 14)
        GoBack();
}

UINT32 WebGetNodeCount(VOID)
{
    return NodeCount;
}

UINT32 WebGetRuleCount(VOID)
{
    return RuleCount;
}

VOID WebTask(VOID)
{
    MOUSE_STATUS Previous = GetMouseStatus();
    Copy(CurrentURL, (CONST CHAR8 *) WEB_HOME, sizeof(CurrentURL));
    Layout();
    WebLoad(CurrentURL);

    for (;;)
    {
        KEYBOARD_STATUS Event;
        MOUSE_STATUS    Mouse = GetMouseStatus();

        while (InputGetKeyboardEvent(&Event))
        {
            Keyboard(&Event);
        }

        if (Mouse.LeftButton && !Previous.LeftButton)
        {
            Click(Mouse.X, Mouse.Y);
        }

        if (Mouse.BackButton && !Previous.BackButton)
        {
            GoBack();
        }

        if (Mouse.Wheel != Previous.Wheel)
        {
            INT64 Delta = (Mouse.Wheel < 0) == (Previous.Wheel < 0) ? (INT64) Mouse.Wheel - Previous.Wheel : Mouse.Wheel;
            Delta = Delta > 3 ? 3 : (Delta < -3 ? -3 : Delta);
            ScrollBy((INT32) (-Delta * 60 * Scale));
        }

        Previous = Mouse;
        TaskSleepMls(12);
    }
}
