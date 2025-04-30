#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MATH_3D_IMPLEMENTATION
#include <math_3d.h>

#include "trace.h"

#define NORMAL "\x1b[m"
#define RED    "\x1b[31m"
#define YELLOW "\x1b[33m"
#define CYAN   "\x1b[36m"

#define LOG_COLOUR(FP, COLOUR, FMT, ...) fprintf (FP, COLOUR FMT"\n"NORMAL, ##__VA_ARGS__)
#define DUMP(FMT, ...) LOG_COLOUR (stdout, CYAN, FMT, ##__VA_ARGS__)
#define LOG_ERROR(FMT, ...) LOG_COLOUR (stderr, YELLOW, FMT, ##__VA_ARGS__)
#define FATAL(FMT, ...) LOG_COLOUR (stderr, RED, FMT, ##__VA_ARGS__)

#define LEN(arr) (sizeof ((arr)) / sizeof ((arr)[0]))
#define ASSERT(expr) \
    if (!(expr)) { FATAL ("Assertion failed in %s at %s():%d\n %s", __FILE__, __func__, __LINE__, #expr); *(int *) 0 = 0; }
#define GLCALL(expr) expr; ASSERT (gl_check_error (#expr, __FILE__, __LINE__))
#define RADIANS(__DEGREES) (__DEGREES * (M_PI / 180.0))

#define WINDOW_WIDTH_PX     800
#define WINDOW_HEIGHT_PX    600
#define FRAME_TIME_MS       (1000.0f / 30.0f)

enum state
{
    STATE_INVALID = 0,
    STATE_RENDER_SQUARE,
    STATE_RENDER_TRIANGLE,
    STATE_RENDER_TEXTURE,
    STATE_RENDER_MAX
};

struct render_target
{
    unsigned int vao;
    unsigned int shader_id;
    unsigned int shader_ids[10];
    unsigned int texture_ids[10];
};

struct context
{
    SDL_Window *window;
    SDL_GLContext *gl;

    enum state state;
    int variation;
    bool rotate;
    struct render_target render_targets[STATE_RENDER_MAX];

    bool draw_wireframes;
};


/* Globals */
static bool g__running;


static bool
gl_check_error (char *func, char *file, int line)
{
    GLenum error;
    bool ok = true;

    while ((error = glGetError ()) != GL_NO_ERROR)
    {
        printf ("OpenGL %s:%d> Error %x:\n %s\n", file, line, error, func);

        ok = false;
    }

    return ok;
}

static char *
_gl_source (GLenum source)
{
    switch (source)
    {
        case GL_DEBUG_SOURCE_API: return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "Window System";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader Compiler";
        case GL_DEBUG_SOURCE_THIRD_PARTY: return "Third Party";
        case GL_DEBUG_SOURCE_APPLICATION: return "App";
        case GL_DEBUG_SOURCE_OTHER: return "Other";
        default: return "???";
    }
}

static char *
_gl_type (GLenum type)
{
    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR: return "Error";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Depricated";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "Undefined Behaviour";
        case GL_DEBUG_TYPE_PORTABILITY: return "Non-Portable";
        case GL_DEBUG_TYPE_PERFORMANCE: return "Performance";
        case GL_DEBUG_TYPE_MARKER: return "Stream Marker";
        case GL_DEBUG_TYPE_PUSH_GROUP: return "Group Push";
        case GL_DEBUG_TYPE_POP_GROUP: return "Group Pop";
        case GL_DEBUG_TYPE_OTHER: return "Other";
        default: return "???";
    }
}

static char *
_gl_severity (GLenum severity)
{
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH: return "High";
        case GL_DEBUG_SEVERITY_MEDIUM: return "Medium";
        case GL_DEBUG_SEVERITY_LOW: return "Low";
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "Notify";
        default: return "???";
    }
}

static void GLAPIENTRY
_gl_debug_msg_cb (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei len, const GLchar *message, const void *user_data)
{
    fprintf (stderr, YELLOW"OpenGL (%s:%s) %s> %s\n"NORMAL, _gl_source (source), _gl_type (type), _gl_severity (severity), message);
}

static void
_signal_handler (int signal)
{
    switch (signal)
    {
        case SIGINT:
            g__running = false;
        case SIGSEGV:
            FATAL ("SEGMENTATION FAULT");
            stack_trace ();
            g__running = false;
            break;
    }
}

static void
init (struct context *ctx)
{
    ASSERT (signal (SIGINT, _signal_handler) != SIG_ERR);
    ASSERT (signal (SIGSEGV, _signal_handler) != SIG_ERR);

    SDL_Init (SDL_INIT_EVERYTHING);

    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    ctx->window = SDL_CreateWindow ("Learn OpenGL",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    WINDOW_WIDTH_PX,
                                    WINDOW_HEIGHT_PX,
                                    SDL_WINDOW_OPENGL);
    ASSERT (ctx->window != NULL);

    ctx->gl = SDL_GL_CreateContext (ctx->window);
    ASSERT (ctx->gl != NULL);
    ctx->variation = -1;

    SDL_GL_SetSwapInterval (1); // vsync on

    ASSERT (glewInit () == GLEW_OK);

    glEnable (GL_DEBUG_OUTPUT);
    glDebugMessageCallback (_gl_debug_msg_cb, 0);

    printf ("Learning OpenGL! (version %s)\n", glGetString (GL_VERSION));
}

static void
cleanup (struct context *ctx)
{
    SDL_GL_DeleteContext (ctx->gl);
    SDL_DestroyWindow (ctx->window);
    SDL_Quit ();
}

static void
process_keydown (struct context *ctx, SDL_Keycode key)
{
    switch (key)
    {
        case SDLK_ESCAPE:
            g__running = false;
            break;
        case SDLK_1:
            ctx->state = STATE_RENDER_SQUARE;
            break;
        case SDLK_2:
            ctx->state = STATE_RENDER_TRIANGLE;
            break;
        case SDLK_3:
            ctx->state = STATE_RENDER_TEXTURE;
            ctx->variation++;
            if (ctx->variation >= 3)
            {
                ctx->variation = 0;
            }
            break;
        case SDLK_w:
            ctx->draw_wireframes = !ctx->draw_wireframes;
            break;
        case SDLK_r:
            ctx->rotate = !ctx->rotate;
            break;
        default:
            printf ("Unhandled key: %c (%d)\n", key, key);
            break;
    }
}

static void
handle_input (struct context *ctx)
{
    SDL_Event e;

    while (SDL_PollEvent (&e) != 0)
    {
        switch (e.type)
        {
            case SDL_QUIT:
                g__running = false;
                break;
            case SDL_KEYDOWN:
                process_keydown (ctx, e.key.keysym.sym);
                break;
        }
    }
}

/* Must be freed with free() */
static char *
read_file_to_buffer (char *file)
{
    char *buf = NULL;
    size_t len = 0;
    FILE *fp;

    printf ("Read File [%s]:\n", file);

    fp = fopen (file, "r");
    if (fp)
    {
        fseek (fp, 0, SEEK_END);
        len = ftell (fp);
        fseek (fp, 0, SEEK_SET);

        buf = malloc (len + 1);
        memset (buf, 0, len + 1);

        if (buf)
        {
            fread (buf, 1, len, fp);
            buf[len] = '\0';

            DUMP ("%s", buf);
        }
        else
        {
            LOG_ERROR ("Failed to allocate buffer (len=%zu)", len + 1);
        }

        fclose (fp);
    }
    else
    {
        LOG_ERROR ("Failed to read file: '%s'", file);
    }

    ASSERT (buf != NULL);

    return buf;
}

static unsigned int
shader_compile (unsigned int type, char *source)
{
    unsigned int id;
    char *message;
    int result;
    int len;

    GLCALL (id = glCreateShader (type));
    GLCALL (glShaderSource (id, 1/* count */, &source, NULL/* length */));
    GLCALL (glCompileShader (id));

    glGetShaderiv (id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        glGetShaderiv (id, GL_INFO_LOG_LENGTH, &len);
        message = alloca (len * sizeof (char));
        glGetShaderInfoLog (id, len, NULL, message);

        fprintf (stderr, YELLOW"Failed to compile %s shader:\n%s\n"NORMAL,
                (type == GL_VERTEX_SHADER ? "vertex" : "fragment"),
                message);

        glDeleteShader (id);
        id = 0;
    }

    ASSERT (id != 0);

    return id;
}

static unsigned int
shader_create (char *vertex_file, char *fragment_file)
{
    char *vertex_source = read_file_to_buffer (vertex_file);
    char *fragment_source = read_file_to_buffer (fragment_file);
    unsigned int program_id = 0;
    unsigned int vert_id = 0;
    unsigned int frag_id = 0;
    char *message;
    int result;
    int len;

    if (vertex_source && fragment_source)
    {
        vert_id = shader_compile (GL_VERTEX_SHADER, vertex_source);
        frag_id = shader_compile (GL_FRAGMENT_SHADER, fragment_source);
        program_id = glCreateProgram ();

        glAttachShader (program_id, vert_id);
        glAttachShader (program_id, frag_id);
        glLinkProgram (program_id);
        glValidateProgram (program_id);

        glGetProgramiv (program_id, GL_LINK_STATUS, &result);
        if (result == GL_FALSE)
        {
            glGetProgramiv (program_id, GL_INFO_LOG_LENGTH, &len);
            message = alloca (len * sizeof (char));
            glGetProgramInfoLog (program_id, len, NULL, message);

            fprintf (stderr, YELLOW"Failed to link shader program:\n%s\n"NORMAL, message);

            program_id = 0;
        }

        // TODO: not entirely sure if this is needed, but it doesn't
        //       seem to cause any problems??
        // glDetachShader (program_id, vert_id);
        // glDetachShader (program_id, frag_id);

        glDeleteShader (vert_id);
        glDeleteShader (frag_id);
    }

    if (vertex_source) free (vertex_source);
    if (fragment_source) free (fragment_source);

    ASSERT (program_id != 0);

    return program_id;
}

static unsigned int
texture_create (char *file)
{
    unsigned char *data;
    unsigned int id = 0;
    int bytes_per_pixel;
    int w;
    int h;

    stbi_set_flip_vertically_on_load (1);
    data = stbi_load (file, &w, &h, &bytes_per_pixel, 4);

    ASSERT (data != NULL);

    GLCALL (glGenTextures (1, &id));
    GLCALL (glBindTexture (GL_TEXTURE_2D, id));

    GLCALL (glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCALL (glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCALL (glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCALL (glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    GLCALL (glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
    GLCALL (glGenerateMipmap (GL_TEXTURE_2D));
    GLCALL (glBindTexture (GL_TEXTURE_2D, 0));

    stbi_image_free (data);
    ASSERT (id > 0);

    printf ("Load texture '%s' (id=%u w=%d h=%d bpp=%d)\n", file, id, w, h, bytes_per_pixel);

    return id;
}

static void
square_setup (struct render_target *r)
{
    float vertices[] = {
        0.5f,  0.5f,  0.0f,    // top right
        0.5f, -0.5f,  0.0f,    // bottom right
        -0.5f, -0.5f,  0.0f,    // bottom left
        -0.5f,  0.5f,  0.0f,    // top left
    }; 
    unsigned int indices[] = {
        0, 1, 3,    // first triangle
        1, 2, 3     // second triangle
    };

    unsigned int vao;
    GLCALL (glGenVertexArrays (1, &vao));
    GLCALL (glBindVertexArray (vao));

    unsigned int vbo;
    GLCALL (glGenBuffers (1, &vbo));
    GLCALL (glBindBuffer (GL_ARRAY_BUFFER, vbo));
    GLCALL (glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW));

    unsigned int ebo;
    GLCALL (glGenBuffers (1, &ebo));
    GLCALL (glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ebo));
    GLCALL (glBufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices, GL_STATIC_DRAW));

    GLCALL (glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof (float), (void *) 0));
    GLCALL (glEnableVertexAttribArray (0));

    unsigned int shader_id = shader_create ("square.vs", "square.fs");

    ASSERT (shader_id != 0);
    ASSERT (vao != 0);

    r->shader_id = shader_id;
    r->vao = vao;
}

static void
square_render (struct render_target *r)
{
    float ticks =  SDL_GetTicks () / 1000.0f;
    float green = (sin (ticks) / 2.0f) + 0.5f;
    int vert_colour_location;

    GLCALL (glUseProgram (r->shader_id));
    GLCALL (vert_colour_location = glGetUniformLocation (r->shader_id, "u_colour"));
    GLCALL (glUniform4f (vert_colour_location, 0.0f, green, 0.0f, 1.0f));

    GLCALL (glBindVertexArray (r->vao));
    GLCALL (glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
    GLCALL (glBindVertexArray (0));
}

static void
triangle_setup (struct render_target *r)
{
    float vertices[] = {
        // positions          // colours
        0.5f, -0.5f,  0.0f,   1.0f, 0.0f, 0.0f, // bottom right
       -0.5f, -0.5f,  0.0f,   0.0f, 1.0f, 0.0f, // bottom left
        0.0f,  0.5f,  0.0f,   0.0f, 0.0f, 1.0f  // top
    };

    unsigned int vao;
    GLCALL (glGenVertexArrays (1, &vao));
    GLCALL (glBindVertexArray (vao));

    unsigned int vbo;
    GLCALL (glGenBuffers (1, &vbo));
    GLCALL (glBindBuffer (GL_ARRAY_BUFFER, vbo));
    GLCALL (glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW));

    // position
    GLCALL (glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof (float), (void *) 0));
    GLCALL (glEnableVertexAttribArray (0));

    // colour
    GLCALL (glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof (float), (void *) (3 * sizeof (float))));
    GLCALL (glEnableVertexAttribArray (1));

    r->shader_id = shader_create ("tri.vs", "tri.fs");
    r->vao = vao;

    ASSERT (r->shader_id != 0);
    ASSERT (r->vao != 0);
}

static void
triangle_render (struct render_target *r)
{
    GLCALL (glUseProgram (r->shader_id));
    GLCALL (glBindVertexArray (r->vao));
    GLCALL (glDrawArrays (GL_TRIANGLES, 0, 3));
    GLCALL (glBindVertexArray (0));
}

static void
texture_setup (struct render_target *r)
{
    float vertices[] = {
        // positions          // colors           // texture coords
        0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
        0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // top left 
    };
    unsigned int indices[] = {  
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    unsigned int vao;
    GLCALL (glGenVertexArrays (1, &vao));
    GLCALL (glBindVertexArray (vao));

    unsigned int vbo;
    GLCALL (glGenBuffers (1, &vbo));
    GLCALL (glBindBuffer (GL_ARRAY_BUFFER, vbo));
    GLCALL (glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW));

    unsigned int ebo;
    GLCALL (glGenBuffers (1, &ebo));
    GLCALL (glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ebo));
    GLCALL (glBufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices, GL_STATIC_DRAW));

    // position (location=0)
    GLCALL (glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof (float), (void *) 0));
    GLCALL (glEnableVertexAttribArray (0));

    // colour (location=1)
    GLCALL (glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof (float), (void *) (3 * sizeof (float))));
    GLCALL (glEnableVertexAttribArray (1));

    // texture (location=2)
    GLCALL (glVertexAttribPointer (2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof (float), (void *) (6 * sizeof (float))));
    GLCALL (glEnableVertexAttribArray (2));

    r->texture_ids[0] = texture_create ("bricks.jpg");
    r->texture_ids[1] = texture_create ("face.png");
    r->shader_ids[0] = shader_create ("tex.vs", "tex.fs");
    r->shader_ids[1] = shader_create ("tex.vs", "tex-colour.fs");
    r->shader_ids[2] = shader_create ("tex.vs", "tex-face.fs");
    r->vao = vao;

    ASSERT (r->texture_ids[0] != 0);
    ASSERT (r->texture_ids[1] != 0);
    ASSERT (r->shader_ids[0] != 0);
    ASSERT (r->shader_ids[1] != 0);
    ASSERT (r->shader_ids[2] != 0);
    ASSERT (r->vao != 0);
}

static void
texture_render (struct context *ctx, struct render_target *rt)
{
    unsigned int shader_id = rt->shader_ids[ctx->variation];
    unsigned int tex_location;
    unsigned int xfrm_location;
    mat4_t xfrm;

    xfrm = m4_identity ();
    if (ctx->rotate)
    {
        float secs = SDL_GetTicks () / 1000.0;
        xfrm = m4_translation (vec3 (0.5, -0.5, 0.0));
        xfrm = m4_mul (xfrm, m4_rotation (secs, vec3 (0.0, 0.0, 1.0)));
    }

    GLCALL (glUseProgram (shader_id));
    GLCALL (xfrm_location = glGetUniformLocation (shader_id, "u_xfrm"));
    GLCALL (glUniformMatrix4fv (xfrm_location, 1, GL_FALSE, &xfrm.m[0][0]));
    GLCALL (tex_location = glGetUniformLocation (shader_id, "u_texture0"));
    GLCALL (glUniform1i (tex_location, 0));

    GLCALL (glActiveTexture (GL_TEXTURE0));
    GLCALL (glBindTexture (GL_TEXTURE_2D, rt->texture_ids[0]));
    if (ctx->variation == 2)
    {
        GLCALL (glUseProgram (shader_id));
        GLCALL (tex_location = glGetUniformLocation (shader_id, "u_texture1"));
        GLCALL (glUniform1i (tex_location, 1));

        GLCALL (glActiveTexture (GL_TEXTURE1));
        GLCALL (glBindTexture (GL_TEXTURE_2D, rt->texture_ids[1]));
    }
    GLCALL (glBindVertexArray (rt->vao));

    /* draw */
    GLCALL (glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));

    /* cleanup */
    GLCALL (glBindVertexArray (0));
    GLCALL (glBindTexture (GL_TEXTURE_2D, 0)); 
    GLCALL (glUseProgram (0));
}

int
main (int c, char **v)
{
    struct context ctx = {0};

    init (&ctx);

    GLCALL (glViewport (0, 0, WINDOW_WIDTH_PX, WINDOW_HEIGHT_PX));

    /**
     * +-------------------- +
     * |  VAO 1              |          +-------------------------+
     * |                     |          |  VBO 1                  |
     * | attribute pointer 0 -----------> pos[0] pos[1] .. pos[n] |
     * | attribute pointer 1 |          +---|------|--------------+
     * |                     |              +------+
     * |                     |               stride
     * | element buffer obj ------+
     * +---------------------+    |     +---------+
     *                            |     |  EBO 1  |
     *                            +-----> indices |
     *                                  +---------+
     */
    
    square_setup (&ctx.render_targets[STATE_RENDER_SQUARE]);
    triangle_setup (&ctx.render_targets[STATE_RENDER_TRIANGLE]);
    texture_setup (&ctx.render_targets[STATE_RENDER_TEXTURE]);

    g__running = true;
    while (g__running)
    {
        struct render_target *rt;

        handle_input (&ctx);

        GLCALL (glClearColor (0.2, 0.3, 0.3, 1.0));
        GLCALL (glClear (GL_COLOR_BUFFER_BIT));
        GLCALL (glPolygonMode (GL_FRONT_AND_BACK, ctx.draw_wireframes ? GL_LINE : GL_FILL));

        rt = &ctx.render_targets[ctx.state];

        switch (ctx.state)
        {
            case STATE_RENDER_SQUARE:
                square_render (rt);
                break;
            case STATE_RENDER_TRIANGLE:
                triangle_render (rt);
                break;
            case STATE_RENDER_TEXTURE:
                texture_render (&ctx, rt);
                break;
        }

        SDL_GL_SwapWindow (ctx.window);
        SDL_Delay (FRAME_TIME_MS);
    }

    cleanup (&ctx);

    return 0;
}

