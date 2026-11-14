// UserPrograms/Shell/Commands/testopengl/testopengl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glext.h>

#include <Display.h>
#include <stdio.h>
#include <string.h>

#define TEST_WIDTH 320U
#define TEST_HEIGHT 240U

STATIC UINT32 Pixels[TEST_WIDTH * TEST_HEIGHT];

STATIC BOOLEAN TestTriangle(VOID)
{
    STATIC CONST GLchar* VertexShaderSource =
        "#version 120\n"
        "attribute vec2 Position;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vec4(Position, 0.0, 1.0);\n"
        "}\n";

    STATIC CONST GLchar* FragmentShaderSource =
        "#version 120\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
        "}\n";

    CONST float Positions[] =
    {
        -0.6f, -0.4f,
        0.6f, -0.4f,
        0.0f, 0.8f
    };

    GLuint VertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (VertexShader == 0 || FragmentShader == 0)
    {
        puts("testopengl: shader creation failed");
        return FALSE;
    }

    glShaderSource(VertexShader, 1, &VertexShaderSource, NULL);
    glCompileShader(VertexShader);

    glShaderSource(FragmentShader, 1, &FragmentShaderSource, NULL);
    glCompileShader(FragmentShader);

    GLint Status = GL_FALSE;

    glGetShaderiv(VertexShader, GL_COMPILE_STATUS, &Status);

    if (Status != GL_TRUE)
    {
        puts("testopengl: vertex shader compilation failed");
        return FALSE;
    }

    glGetShaderiv(FragmentShader, GL_COMPILE_STATUS, &Status);

    if (Status != GL_TRUE)
    {
        puts("testopengl: fragment shader compilation failed");
        return FALSE;
    }

    GLuint Program = glCreateProgram();

    if (Program == 0)
    {
        puts("testopengl: program creation failed");
        return FALSE;
    }

    glAttachShader(Program, VertexShader);
    glAttachShader(Program, FragmentShader);
    glBindAttribLocation(Program, 0, "Position");
    glLinkProgram(Program);

    glGetProgramiv(Program, GL_LINK_STATUS, &Status);

    if (Status != GL_TRUE)
    {
        puts("testopengl: program linking failed");
        return FALSE;
    }

    GLuint Buffer = 0;

    glGenBuffers(1, &Buffer);
    glBindBuffer(GL_ARRAY_BUFFER, Buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Positions), Positions, GL_STATIC_DRAW);

    glViewport(0, 0, TEST_WIDTH, TEST_HEIGHT);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(Program);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glFinish();

    if (glGetError() != GL_NO_ERROR)
    {
        puts("testopengl: rendering failed");
        return FALSE;
    }

    glReadPixels(0, 0, TEST_WIDTH, TEST_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, Pixels);

    if (glGetError() != GL_NO_ERROR)
    {
        puts("testopengl: readback failed");
        return FALSE;
    }

    if ((Pixels[(TEST_HEIGHT / 2) * TEST_WIDTH + TEST_WIDTH / 2] & 0xFFFFFFU) != 0xFFFFFFU)
    {
        puts("testopengl: triangle pixel verification failed");
        return FALSE;
    }

    DISPLAY_INFO DisplayInfo;

    if (DisplayGetInfo(&DisplayInfo) < 0 ||
        DisplayInfo.Width < TEST_WIDTH ||
        DisplayInfo.Height < TEST_HEIGHT ||
        DisplayInfo.Format != DISPLAY_FORMAT_BGRA8888)
    {
        puts("testopengl: display unavailable");
        return FALSE;
    }

    UINT32 DisplayPixels[TEST_WIDTH * TEST_HEIGHT];

    for (UINT32 Y = 0; Y < TEST_HEIGHT; Y++)
    {
        memcpy(&DisplayPixels[Y * TEST_WIDTH],
               &Pixels[(TEST_HEIGHT - Y - 1) * TEST_WIDTH],
               TEST_WIDTH * sizeof(UINT32));
    }

    for (UINT32 Index = 0; Index < TEST_WIDTH * TEST_HEIGHT; Index++)
    {
        DisplayPixels[Index] |= 0xFF000000U;
    }

    if (DisplayPresentRect(
        (DisplayInfo.Width - TEST_WIDTH) / 2,
        (DisplayInfo.Height - TEST_HEIGHT) / 2,
        TEST_WIDTH,
        TEST_HEIGHT,
        DisplayPixels) < 0)
    {
        perror("testopengl: present");
        return FALSE;
    }

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &Buffer);
    glUseProgram(0);
    glDeleteProgram(Program);
    glDeleteShader(VertexShader);
    glDeleteShader(FragmentShader);

    return TRUE;
}

int main(VOID)
{
    if (!TestTriangle())
    {
        return 1;
    }

    puts("testopengl: Mesa OpenGL triangle PASS");

    return 0;
}
