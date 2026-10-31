// UserPrograms/System/lwm/Source/LwmRender.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Lwm.h>
#include <string.h>
#include <sys/ioctl.h>
#include <libintl.h>

#define LWM_GLYPH_CACHE_COUNT 256U
#define LWM_GLYPH_BITMAP_SIZE 4096U

typedef struct
{
    TRUE_TYPE_FONT *Font;
    UINT32          Codepoint;
    UINT32          Size;
    INT32           Width;
    INT32           Height;
    INT32           Left;
    INT32           Top;
    UINT8           Bitmap[LWM_GLYPH_BITMAP_SIZE];
} LWM_GLYPH;

STATIC LWM_GLYPH GlyphCache[LWM_GLYPH_CACHE_COUNT];

STATIC UINT32 LwmGlyph(LWM *Manager, TRUE_TYPE_FONT *Font, UINT32 Codepoint, UINT32 X, UINT32 Baseline, UINT32 Size, UINT32 Color, UINT32 Advance, UINT32 Right)
{
    LWM_GLYPH *Glyph = &GlyphCache[(Codepoint * 2654435761U + Size * 31U) % LWM_GLYPH_CACHE_COUNT];

    if (Glyph->Font != Font || Glyph->Codepoint != Codepoint || Glyph->Size != Size)
    {
        FLOAT32 Scale = ScaleForPixelHeight(&Font->Info, Size);
        INT32   Width = 0;
        INT32   Height = 0;
        INT32   Left = 0;
        INT32   Top = 0;

        Manager->FontUsed = 0;

        UINT8 *Bitmap = GetCodepointBitmap(&Font->Info, Scale, Scale, (INT32) Codepoint, &Width, &Height, &Left, &Top);

        if (Width < 0 || Height < 0 || (UINT64) Width * Height > LWM_GLYPH_BITMAP_SIZE || (Bitmap == NULL && Width != 0 && Height != 0))
        {
            Manager->FontUsed = 0;

            return DrawTrueTypeCodepoint(Font, Codepoint, X, Baseline, Color, Size);
        }

        Glyph->Font = Font;
        Glyph->Codepoint = Codepoint;
        Glyph->Size = Size;
        Glyph->Width = Width;
        Glyph->Height = Height;
        Glyph->Left = Left;
        Glyph->Top = Top;

        if (Bitmap != NULL)
        {
            memcpy(Glyph->Bitmap, Bitmap, (UINTN) Width * Height);
        }
    }

    for (INT32 Row = 0; Row < Glyph->Height; Row++)
    {
        INT64 Y = (INT64) Baseline + Glyph->Top + Row;

        if (Y < 0 || Y >= Manager->Surface.FrameBufferHeight)
        {
            continue;
        }

        for (INT32 Column = 0; Column < Glyph->Width; Column++)
        {
            INT64  PixelX = (INT64) X + Glyph->Left + Column;
            UINT32 Alpha = Glyph->Bitmap[(UINTN) Row * Glyph->Width + Column];

            if (Alpha == 0 || PixelX < 0 || PixelX >= Right || PixelX >= Manager->Surface.FrameBufferWidth)
            {
                continue;
            }

            UINT32 *Pixel = Manager->Surface.FrameBuffer + (UINTN) Y * Manager->Surface.FrameBufferWidth + PixelX;
            UINT32  Background = *Pixel;
            UINT32  Inverse = 255 - Alpha;
            UINT32  Red = (((Color >> 16) & 255) * Alpha + ((Background >> 16) & 255) * Inverse) / 255;
            UINT32  Green = (((Color >> 8) & 255) * Alpha + ((Background >> 8) & 255) * Inverse) / 255;
            UINT32  Blue = ((Color & 255) * Alpha + (Background & 255) * Inverse) / 255;

            *Pixel = 0xFF000000U | (Red << 16) | (Green << 8) | Blue;
        }
    }

    return X + Advance;
}

VOID *LwmAllocate(UINTN Size, VOID *Context)
{
    LWM *Manager = Context;

    if (Size > LWM_FONT_WORK_SIZE - 15U)
    {
        return NULL;
    }

    UINTN Aligned = (Size + 15U) & ~(UINTN) 15U;

    if (Aligned > LWM_FONT_WORK_SIZE - Manager->FontUsed)
    {
        return NULL;
    }

    VOID *Memory = Manager->FontWork + Manager->FontUsed;

    Manager->FontUsed += Aligned;

    return Memory;
}

VOID LwmFree(VOID *Pointer, VOID *Context)
{
    (VOID) Pointer;
    (VOID) Context;
}

VOID LwmFill(LWM *Manager, LWM_RECT Rect, UINT32 Color)
{
    INT64 Left = Rect.X < 0 ? 0 : Rect.X;
    INT64 Top = Rect.Y < 0 ? 0 : Rect.Y;
    INT64 Right = (INT64) Rect.X + Rect.Width;
    INT64 Bottom = (INT64) Rect.Y + Rect.Height;

    if (Right > Manager->Surface.FrameBufferWidth)
    {
        Right = Manager->Surface.FrameBufferWidth;
    }

    if (Bottom > Manager->Surface.FrameBufferHeight)
    {
        Bottom = Manager->Surface.FrameBufferHeight;
    }

    for (INT64 Y = Top; Y < Bottom; Y++)
    {
        UINT32 *Pixel = Manager->Surface.FrameBuffer + (UINTN) Y * Manager->Surface.FrameBufferWidth;

        for (INT64 X = Left; X < Right; X++)
        {
            Pixel[X] = Color;
        }
    }
}

STATIC UINT32 LwmCodepoint(CONST char **Text)
{
    UINT8  First = (UINT8) * (*Text)++;
    UINT32 Value = First;
    UINT32 Count = 0;

    if (First >= 0xC2 && First <= 0xDF)
    {
        Count = 1;
        Value = First & 31U;
    }
    else if (First >= 0xE0 && First <= 0xEF)
    {
        Count = 2;
        Value = First & 15U;
    }
    else if (First >= 0xF0 && First <= 0xF4)
    {
        Count = 3;
        Value = First & 7U;
    }

    while (Count != 0)
    {
        UINT8 Byte = (UINT8) * *Text;

        if ((Byte & 0xC0) != 0x80)
        {
            return 0xFFFD;
        }

        Value = (Value << 6) | (Byte & 63U);
        (*Text)++;
        Count--;
    }

    return Value;
}

VOID LwmText(LWM *Manager, CONST char *Text, UINT32 X, UINT32 Baseline, UINT32 Size, UINT32 Color, UINT32 Right)
{
    while (*Text != 0)
    {
        UINT32          Codepoint = LwmCodepoint(&Text);
        TRUE_TYPE_FONT *Font = FindGlyphIndex(&Manager->Font.Info, (INT32) Codepoint) != 0 ? &Manager->Font : &Manager->Fallback;
        FLOAT32         Scale = ScaleForPixelHeight(&Font->Info, Size);
        INT32           Advance;
        INT32           Bearing;

        GetCodepointHMetrics(&Font->Info, (INT32) Codepoint, &Advance, &Bearing);

        UINT32 Pixels = (UINT32) TTFFloor(Advance * Scale + 0.5f);

        if ((UINT64) X + Pixels > Right)
        {
            break;
        }

        Manager->FontUsed = 0;
        X = LwmGlyph(Manager, Font, Codepoint, X, Baseline, Size, Color, Pixels, Right);
    }
}

VOID LwmDesktop(LWM *Manager)
{
    Manager->Surface = (TRUE_TYPE_SURFACE){Manager->Desktop, Manager->Width, Manager->Height};

    LwmFill(Manager, (LWM_RECT){0, 0, Manager->Width, Manager->Height}, 0x00151922U);
    Manager->Surface = (TRUE_TYPE_SURFACE){Manager->Frame, Manager->Width, Manager->Height};
}

VOID LwmDamage(LWM *Manager, LWM_RECT Rect)
{
    INT64 Top = Rect.Y < 0 ? 0 : Rect.Y;
    INT64 Bottom = (INT64) Rect.Y + Rect.Height;
    INT64 Left = Rect.X < 0 ? 0 : Rect.X;
    INT64 Right = (INT64) Rect.X + Rect.Width;

    if (Bottom > Manager->Height)
    {
        Bottom = Manager->Height;
    }

    if (Right > Manager->Width)
    {
        Right = Manager->Width;
    }

    if (Top >= Bottom || Left >= Right)
    {
        return;
    }

    if (Manager->DamageRight == 0 || Left < Manager->DamageLeft)
    {
        Manager->DamageLeft = (UINT32) Left;
    }

    if (Right > Manager->DamageRight)
    {
        Manager->DamageRight = (UINT32) Right;
    }

    for (INT64 Row = Top; Row < Bottom; Row++)
    {
        Manager->DamageRows[Row] = TRUE;
    }

    Manager->Dirty = TRUE;
}

UINT32 LwmRoundInset(UINT32 Row, UINT32 Height, UINT32 Radius)
{
    if (Radius == 0)
    {
        return 0;
    }

    UINT32 Edge = Row < Height / 2 ? Row : Height - Row - 1;

    if (Edge >= Radius)
    {
        return 0;
    }

    INT32  Y = (INT32) (Radius * 2 - Edge * 2 - 1);
    UINT32 Inset = 0;

    while (Inset < Radius)
    {
        INT32 X = (INT32) (Radius * 2 - Inset * 2 - 1);

        if (X * X + Y * Y <= (INT32) (Radius * Radius * 4))
        {
            break;
        }

        Inset++;
    }

    return Inset;
}

STATIC UINT32 LwmRoundCoverage(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Radius)
{
    if (Radius == 0 || (X >= Radius && X < Width - Radius) || (Y >= Radius && Y < Height - Radius))
    {
        return 255;
    }

    INT32  CenterX = (INT32) (X < Radius ? Radius : Width - Radius) * 8;
    INT32  CenterY = (INT32) (Y < Radius ? Radius : Height - Radius) * 8;
    INT32  Limit = (INT32) (Radius * Radius * 64);
    UINT32 Samples = 0;

    for (UINT32 Row = 0; Row < 4; Row++)
    {
        INT32 DY = (INT32) (Y * 8 + Row * 2 + 1) - CenterY;

        for (UINT32 Column = 0; Column < 4; Column++)
        {
            INT32 DX = (INT32) (X * 8 + Column * 2 + 1) - CenterX;

            Samples += DX * DX + DY * DY <= Limit ? 1U : 0U;
        }
    }

    return Samples * 255 / 16;
}

STATIC UINT32 LwmBlend(UINT32 Background, UINT32 Foreground, UINT32 Alpha)
{
    UINT32 Inverse = 255 - Alpha;
    UINT32 Red = (((Foreground >> 16) & 255) * Alpha + ((Background >> 16) & 255) * Inverse) / 255;
    UINT32 Green = (((Foreground >> 8) & 255) * Alpha + ((Background >> 8) & 255) * Inverse) / 255;
    UINT32 Blue = ((Foreground & 255) * Alpha + (Background & 255) * Inverse) / 255;

    return 0xFF000000U | (Red << 16) | (Green << 8) | Blue;
}

STATIC VOID LwmShadowPixel(LWM *Manager, INT32 X, INT32 Y, UINT32 Alpha)
{
    if (X < 0 || X >= (INT32) Manager->Width)
    {
        return;
    }

    UINT32 *Pixel = Manager->Frame + (UINTN) Y * Manager->Width + X;
    UINT32  Color = *Pixel;
    UINT32  Red = ((Color >> 16) & 255) * (255 - Alpha) / 255;
    UINT32  Green = ((Color >> 8) & 255) * (255 - Alpha) / 255;
    UINT32  Blue = (Color & 255) * (255 - Alpha) / 255;

    *Pixel = 0xFF000000U | (Red << 16) | (Green << 8) | Blue;
}

STATIC VOID LwmShadow(LWM *Manager, CONST LWM_WINDOW *Window)
{
    CONST LWM_RECT *Rect = &Window->Rect;

    for (INT32 Ring = 12; Ring > 0; Ring--)
    {
        INT32  Left = Rect->X - Ring;
        INT32  Right = Rect->X + (INT32) Rect->Width + Ring;
        INT32  Top = Rect->Y - Ring + 4;
        INT32  Bottom = Rect->Y + (INT32) Rect->Height + Ring + 4;
        UINT32 Alpha = (UINT32) (13 - Ring) * 2;

        for (INT32 Y = Top; Y < Bottom; Y++)
        {
            if (Y < 0 || Y >= (INT32) (Manager->Height - LWM_DOCK_HEIGHT) || !Manager->DamageRows[Y])
            {
                continue;
            }

            UINT32 Radius = Window->Maximized ? 0 : 12U + (UINT32) Ring;
            UINT32 Inset = LwmRoundInset((UINT32) (Y - Top), (UINT32) (Bottom - Top), Radius);
            INT32  Begin = Left + (INT32) Inset;
            INT32  End = Right - (INT32) Inset;

            if (Y == Top || Y == Bottom - 1)
            {
                for (INT32 X = Begin; X < End; X++)
                {
                    LwmShadowPixel(Manager, X, Y, Alpha * LwmRoundCoverage((UINT32) (X - Left), (UINT32) (Y - Top), (UINT32) (Right - Left), (UINT32) (Bottom - Top), Radius) / 255);
                }
            }
            else
            {
                LwmShadowPixel(Manager, Begin, Y, Alpha * LwmRoundCoverage((UINT32) (Begin - Left), (UINT32) (Y - Top), (UINT32) (Right - Left), (UINT32) (Bottom - Top), Radius) / 255);
                LwmShadowPixel(Manager, End - 1, Y, Alpha * LwmRoundCoverage((UINT32) (End - 1 - Left), (UINT32) (Y - Top), (UINT32) (Right - Left), (UINT32) (Bottom - Top), Radius) / 255);
            }
        }
    }
}

STATIC LWM_RECT LwmShadowRect(LWM_RECT Rect)
{
    return (LWM_RECT){Rect.X - 12, Rect.Y - 12, Rect.Width + 24, Rect.Height + 28};
}

INT32 LwmCompose(LWM *Manager)
{
    if (!Manager->InitialShell)
    {
        return 0;
    }

    if (!Manager->FrameReady)
    {
        LwmDamage(Manager, (LWM_RECT){0, 0, Manager->Width, Manager->Height});
    }

    INT32  Focused = -1;
    UINT64 DockState = Manager->WindowCount * 257U + Manager->ShellFocused;

    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index];

        DockState = DockState * 131U + Window->Endpoint + Window->Minimized;

        if (!Manager->ShellFocused && !Window->Minimized)
        {
            Focused = (INT32) Index;
        }
    }

    if (!Manager->FrameReady || DockState != Manager->DockState)
    {
        LwmDamage(Manager, (LWM_RECT){0, (INT32) Manager->Height - LWM_DOCK_HEIGHT, Manager->Width, LWM_DOCK_HEIGHT});
    }

    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index];
        BOOLEAN     Visible = !Window->Minimized && (Window->Endpoint == 0 || Window->HasFrame);
        BOOLEAN     Moved = !Window->PaintTracked || memcmp(&Window->Rect, &Window->PaintedRect, sizeof(Window->Rect)) != 0;

        if (Window->PaintedVisible && (Moved || !Visible))
        {
            LwmDamage(Manager, LwmShadowRect(Window->PaintedRect));
        }

        if (Visible && (Window->Dirty || Moved || !Window->PaintedVisible))
        {
            LwmDamage(Manager, LwmShadowRect(Window->Rect));
        }

        if (Visible && Window->Dirty)
        {
            LwmPaintWindow(Manager, Window, (INT32) Index == Focused);
        }

        Window->PaintedRect = Window->Rect;
        Window->PaintedVisible = Visible;
        Window->PaintTracked = TRUE;
    }

    LWM_WINDOW *Shell = &Manager->Shell;
    UINTN       RowBytes = (UINTN) Manager->Width * sizeof(UINT32);

    for (UINT32 Row = 0; Row < Manager->Height; Row++)
    {
        if (!Manager->DamageRows[Row])
        {
            continue;
        }

        memcpy(Manager->Frame + (UINTN) Row * Manager->Width, Manager->Desktop + (UINTN) Row * Manager->Width, RowBytes);

        if (Shell->HasFrame && Row < Shell->ClientHeight)
        {
            UINT32 Width = Shell->ClientWidth < Manager->Width ? Shell->ClientWidth : Manager->Width;

            memcpy(Manager->Frame + (UINTN) Row * Manager->Width, Shell->ClientPixels + (UINTN) Row * Shell->ClientWidth, (UINTN) Width * sizeof(UINT32));
        }
    }

    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index];

        if (!Window->PaintedVisible)
        {
            continue;
        }

        LwmShadow(Manager, Window);

        for (UINT32 Row = 0; Row < Window->Rect.Height; Row++)
        {
            INT64 Y = (INT64) Window->Rect.Y + Row;

            if (Y < 0 || Y >= Manager->Height - LWM_DOCK_HEIGHT || !Manager->DamageRows[Y])
            {
                continue;
            }

            UINT32 Radius = Window->Maximized ? 0 : 12;
            UINT32 Inset = LwmRoundInset(Row, Window->Rect.Height, Radius);

            if (Inset != 0)
            {
                Inset--;
            }

            INT64 Left = Window->Rect.X + Inset;
            INT64 Right = (INT64) Window->Rect.X + Window->Rect.Width - Inset;

            if (Left < 0)
            {
                Left = 0;
            }

            if (Right > Manager->Width)
            {
                Right = Manager->Width;
            }

            if (Left < Right)
            {
                UINT32 *Source = Window->Pixels + (UINTN) Row * Window->Stride + (Left - Window->Rect.X);

                UINT32 *Destination = Manager->Frame + (UINTN) Y * Manager->Width + Left;
                BOOLEAN Corner = Radius != 0 && (Row < Radius || Row >= Window->Rect.Height - Radius);
                UINT32  Background[4];

                if (Corner)
                {
                    Background[0] = Destination[0];
                    Background[1] = Destination[1];
                    Background[2] = Destination[Right - Left - 2];
                    Background[3] = Destination[Right - Left - 1];
                }

                memcpy(Destination, Source, (UINTN) (Right - Left) * sizeof(UINT32));

                if (Corner)
                {
                    UINTN Positions[4] = {0, 1, (UINTN) (Right - Left - 2), (UINTN) (Right - Left - 1)};

                    for (UINT32 Index = 0; Index < 4; Index++)
                    {
                        UINTN  Pixel = Positions[Index];
                        UINT32 LocalX = (UINT32) (Left - Window->Rect.X) + (UINT32) Pixel;
                        UINT32 Alpha = LwmRoundCoverage(LocalX, Row, Window->Rect.Width, Window->Rect.Height, Radius);

                        Destination[Pixel] = LwmBlend(Background[Index], Source[Pixel], Alpha);
                    }
                }
            }
        }
    }

    BOOLEAN DockDirty = FALSE;

    for (UINT32 Row = Manager->Height - LWM_DOCK_HEIGHT; Row < Manager->Height; Row++)
    {
        DockDirty = DockDirty || Manager->DamageRows[Row];
    }

    if (DockDirty)
    {
        LwmDock(Manager);
    }

    UINT32 First = Manager->Height;
    UINT32 Last = 0;

    for (UINT32 Row = 0; Row < Manager->Height; Row++)
    {
        if (Manager->DamageRows[Row] && memcmp(Manager->Mapped + (UINTN) Row * Manager->Width, Manager->Frame + (UINTN) Row * Manager->Width, RowBytes) != 0)
        {
            if (First == Manager->Height)
            {
                First = Row;
            }

            Last = Row + 1;
        }
    }

    if (First < Last)
    {
        UINT32 Width = Manager->DamageRight - Manager->DamageLeft;

        for (UINT32 Row = First; Row < Last; Row++)
        {
            memcpy(Manager->PresentBuffer + (UINTN) (Row - First) * Width, Manager->Frame + (UINTN) Row * Manager->Width + Manager->DamageLeft, (UINTN) Width * sizeof(UINT32));
        }

        LOS_FB_RECT Rect = {Manager->DamageLeft, First, Width, Last - First, (UINT64) Manager->PresentBuffer};

        if (ioctl(Manager->FB, LOS_FBIO_PRESENT_RECT, &Rect) < 0)
        {
            return -1;
        }
    }

    memset(Manager->DamageRows, 0, sizeof(Manager->DamageRows));
    Manager->DamageLeft = 0;
    Manager->DamageRight = 0;
    Manager->DockState = DockState;
    Manager->FrameReady = TRUE;
    Manager->Dirty = FALSE;

    return 0;
}
