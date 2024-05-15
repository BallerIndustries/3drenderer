#include "array.h"
#include "mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

mesh_t mesh = {
    .vertices = NULL,
    .faces = NULL,
    .rotation = { 0, 0, 0}
};

vec3_t cube_vertices[N_CUBE_VERTICES] = {
    { .x = -1, .y = -1, .z = -1 },  // 1
    { .x = -1, .y =  1, .z = -1 },  // 2
    { .x =  1, .y =  1, .z = -1 },  // 3
    { .x =  1, .y = -1, .z = -1 },  // 4
    { .x =  1, .y =  1, .z =  1 },  // 5
    { .x =  1, .y = -1, .z =  1 },  // 6
    { .x = -1, .y =  1, .z =  1 },  // 7
    { .x = -1, .y = -1, .z =  1 },  // 8
}; 

face_t cube_faces[N_CUBE_FACES] = {
    // front
    { .a = 1, .b = 2, .c = 3, .color = 0xFFFF0000 },
    { .a = 1, .b = 3, .c = 4, .color = 0xFFFF0000 },

    // right
    { .a = 4, .b = 5, .c = 6, .color = 0xFF00FF00 },
    { .a = 4, .b = 3, .c = 5, .color = 0xFF00FF00 },

    // back
    { .a = 6, .b = 5, .c = 7, .color = 0xFF0000FF },
    { .a = 6, .b = 7, .c = 8, .color = 0xFF0000FF },

    // left
    { .a = 8, .b = 7, .c = 2, .color = 0xFFFFFF00 },
    { .a = 8, .b = 2, .c = 1, .color = 0xFFFFFF00 },

    // top
    { .a = 2, .b = 7, .c = 5, .color = 0xFFFF00FF },
    { .a = 2, .b = 5, .c = 3, .color = 0xFFFF00FF },

    // bottom
    { .a = 6, .b = 8, .c = 1, .color = 0xFF00FFFF },
    { .a = 6, .b = 1, .c = 4, .color = 0xFF00FFFF },
};

void load_cube_mesh_data(void) {
    for (int i = 0; i < N_CUBE_VERTICES; i++) {
        vec3_t cube_vertex = cube_vertices[i];
        array_push(mesh.vertices, cube_vertex);
    }

    for (int i = 0; i < N_CUBE_FACES; i++) {
        face_t cube_face = cube_faces[i];
        array_push(mesh.faces, cube_face);
    }
}

void load_obj_file_data(void) {
    FILE* file = fopen("assets/f22.obj", "r");
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    if (file == NULL) {
        fprintf(stderr, "Failed to open file");
        exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &len, file)) != -1) {
        char str[20];
        sscanf(line, "%s", str);

        if (strcmp(str, "v") == 0) {
            float x;
            float y;
            float z;
            sscanf(line, "%*s %f %f %f", &x, &y, &z);

            vec3_t cube_vertex = { x, y, z };
            array_push(mesh.vertices, cube_vertex);
        }
        else if (strcmp(str, "f") == 0) {
            int a;
            int b;
            int c;
            sscanf(line, "%*s %i/%*i/%*i %i/%*i/%*i %i/%*i/%*i", &a, &b, &c);
            face_t cube_face = { a, b, c };
            array_push(mesh.faces, cube_face);
        }
    }

    fclose(file);

    if (line) {
        free(line);
    }
}